#include "ViscaBusScanner.h"
#include "ViscaBuilder.h"
#include "ViscaParser.h"
#include "ViscaRxAccumulator.h"

#include <glog/logging.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Visca {

using namespace std::string_view_literals;

ViscaBusScanner::ViscaBusScanner(std::shared_ptr<::Transport::ITransport> transport)
    : m_transport(std::move(transport))
{
}

std::vector<DiscoveredCamera> ViscaBusScanner::scanBus(
    ProgressCallback onProgress,
    CameraDiscoveredCallback onFound,
    ErrorCallback onError,
    CancellationPredicate isCancelled)
{
    std::vector<DiscoveredCamera> discovered {};

    LOG(INFO) << "ViscaBusScanner: Starting daisy-chain bus scan...";

    if (isCancelled && isCancelled()) {
        constexpr auto cancelMsg = "Scan cancelled before start."sv;
        LOG(INFO) << "ViscaBusScanner: " << cancelMsg;
        if (onError) {
            onError(ScanError::Cancelled, cancelMsg);
        }
        return discovered;
    }

    if (!m_transport || !m_transport->isOpen()) {
        constexpr auto err = "Transport interface is not configured or not open."sv;
        LOG(ERROR) << "ViscaBusScanner: " << err;
        if (onError) {
            onError(ScanError::TransportNotOpen, err);
        }
        return discovered;
    }

    std::mutex scanMutex {};
    std::condition_variable cv {};
    std::vector<ViscaFrame> rxFrames {};

    ViscaRxAccumulator accumulator {};
    accumulator.setFrameCallback([&](const ViscaFrame& frame) {
        std::scoped_lock lock(scanMutex);
        rxFrames.push_back(frame);
        cv.notify_all();
    });

    // Wire transport data into local accumulator
    m_transport->setDataCallback([&](const std::vector<uint8_t>& data) { accumulator.addData(data); });

    auto waitForFrame = [&](std::chrono::milliseconds timeout) -> std::optional<ViscaFrame> {
        std::unique_lock<std::mutex> lock(scanMutex);
        if (!rxFrames.empty()) {
            ViscaFrame f = std::move(rxFrames.front());
            rxFrames.erase(rxFrames.begin());
            return f;
        }
        if (cv.wait_for(lock, timeout, [&]() { return !rxFrames.empty(); })) {
            ViscaFrame f = std::move(rxFrames.front());
            rxFrames.erase(rxFrames.begin());
            return f;
        }
        return std::nullopt;
    };

    // Step 1: Send AddressSet (88 30 01 FF)
    {
        std::scoped_lock lock(scanMutex);
        rxFrames.clear();
    }

    const ViscaFrame addrSet = ViscaBuilder::addressSet();
    LOG(INFO) << "ViscaBusScanner: Broadcasting AddressSet (88 30 01 FF)...";
    if (!m_transport->sendData(addrSet.bytes())) {
        constexpr auto err = "Failed to transmit AddressSet broadcast frame."sv;
        LOG(ERROR) << "ViscaBusScanner: " << err;
        if (onError) {
            onError(ScanError::AddressSetSendFailed, err);
        }
        return discovered;
    }

    uint8_t detectedCameras = 0;
    const auto addrResp = waitForFrame(std::chrono::milliseconds(1000));
    if (addrResp.has_value()) {
        const auto parsed = ViscaParser::parseAddressSet(*addrResp);
        if (parsed.has_value()) {
            detectedCameras = parsed->cameraCount;
            LOG(INFO) << "ViscaBusScanner: AddressSet acknowledged. Detected "
                      << static_cast<int>(detectedCameras) << " camera(s) on daisy-chain.";
        }
    }

    // Fallback: If address set didn't report count, test at least camera 1
    if (detectedCameras == 0) {
        constexpr auto timeoutMsg = "AddressSet response timeout; falling back to address 1."sv;
        LOG(WARNING) << "ViscaBusScanner: Timeout or unparsed response for AddressSet; falling back to probing address 1.";
        if (onError) {
            onError(ScanError::AddressSetTimeout, timeoutMsg);
        }
        detectedCameras = 1;
    }
    if (detectedCameras > kMaxCamerasOnBus) {
        detectedCameras = kMaxCamerasOnBus;
    }

    const size_t totalSteps = detectedCameras;
    if (onProgress) {
        onProgress(0, totalSteps);
    }

    // Step 2: Query CAM_VersionInq for each detected camera address
    for (uint8_t addr = 1; addr <= detectedCameras; ++addr) {
        if (isCancelled && isCancelled()) {
            const std::string cancelMsg = "Scan cancelled by user at address " + std::to_string(addr);
            LOG(INFO) << "ViscaBusScanner: " << cancelMsg;
            if (onError) {
                onError(ScanError::Cancelled, cancelMsg);
            }
            break;
        }

        {
            std::scoped_lock lock(scanMutex);
            rxFrames.clear();
        }

        const ViscaFrame verInq = ViscaBuilder::versionInquiry(addr);
        if (!m_transport->sendData(verInq.bytes())) {
            const std::string err = "Failed to transmit CAM_VersionInq to address " + std::to_string(addr);
            LOG(WARNING) << "ViscaBusScanner: " << err;
            if (onError) {
                onError(ScanError::VersionInquirySendFailed, err);
            }
            if (onProgress) {
                onProgress(addr, totalSteps);
            }
            continue;
        }

        const auto verResp = waitForFrame(std::chrono::milliseconds(1000));
        if (verResp.has_value()) {
            const auto verInfo = ViscaParser::parseVersionInquiry(*verResp);
            if (verInfo.has_value()) {
                DiscoveredCamera cam {};
                cam.address = addr;
                cam.vendorId = verInfo->vendorId;
                cam.modelId = verInfo->modelId;
                cam.romVersion = verInfo->romVersion;
                cam.maxSockets = verInfo->maxSockets;

                LOG(INFO) << "ViscaBusScanner: Discovered camera at address "
                          << static_cast<int>(addr) << " (Vendor 0x" << std::hex << cam.vendorId
                          << ", Model 0x" << cam.modelId << ", ROM 0x" << cam.romVersion
                          << ", MaxSockets " << std::dec << static_cast<int>(cam.maxSockets) << ")";

                discovered.push_back(cam);
                if (onFound) {
                    onFound(cam);
                }
            } else {
                LOG(WARNING) << "ViscaBusScanner: Received unparsed version response from address " << static_cast<int>(addr);
            }
        } else {
            const std::string err = "Timeout waiting for CAM_VersionInq response from address " + std::to_string(addr);
            LOG(WARNING) << "ViscaBusScanner: " << err;
            if (onError) {
                onError(ScanError::VersionInquiryTimeout, err);
            }
        }

        if (onProgress) {
            onProgress(addr, totalSteps);
        }
    }

    LOG(INFO) << "ViscaBusScanner: Daisy-chain scan complete. Discovered "
              << discovered.size() << " camera(s).";

    return discovered;
}

} // namespace Visca
