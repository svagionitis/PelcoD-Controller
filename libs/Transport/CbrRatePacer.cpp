#include "CbrRatePacer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace Transport {

namespace {

    constexpr std::size_t kPacketSizeBytes = 188U;
    constexpr std::uint64_t kBitsPerPacket = 188ULL * 8ULL; // 1504 bits
    constexpr std::uint64_t kPcrTicksPerSec = 27000000ULL; // 27 MHz

    constexpr std::array<std::uint8_t, CbrRatePacer::kTsPacketSize> createNullPacket() noexcept
    {
        std::array<std::uint8_t, CbrRatePacer::kTsPacketSize> pkt { };
        pkt[0] = 0x47U; // Sync byte
        pkt[1] = 0x1FU; // PID high 5 bits = 0x1F (PID 0x1FFF)
        pkt[2] = 0xFFU; // PID low 8 bits = 0xFF
        pkt[3] = 0x10U; // AFC = 01 (payload only), CC = 0x00
        for (std::size_t i = 4U; i < CbrRatePacer::kTsPacketSize; ++i) {
            pkt[i] = 0xFFU;
        }
        return pkt;
    }

    const std::array<std::uint8_t, CbrRatePacer::kTsPacketSize> s_nullPacket = createNullPacket();

} // namespace

const std::array<std::uint8_t, CbrRatePacer::kTsPacketSize>& CbrRatePacer::nullPacket() noexcept
{
    return s_nullPacket;
}

bool CbrRatePacer::hasPcr(const std::uint8_t* packet) noexcept
{
    if (packet == nullptr) {
        return false;
    }
    const bool isSync = (packet[0] == 0x47U);
    const bool hasAf = ((packet[3] & 0x20U) != 0U);
    if (!isSync || !hasAf) {
        return false;
    }
    const std::uint8_t afLen = packet[4];
    if (afLen < 7U) {
        return false;
    }
    return ((packet[5] & 0x10U) != 0U);
}

CbrRatePacer::CbrRatePacer(const CbrPacerConfig& config, EmitPacketCallback callback)
    : m_config(config)
    , m_callback(std::move(callback))
    , m_queueCapacity(std::max<std::size_t>(config.maxQueuePackets, 1U))
{
    m_packetBuffer.resize(m_queueCapacity * kPacketSizeBytes, 0x00U);
    m_startTime = std::chrono::steady_clock::now();
    m_lastBitrateCalc = m_startTime;

    if (m_config.autoStartWorker) {
        static_cast<void>(startWorker());
    }
}

CbrRatePacer::~CbrRatePacer()
{
    stopWorker();
}

bool CbrRatePacer::startWorker()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_running.load(std::memory_order_acquire)) {
        return true;
    }

    m_running.store(true, std::memory_order_release);
    m_startTime = std::chrono::steady_clock::now();
    m_lastBitrateCalc = m_startTime;
    m_packetsScheduled = 0U;
    m_bytesSinceLastCalc = 0U;

    m_workerThread = std::thread(&CbrRatePacer::workerLoop, this);
    return true;
}

void CbrRatePacer::stopWorker()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_running.load(std::memory_order_acquire)) {
            return;
        }
        m_running.store(false, std::memory_order_release);
        m_cv.notify_all();
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

bool CbrRatePacer::isRunning() const noexcept
{
    return m_running.load(std::memory_order_acquire);
}

std::size_t CbrRatePacer::submitTsPackets(const std::uint8_t* packets, std::size_t count)
{
    if (packets == nullptr || count == 0U) {
        return 0U;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    const std::size_t available = m_queueCapacity - m_queueCount;
    const std::size_t toAccept = std::min(count, available);
    const std::size_t toDrop = count - toAccept;

    for (std::size_t i = 0U; i < toAccept; ++i) {
        const std::size_t offset = m_queueTail * kPacketSizeBytes;
        std::copy_n(packets + (i * kPacketSizeBytes), kPacketSizeBytes,
            m_packetBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
        m_queueTail = (m_queueTail + 1U) % m_queueCapacity;
        ++m_queueCount;
    }

    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.payloadPacketsIn += count;
        m_stats.droppedPackets += toDrop;
        m_stats.currentQueueSize = m_queueCount;
    }

    if (toAccept > 0U) {
        m_cv.notify_one();
    }

    return toAccept;
}

void CbrRatePacer::flush()
{
    std::vector<std::uint8_t> toEmit;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queueCount == 0U) {
            return;
        }

        toEmit.resize(m_queueCount * kPacketSizeBytes);
        for (std::size_t i = 0U; i < m_queueCount; ++i) {
            const std::size_t srcOffset = m_queueHead * kPacketSizeBytes;
            const std::size_t dstOffset = i * kPacketSizeBytes;
            std::copy_n(m_packetBuffer.begin() + static_cast<std::ptrdiff_t>(srcOffset), kPacketSizeBytes,
                toEmit.begin() + static_cast<std::ptrdiff_t>(dstOffset));
            m_queueHead = (m_queueHead + 1U) % m_queueCapacity;
        }
        m_queueCount = 0U;

        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.currentQueueSize = 0U;
        }
    }

    const std::size_t count = toEmit.size() / kPacketSizeBytes;
    for (std::size_t i = 0U; i < count; ++i) {
        emitPacket(toEmit.data() + (i * kPacketSizeBytes), kPacketSizeBytes);
    }
}

