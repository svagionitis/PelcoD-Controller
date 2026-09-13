/// @file BusScanner.cpp
/// @brief Implementation of RS-485 bus address auto-discovery scanner.

#include "BusScanner.h"

#include <algorithm>
#include <utility>

namespace PelcoD {

BusScanner::BusScanner(std::shared_ptr<ITransport> transport)
    : m_transport(std::move(transport))
{
}

BusScanner::~BusScanner()
{
    stopScan();
}

void BusScanner::setTransport(std::shared_ptr<ITransport> transport)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != ScanState::Idle) {
        return;
    }
    m_transport = std::move(transport);
}

std::shared_ptr<ITransport> BusScanner::getTransport() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transport;
}

bool BusScanner::startScan(const ScanConfig& config)
{
    if (config.startAddress == 0U || config.startAddress > 254U ||
        config.endAddress == 0U || config.endAddress > 254U ||
        config.startAddress > config.endAddress) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != ScanState::Idle || !m_transport) {
        return false;
    }

    if (!m_transport->isOpen()) {
        if (!m_transport->open()) {
            return false;
        }
    }

    // Register callback for incoming responses
    m_transport->setDataCallback([this](const std::vector<std::uint8_t>& data) {
        onDataReceived(data);
    });

    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_stopRequested.store(false);
    m_pauseRequested.store(false);
    m_state.store(ScanState::Scanning);

    const auto stateCb = m_stateCb;
    if (stateCb) {
        stateCb(ScanState::Scanning);
    }

    m_worker = std::thread(&BusScanner::scanWorker, this, config);
    return true;
}

void BusScanner::stopScan()
{
    m_stopRequested.store(true);
    m_pauseRequested.store(false);
    m_rxCv.notify_all();

    if (m_worker.joinable() && m_worker.get_id() != std::this_thread::get_id()) {
        m_worker.join();
    }

    if (m_state.load() != ScanState::Idle) {
        m_state.store(ScanState::Idle);
        ScanStateChangedCallback stateCb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            stateCb = m_stateCb;
        }
        if (stateCb) {
            stateCb(ScanState::Idle);
        }
    }
}

void BusScanner::pauseScan()
{
    if (m_state.load() == ScanState::Scanning) {
        m_pauseRequested.store(true);
        m_state.store(ScanState::Paused);
        ScanStateChangedCallback stateCb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            stateCb = m_stateCb;
        }
        if (stateCb) {
            stateCb(ScanState::Paused);
        }
    }
}

void BusScanner::resumeScan()
{
    if (m_state.load() == ScanState::Paused) {
        m_pauseRequested.store(false);
        m_state.store(ScanState::Scanning);
        ScanStateChangedCallback stateCb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            stateCb = m_stateCb;
        }
        if (stateCb) {
            stateCb(ScanState::Scanning);
        }
    }
}

bool BusScanner::isScanning() const noexcept
{
    return m_state.load() == ScanState::Scanning;
}

bool BusScanner::isPaused() const noexcept
{
    return m_state.load() == ScanState::Paused;
}

ScanState BusScanner::getState() const noexcept
{
    return m_state.load();
}

std::vector<DiscoveredDevice> BusScanner::getDiscoveredDevices() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_discoveredDevices;
}

void BusScanner::setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_discoveredCb = std::move(cb);
}

void BusScanner::setScanProgressCallback(ScanProgressCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCb = std::move(cb);
}

void BusScanner::setScanStateChangedCallback(ScanStateChangedCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCb = std::move(cb);
}

void BusScanner::setScanFinishedCallback(ScanFinishedCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_finishedCb = std::move(cb);
}

void BusScanner::onDataReceived(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_rxMutex);
    m_rxBuffer.insert(m_rxBuffer.end(), data.begin(), data.end());

    const auto frames = PelcoDFrame::splitStream(m_rxBuffer);
    for (const auto& frame : frames) {
        if (!frame.empty() && frame[1] == m_currentProbeAddress) {
            m_foundResponse = true;
            m_matchedResponse = frame;
            m_rxCv.notify_all();
            break;
        }
    }
}

void BusScanner::scanWorker(ScanConfig config)
{
    if (config.startAddress > config.endAddress) {
        std::swap(config.startAddress, config.endAddress);
    }
    if (config.startAddress == 0U) {
        config.startAddress = 1U;
    }
    if (config.endAddress == 0U) {
        config.endAddress = 254U;
    }

    const std::size_t totalCount = static_cast<std::size_t>(config.endAddress - config.startAddress + 1);
    std::size_t scannedCount = 0U;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_discoveredDevices.clear();
    }

    for (int addr = config.startAddress; addr <= config.endAddress; ++addr) {
        if (m_stopRequested.load()) {
            break;
        }

        while (m_pauseRequested.load() && !m_stopRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (m_stopRequested.load()) {
            break;
        }

        const auto targetAddr = static_cast<std::uint8_t>(addr);
        ++scannedCount;

        ScanProgressCallback progCb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            progCb = m_progressCb;
        }
        if (progCb) {
            progCb(targetAddr, scannedCount, totalCount);
        }

        {
            std::lock_guard<std::mutex> rxLock(m_rxMutex);
            m_rxBuffer.clear();
            m_currentProbeAddress = targetAddr;
            m_foundResponse = false;
            m_matchedResponse.clear();
        }

        const auto probe = ProtocolBuilder::buildQueryPan(targetAddr);
        const auto sentTime = std::chrono::steady_clock::now();

        std::shared_ptr<ITransport> trans;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            trans = m_transport;
        }

        if (!trans || !trans->sendData(probe)) {
            continue;
        }

        std::unique_lock<std::mutex> rxLock(m_rxMutex);
        m_rxCv.wait_for(rxLock, std::chrono::milliseconds(config.timeoutMs), [this] {
            return m_foundResponse || m_stopRequested.load();
        });

        if (m_stopRequested.load()) {
            break;
        }

        if (m_foundResponse) {
            const auto endTime = std::chrono::steady_clock::now();
            const auto latencyMs = static_cast<std::uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - sentTime).count());

            DiscoveredDevice dev;
            dev.address = targetAddr;
            dev.responseTimeMs = latencyMs;
            dev.rawResponse = m_matchedResponse;

            std::uint16_t panVal { 0U };
            if (ProtocolParser::parsePan(dev.rawResponse, panVal)) {
                dev.hasPanPosition = true;
                dev.panCentidegrees = panVal;
            }

            DeviceDiscoveredCallback discCb;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_discoveredDevices.push_back(dev);
                discCb = m_discoveredCb;
            }
            if (discCb) {
                discCb(dev);
            }
        }

        rxLock.unlock();

        if (config.interCommandDelayMs > 0U) {
            std::unique_lock<std::mutex> delayLock(m_rxMutex);
            m_rxCv.wait_for(delayLock, std::chrono::milliseconds(config.interCommandDelayMs), [this] {
                return m_stopRequested.load();
            });
        }
    }

    std::vector<DiscoveredDevice> results;
    ScanStateChangedCallback stateCb;
    ScanFinishedCallback finCb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        results = m_discoveredDevices;
        stateCb = m_stateCb;
        finCb = m_finishedCb;
    }

    if (finCb) {
        finCb(results);
    }
    if (stateCb) {
        stateCb(ScanState::Idle);
    }

    m_state.store(ScanState::Idle);
}

} // namespace PelcoD
