#include "RtspStreamFeeder.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

#include "Live555RtspServer.h"
#include "RtspSyntheticPattern.h"

namespace RtspServerApp {

namespace {

    constexpr std::size_t kTsPacketSize { 188U };

} // namespace

RtspStreamFeeder::RtspStreamFeeder() = default;

RtspStreamFeeder::~RtspStreamFeeder()
{
    stop();
}

bool RtspStreamFeeder::start(const FeederConfig& config, Video::Live555RtspServer* server)
{
    if (server == nullptr) {
        return false;
    }

    stop();

    m_config = config;
    m_server = server;
    m_bytesStreamed.store(0U, std::memory_order_relaxed);
    m_framesStreamed.store(0U, std::memory_order_relaxed);
    m_currentFps.store(0.0, std::memory_order_relaxed);
    m_running.store(true, std::memory_order_release);

    m_thread = std::thread(&RtspStreamFeeder::workerLoop, this);
    return true;
}

void RtspStreamFeeder::stop()
{
    if (m_running.exchange(false, std::memory_order_acq_rel)) {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

bool RtspStreamFeeder::isRunning() const noexcept
{
    return m_running.load(std::memory_order_acquire);
}

FeederMetrics RtspStreamFeeder::metrics() const noexcept
{
    FeederMetrics m {};
    m.bytesStreamed = m_bytesStreamed.load(std::memory_order_relaxed);
    m.framesStreamed = m_framesStreamed.load(std::memory_order_relaxed);
    m.currentFps = m_currentFps.load(std::memory_order_relaxed);
    return m;
}

const FeederConfig& RtspStreamFeeder::config() const noexcept
{
    return m_config;
}

void RtspStreamFeeder::workerLoop()
{
    switch (m_config.mode) {
    case FeederSourceMode::MpegTsFile:
        runMpegTsMode();
        break;
    case FeederSourceMode::H264File:
        runH264Mode();
        break;
    case FeederSourceMode::SyntheticPattern:
    default:
        runSyntheticMode();
        break;
    }

    m_running.store(false, std::memory_order_release);
}

void RtspStreamFeeder::runSyntheticMode()
{
    const std::uint32_t fps = (m_config.targetFps > 0U) ? m_config.targetFps : 30U;
    const auto interval = std::chrono::microseconds(1000000U / fps);

    // Initial parameter set injection (raw without start code for Live555 SDP)
    m_server->setH264SpsPps(kSyntheticSpsNoStartCode, kSyntheticPpsNoStartCode);

    // Parse NAL unit boundaries in pre-encoded colorbars buffer
    struct NaluSpan {
        std::size_t offset { 0U };
        std::size_t size { 0U };
        bool isSlice { false };
    };

    std::vector<NaluSpan> nalus;
    const std::size_t dataSize = kSyntheticColorbars.size();
    std::size_t i { 0U };
    std::size_t currentStart { 0U };
    bool inNalu { false };

    auto checkSlice = [](std::size_t start) -> bool {
        std::size_t prefix = 3U;
        if (start + 4U <= kSyntheticColorbars.size() && kSyntheticColorbars[start] == 0x00
            && kSyntheticColorbars[start + 1U] == 0x00 && kSyntheticColorbars[start + 2U] == 0x00
            && kSyntheticColorbars[start + 3U] == 0x01) {
            prefix = 4U;
        }
        if (start + prefix < kSyntheticColorbars.size()) {
            const std::uint8_t nalType = kSyntheticColorbars[start + prefix] & 0x1FU;
            return (nalType == 1U || nalType == 5U);
        }
        return false;
    };

    while (i + 3U <= dataSize) {
        bool isStartCode3 = (kSyntheticColorbars[i] == 0x00 && kSyntheticColorbars[i + 1U] == 0x00
            && kSyntheticColorbars[i + 2U] == 0x01);
        bool isStartCode4 = (i + 4U <= dataSize && kSyntheticColorbars[i] == 0x00 && kSyntheticColorbars[i + 1U] == 0x00
            && kSyntheticColorbars[i + 2U] == 0x00 && kSyntheticColorbars[i + 3U] == 0x01);

        if (isStartCode3 || isStartCode4) {
            if (inNalu && i > currentStart) {
                nalus.push_back({ currentStart, i - currentStart, checkSlice(currentStart) });
            }
            currentStart = i;
            inNalu = true;
            i += isStartCode4 ? 4U : 3U;
        } else {
            i++;
        }
    }

    if (inNalu && dataSize > currentStart) {
        nalus.push_back({ currentStart, dataSize - currentStart, checkSlice(currentStart) });
    }

    if (nalus.empty()) {
        return;
    }

    auto lastFpsUpdate = std::chrono::steady_clock::now();
    std::uint64_t framesSinceUpdate { 0U };

    do {
        for (const auto& span : nalus) {
            if (!m_running.load(std::memory_order_acquire)) {
                break;
            }

            const auto frameStart = std::chrono::steady_clock::now();
            const bool ok = m_server->pushH264Nalu(kSyntheticColorbars.data() + span.offset, span.size);
            if (ok) {
                m_bytesStreamed.fetch_add(span.size, std::memory_order_relaxed);
            }

            // Only pace on actual video frame slices (IDR / P-frames)
            if (span.isSlice) {
                m_framesStreamed.fetch_add(1U, std::memory_order_relaxed);
                framesSinceUpdate++;

                const auto now = std::chrono::steady_clock::now();
                const auto fpsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdate);
                if (fpsDuration.count() >= 1000) {
                    const double calculatedFps
                        = static_cast<double>(framesSinceUpdate) * 1000.0 / static_cast<double>(fpsDuration.count());
                    m_currentFps.store(calculatedFps, std::memory_order_relaxed);
                    lastFpsUpdate = now;
                    framesSinceUpdate = 0U;
                }

                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - frameStart);
                if (elapsed < interval) {
                    std::this_thread::sleep_for(interval - elapsed);
                }
            }
        }
    } while (m_running.load(std::memory_order_acquire));
}

void RtspStreamFeeder::runMpegTsMode()
{
    if (m_config.filePath.empty()) {
        return;
    }

    std::ifstream file(m_config.filePath, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    const std::size_t chunkSize = (m_config.tsChunkSize >= kTsPacketSize)
        ? (m_config.tsChunkSize - (m_config.tsChunkSize % kTsPacketSize))
        : (7U * kTsPacketSize);

    std::vector<std::uint8_t> buffer(chunkSize);
    const std::uint32_t fps = (m_config.targetFps > 0U) ? m_config.targetFps : 30U;
    const auto interval = std::chrono::microseconds(1000000U / fps);

    auto lastFpsUpdate = std::chrono::steady_clock::now();
    std::uint64_t framesSinceUpdate { 0U };

    while (m_running.load(std::memory_order_acquire)) {
        const auto frameStart = std::chrono::steady_clock::now();

        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const std::size_t bytesRead = static_cast<std::size_t>(file.gcount());

        if (bytesRead == 0U) {
            if (m_config.loopPlayback) {
                file.clear();
                file.seekg(0, std::ios::beg);
                continue;
            }
            break;
        }

        const std::size_t alignedBytes = bytesRead - (bytesRead % kTsPacketSize);
        if (alignedBytes > 0U) {
            const bool ok = m_server->pushTsPackets(buffer.data(), alignedBytes);
            if (ok) {
                m_bytesStreamed.fetch_add(alignedBytes, std::memory_order_relaxed);
                m_framesStreamed.fetch_add(1U, std::memory_order_relaxed);
                framesSinceUpdate++;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const auto fpsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdate);
        if (fpsDuration.count() >= 1000) {
            const double calculatedFps
                = static_cast<double>(framesSinceUpdate) * 1000.0 / static_cast<double>(fpsDuration.count());
            m_currentFps.store(calculatedFps, std::memory_order_relaxed);
            lastFpsUpdate = now;
            framesSinceUpdate = 0U;
        }

        const auto elapsed
            = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frameStart);
        if (elapsed < interval) {
            std::this_thread::sleep_for(interval - elapsed);
        }
    }
}

void RtspStreamFeeder::runH264Mode()
{
    if (m_config.filePath.empty()) {
        return;
    }

    std::ifstream file(m_config.filePath, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    // Read full file into memory for parsing Annex-B start codes
    file.seekg(0, std::ios::end);
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (fileSize == 0U) {
        return;
    }

    std::vector<std::uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));

    // Find all Annex-B NAL unit boundaries (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01)
    struct NaluSpan {
        std::size_t offset { 0U };
        std::size_t size { 0U };
    };

    std::vector<NaluSpan> nalus;
    std::size_t i { 0U };
    std::size_t currentStart { 0U };
    bool inNalu { false };

    while (i + 3U <= fileSize) {
        bool isStartCode3 = (data[i] == 0x00 && data[i + 1U] == 0x00 && data[i + 2U] == 0x01);
        bool isStartCode4 = (i + 4U <= fileSize && data[i] == 0x00 && data[i + 1U] == 0x00 && data[i + 2U] == 0x00
            && data[i + 3U] == 0x01);

        if (isStartCode3 || isStartCode4) {
            if (inNalu && i > currentStart) {
                nalus.push_back({ currentStart, i - currentStart });
            }
            currentStart = i;
            inNalu = true;
            i += isStartCode4 ? 4U : 3U;
        } else {
            i++;
        }
    }

    if (inNalu && fileSize > currentStart) {
        nalus.push_back({ currentStart, fileSize - currentStart });
    }

    if (nalus.empty()) {
        return;
    }

    const std::uint32_t fps = (m_config.targetFps > 0U) ? m_config.targetFps : 30U;
    const auto interval = std::chrono::microseconds(1000000U / fps);

    auto lastFpsUpdate = std::chrono::steady_clock::now();
    std::uint64_t framesSinceUpdate { 0U };

    do {
        for (const auto& span : nalus) {
            if (!m_running.load(std::memory_order_acquire)) {
                break;
            }

            const auto frameStart = std::chrono::steady_clock::now();
            const bool ok = m_server->pushH264Nalu(data.data() + span.offset, span.size);
            if (ok) {
                m_bytesStreamed.fetch_add(span.size, std::memory_order_relaxed);
                m_framesStreamed.fetch_add(1U, std::memory_order_relaxed);
                framesSinceUpdate++;
            }

            const auto now = std::chrono::steady_clock::now();
            const auto fpsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdate);
            if (fpsDuration.count() >= 1000) {
                const double calculatedFps
                    = static_cast<double>(framesSinceUpdate) * 1000.0 / static_cast<double>(fpsDuration.count());
                m_currentFps.store(calculatedFps, std::memory_order_relaxed);
                lastFpsUpdate = now;
                framesSinceUpdate = 0U;
            }

            const auto elapsed
                = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frameStart);
            if (elapsed < interval) {
                std::this_thread::sleep_for(interval - elapsed);
            }
        }
    } while (m_config.loopPlayback && m_running.load(std::memory_order_acquire));
}

} // namespace RtspServerApp