void CbrRatePacer::reset() noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queueHead = 0U;
    m_queueTail = 0U;
    m_queueCount = 0U;
    m_startTime = std::chrono::steady_clock::now();
    m_lastBitrateCalc = m_startTime;
    m_virtualElapsed = std::chrono::nanoseconds(0);
    m_packetsScheduled = 0U;
    m_bytesSinceLastCalc = 0U;

    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats = CbrPacerStats { };
    }
}

bool CbrRatePacer::setConfig(const CbrPacerConfig& config)
{
    if (config.targetBitrateBps == 0U) {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    if (config.targetBitrateBps != m_config.targetBitrateBps) {
        m_startTime = std::chrono::steady_clock::now();
        m_virtualElapsed = std::chrono::nanoseconds(0);
        m_packetsScheduled = 0U;
    }
    m_config = config;

    const std::size_t newCap = std::max<std::size_t>(config.maxQueuePackets, 1U);
    if (newCap != m_queueCapacity) {
        std::vector<std::uint8_t> newBuf(newCap * kPacketSizeBytes, 0x00U);
        const std::size_t toKeep = std::min(m_queueCount, newCap);
        for (std::size_t i = 0U; i < toKeep; ++i) {
            const std::size_t src = ((m_queueHead + i) % m_queueCapacity) * kPacketSizeBytes;
            const std::size_t dst = i * kPacketSizeBytes;
            std::copy_n(m_packetBuffer.begin() + static_cast<std::ptrdiff_t>(src), kPacketSizeBytes,
                newBuf.begin() + static_cast<std::ptrdiff_t>(dst));
        }
        m_packetBuffer = std::move(newBuf);
        m_queueCapacity = newCap;
        m_queueHead = 0U;
        m_queueTail = toKeep % newCap;
        m_queueCount = toKeep;
    }

    return true;
}

CbrPacerConfig CbrRatePacer::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

CbrPacerStats CbrRatePacer::stats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

double CbrRatePacer::measuredBitrate() const noexcept
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats.measuredBitrateBps;
}

std::size_t CbrRatePacer::nullPacketsInjected() const noexcept
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return static_cast<std::size_t>(m_stats.nullPacketsInjected);
}

std::size_t CbrRatePacer::step(std::chrono::nanoseconds stepDuration)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_virtualElapsed += stepDuration;

    const auto elapsedNs = static_cast<std::uint64_t>(m_virtualElapsed.count());
    const auto sec = elapsedNs / 1000000000ULL;
    const auto remNs = elapsedNs % 1000000000ULL;
    const auto rateBps = static_cast<std::uint64_t>(m_config.targetBitrateBps);

    const std::uint64_t targetBits = (sec * rateBps) + ((remNs * rateBps) / 1000000000ULL);
    const std::uint64_t targetPackets = targetBits / kBitsPerPacket;

    if (targetPackets <= m_packetsScheduled) {
        return 0U;
    }

    const std::size_t packetsToEmit = static_cast<std::size_t>(targetPackets - m_packetsScheduled);
    std::size_t emittedCount = 0U;

    std::array<std::uint8_t, kPacketSizeBytes> packetBuf { };

    for (std::size_t i = 0U; i < packetsToEmit; ++i) {
        bool isPayload = false;
        if (m_queueCount > 0U) {
            const std::size_t offset = m_queueHead * kPacketSizeBytes;
            std::copy_n(
                m_packetBuffer.begin() + static_cast<std::ptrdiff_t>(offset), kPacketSizeBytes, packetBuf.begin());
            m_queueHead = (m_queueHead + 1U) % m_queueCapacity;
            --m_queueCount;
            isPayload = true;
        } else {
            packetBuf = s_nullPacket;
        }

        const std::uint64_t departurePacketIdx = m_packetsScheduled + i;
        const std::uint64_t departureTicks = (departurePacketIdx * kBitsPerPacket * kPcrTicksPerSec) / rateBps;

        if (isPayload && m_config.enablePcrRestamping && hasPcr(packetBuf.data())) {
            restampPcr(packetBuf.data(), departureTicks);
        }

        lock.unlock();
        emitPacket(packetBuf.data(), kPacketSizeBytes);
        lock.lock();

        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            if (isPayload) {
                ++m_stats.payloadPacketsOut;
            } else {
                ++m_stats.nullPacketsInjected;
            }
            m_stats.currentQueueSize = m_queueCount;
        }

        ++emittedCount;
    }

    m_packetsScheduled += emittedCount;
    return emittedCount;
}

