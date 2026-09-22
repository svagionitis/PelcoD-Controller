#include "ViscaBusScanner.h"
#include "ViscaBuilder.h"
#include "ViscaParser.h"
#include "ViscaRxAccumulator.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Visca {

ViscaBusScanner::ViscaBusScanner(std::shared_ptr<::Transport::ITransport> transport)
    : m_transport(std::move(transport))
{
}

std::vector<DiscoveredCamera> ViscaBusScanner::scanBus(ProgressCallback onProgress, CameraDiscoveredCallback onFound)
{
    std::vector<DiscoveredCamera> discovered;

    if (!m_transport || !m_transport->isOpen()) {
        return discovered;
    }

    std::mutex scanMutex;
    std::condition_variable cv;
    std::vector<ViscaFrame> rxFrames;

    ViscaRxAccumulator accumulator;
    accumulator.setFrameCallback([&](const ViscaFrame& frame) {
        std::lock_guard<std::mutex> lock(scanMutex);
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
        std::lock_guard<std::mutex> lock(scanMutex);
        rxFrames.clear();
    }
    const ViscaFrame addrSet = ViscaBuilder::addressSet();
    if (!m_transport->sendData(addrSet.bytes())) {
        return discovered;
    }

    uint8_t detectedCameras = 0;
    const auto addrResp = waitForFrame(std::chrono::milliseconds(1000));
    if (addrResp.has_value()) {
        const auto parsed = ViscaParser::parseAddressSet(*addrResp);
        if (parsed.has_value()) {
            detectedCameras = parsed->cameraCount;
        }
    }

    // Fallback: If address set didn't report count, test at least camera 1
    if (detectedCameras == 0) {
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
        {
            std::lock_guard<std::mutex> lock(scanMutex);
            rxFrames.clear();
        }

        const ViscaFrame verInq = ViscaBuilder::versionInquiry(addr);
        if (!m_transport->sendData(verInq.bytes())) {
            continue;
        }

        const auto verResp = waitForFrame(std::chrono::milliseconds(1000));
        if (verResp.has_value()) {
            const auto verInfo = ViscaParser::parseVersionInquiry(*verResp);
            if (verInfo.has_value()) {
                DiscoveredCamera cam;
                cam.address = addr;
                cam.vendorId = verInfo->vendorId;
                cam.modelId = verInfo->modelId;
                cam.romVersion = verInfo->romVersion;
                cam.maxSockets = verInfo->maxSockets;

                discovered.push_back(cam);
                if (onFound) {
                    onFound(cam);
                }
            }
        }

        if (onProgress) {
            onProgress(addr, totalSteps);
        }
    }

    return discovered;
}

} // namespace Visca