void CbrRatePacer::workerLoop()
{
    while (m_running.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(m_mutex);

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_startTime).count());

        const auto sec = elapsedNs / 1000000000ULL;
        const auto remNs = elapsedNs % 1000000000ULL;
        const auto rateBps = static_cast<std::uint64_t>(m_config.targetBitrateBps);

        const std::uint64_t targetBits = (sec * rateBps) + ((remNs * rateBps) / 1000000000ULL);
        const std::uint64_t targetPackets = targetBits / kBitsPerPacket;

        if (targetPackets > m_packetsScheduled) {
            std::size_t burstLimit = std::min<std::size_t>(
                static_cast<std::size_t>(targetPackets - m_packetsScheduled), m_config.maxBurstPackets);

            std::array<std::uint8_t, kPacketSizeBytes> packetBuf { };

            while (burstLimit > 0U && m_running.load(std::memory_order_acquire)) {
                bool isPayload = false;
                if (m_queueCount > 0U) {
                    const std::size_t offset = m_queueHead * kPacketSizeBytes;
                    std::copy_n(m_packetBuffer.begin() + static_cast<std::ptrdiff_t>(offset), kPacketSizeBytes,
                        packetBuf.begin());
                    m_queueHead = (m_queueHead + 1U) % m_queueCapacity;
                    --m_queueCount;
                    isPayload = true;
                } else {
                    packetBuf = s_nullPacket;
                }

                const std::uint64_t departureTicks = (m_packetsScheduled * kBitsPerPacket * kPcrTicksPerSec) / rateBps;
                if (isPayload && m_config.enablePcrRestamping && hasPcr(packetBuf.data())) {
                    restampPcr(packetBuf.data(), departureTicks);
                }

                ++m_packetsScheduled;
                --burstLimit;

                lock.unlock();
                emitPacket(packetBuf.data(), kPacketSizeBytes);
                lock.lock();

                {
                    std::lock_guard<std::mutex> statsLock(m_statsMutex);
                    if (isPayload) {
                        ++m_stats.payloadPacketsOut;
                    } else {
                        ++m_stats.nullPacketsInjected;
                    }
                    m_stats.currentQueueSize = m_queueCount;
                }
            }
        }

        // Calculate sleep duration until next packet slot
        const std::uint64_t nextPacketIdx = m_packetsScheduled + 1U;
        const std::uint64_t nextTargetBits = nextPacketIdx * kBitsPerPacket;
        const auto nextSec = nextTargetBits / rateBps;
        const auto nextRemBits = nextTargetBits % rateBps;
        const auto nextRemNs = (nextRemBits * 1000000000ULL) / rateBps;
        const auto nextScheduledNs = (nextSec * 1000000000ULL) + nextRemNs;

        const auto scheduledTime = m_startTime + std::chrono::nanoseconds(nextScheduledNs);
        const auto curTime = std::chrono::steady_clock::now();

        if (scheduledTime > curTime) {
            const auto sleepDuration = scheduledTime - curTime;
            m_cv.wait_for(lock, sleepDuration, [this] { return !m_running.load(std::memory_order_acquire); });
        }
    }
}

void CbrRatePacer::emitPacket(const std::uint8_t* packet, std::size_t size)
{
    if (m_callback) {
        m_callback(packet, size);
    }

    // Telemetry update
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_bytesSinceLastCalc += size;

    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastBitrateCalc).count();
    if (diff >= 500) {
        m_stats.measuredBitrateBps
            = (static_cast<double>(m_bytesSinceLastCalc * 8ULL) * 1000.0) / static_cast<double>(diff);
        m_bytesSinceLastCalc = 0U;
        m_lastBitrateCalc = now;
    }
}

void CbrRatePacer::restampPcr(std::uint8_t* packet, std::uint64_t currentPcrTicks)
{
    const std::uint64_t pcrBase = (currentPcrTicks / 300ULL) & 0x1FFFFFFFFULL;
    const auto pcrExt = static_cast<std::uint16_t>(currentPcrTicks % 300ULL);

    constexpr std::size_t afOffset = 6U;
    packet[afOffset] = static_cast<std::uint8_t>((pcrBase >> 25U) & 0xFFU);
    packet[afOffset + 1U] = static_cast<std::uint8_t>((pcrBase >> 17U) & 0xFFU);
    packet[afOffset + 2U] = static_cast<std::uint8_t>((pcrBase >> 9U) & 0xFFU);
    packet[afOffset + 3U] = static_cast<std::uint8_t>((pcrBase >> 1U) & 0xFFU);
    packet[afOffset + 4U] = static_cast<std::uint8_t>(((pcrBase & 1U) << 7U) | 0x7EU | ((pcrExt >> 8U) & 1U));
    packet[afOffset + 5U] = static_cast<std::uint8_t>(pcrExt & 0xFFU);
}

} // namespace Transport
