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
    std::shared_ptr<ITransport> oldTrans;
    std::shared_ptr<CallbackGate> oldGate;
    {
        std::scoped_lock lock(m_mutex);
        if (m_state.load() != ScanState::Idle) {
            return;
        }
        oldTrans = m_transport;
        oldGate = m_gate;
        m_transport = std::move(transport);
        m_gate.reset();
    }
    if (oldTrans) {
        oldTrans->setDataCallback(nullptr);
    }
    if (oldGate) {
        oldGate->close();
    }
}

std::shared_ptr<ITransport> BusScanner::getTransport() const
{
    std::scoped_lock lock(m_mutex);
    return m_transport;
}

bool BusScanner::startScan(const ScanConfig& config)
{
    if (config.startAddress == 0U || config.startAddress > 254U || config.endAddress == 0U || config.endAddress > 254U
        || config.startAddress > config.endAddress) {
        return false;
    }

    if (m_worker.joinable() && m_worker.get_id() == std::this_thread::get_id()) {
        return false;
    }

    ScanStateChangedCallback stateCb;
    {
        std::scoped_lock lock(m_mutex);
        if (m_state.load() != ScanState::Idle || !m_transport) {
            return false;
        }

        if (!m_transport->isOpen()) {
            if (!m_transport->open()) {
                return false;
            }
        }

        if (m_gate) {
            m_gate->close();
        }
        auto gate = std::make_shared<CallbackGate>();
        m_gate = gate;

        // Register callback for incoming responses guarded by CallbackGate
        m_transport->setDataCallback([this, gate](const std::vector<std::uint8_t>& data) {
            const CallbackGate::Pass pass(*gate);
            if (!pass) {
                return;
            }
            onDataReceived(data);
        });

        if (m_worker.joinable()) {
            m_worker.join();
        }

        m_stopRequested.store(false);
        m_pauseRequested.store(false);
        m_state.store(ScanState::Scanning);

        m_worker = std::thread(&BusScanner::scanWorker, this, config);
        stateCb = m_stateCb;
    }

    // Dispatch state change outside of m_mutex to prevent re-entrant deadlocks
    if (stateCb) {
        stateCb(ScanState::Scanning);
    }

    return true;
}

void BusScanner::stopScan()
{
    m_stopRequested.store(true);
    m_pauseRequested.store(false);
    m_rxCv.notify_all();
    m_pauseCv.notify_all();

    if (m_worker.joinable() && m_worker.get_id() != std::this_thread::get_id()) {
        m_worker.join();
    }

    std::shared_ptr<ITransport> trans;
    std::shared_ptr<CallbackGate> gate;
    bool shouldEmitIdle { false };
    ScanStateChangedCallback stateCb;

    {
        std::scoped_lock lock(m_mutex);
        trans = m_transport;
        gate = m_gate;
        if (m_state.load() != ScanState::Idle) {
            m_state.store(ScanState::Idle);
            shouldEmitIdle = true;
            stateCb = m_stateCb;
        }
    }

    if (trans) {
        trans->setDataCallback(nullptr);
    }
    if (gate) {
        gate->close();
    }

    if (shouldEmitIdle && stateCb) {
        stateCb(ScanState::Idle);
    }
}

void BusScanner::pauseScan()
{
    if (m_state.load() == ScanState::Scanning) {
        m_pauseRequested.store(true);
        m_state.store(ScanState::Paused);
        ScanStateChangedCallback stateCb;
        {
            std::scoped_lock lock(m_mutex);
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
        m_pauseCv.notify_all();
        ScanStateChangedCallback stateCb;
        {
            std::scoped_lock lock(m_mutex);
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
    std::scoped_lock lock(m_mutex);
    return m_discoveredDevices;
}

void BusScanner::setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_discoveredCb = std::move(cb);
}

void BusScanner::setScanProgressCallback(ScanProgressCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_progressCb = std::move(cb);
}

void BusScanner::setBaudRateChangedCallback(BaudRateChangedCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_baudRateCb = std::move(cb);
}

void BusScanner::setMultiBaudProgressCallback(MultiBaudProgressCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_multiBaudProgressCb = std::move(cb);
}

void BusScanner::setScanStateChangedCallback(ScanStateChangedCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_stateCb = std::move(cb);
}

void BusScanner::setScanFinishedCallback(ScanFinishedCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_finishedCb = std::move(cb);
}

void BusScanner::onDataReceived(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return;
    }

    std::scoped_lock lock(m_rxMutex);
    m_rxBuffer.insert(m_rxBuffer.end(), data.begin(), data.end());

    const auto frames = PelcoDFrame::splitStream(m_rxBuffer);
    for (const auto& frame : frames) {
        if (frame.empty() || !PelcoDFrame::isValidFrame(frame)) {
            continue;
        }

        // Filter out probe packet echo (common on RS-485 2-wire half-duplex)
        if (!m_currentProbePacket.empty() && frame == m_currentProbePacket) {
            continue;
        }

        // Verify that the frame is for the probed address and contains valid Pan telemetry
        if (frame[1] == m_currentProbeAddress) {
            std::uint16_t panVal { 0U };
            if (ProtocolParser::parsePan(frame, panVal)) {
                m_foundResponse = true;
                m_matchedResponse = frame;
                m_matchedPan = panVal;
                m_rxCv.notify_all();
                break;
            }
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

    std::shared_ptr<ITransport> trans;
    {
        std::scoped_lock lock(m_mutex);
        trans = m_transport;
    }

    const std::uint32_t originalBaud = trans ? trans->getBaudRate() : 0U;

    std::vector<std::uint32_t> baudList = config.baudRates;
    if (baudList.empty()) {
        baudList.push_back(originalBaud);
    }

    const std::size_t addressesPerBaud = static_cast<std::size_t>(config.endAddress - config.startAddress + 1);
    const std::size_t totalCount = addressesPerBaud * baudList.size();
    std::size_t scannedCount = 0U;

    {
        std::scoped_lock lock(m_mutex);
        m_discoveredDevices.clear();
    }

    for (const auto currentBaud : baudList) {
        if (m_stopRequested.load()) {
            break;
        }

        if (trans && currentBaud > 0U) {
            trans->setBaudRate(currentBaud);
        }

        BaudRateChangedCallback baudCb;
        {
            std::scoped_lock lock(m_mutex);
            baudCb = m_baudRateCb;
        }
        if (baudCb) {
            baudCb(currentBaud);
        }

        if (config.baudSwitchDelayMs > 0U && currentBaud > 0U) {
            std::unique_lock<std::mutex> delayLock(m_rxMutex);
            m_rxCv.wait_for(delayLock, std::chrono::milliseconds(config.baudSwitchDelayMs),
                [this] { return m_stopRequested.load(); });
            if (m_stopRequested.load()) {
                break;
            }
        }

        for (int addr = config.startAddress; addr <= config.endAddress; ++addr) {
            if (m_stopRequested.load()) {
                break;
            }

            {
                std::unique_lock<std::mutex> pauseLock(m_mutex);
                m_pauseCv.wait(pauseLock, [this] { return !m_pauseRequested.load() || m_stopRequested.load(); });
            }

            if (m_stopRequested.load()) {
                break;
            }

            const auto targetAddr = static_cast<std::uint8_t>(addr);
            ++scannedCount;

            ScanProgressCallback progCb;
            MultiBaudProgressCallback multiProgCb;
            {
                std::scoped_lock lock(m_mutex);
                progCb = m_progressCb;
                multiProgCb = m_multiBaudProgressCb;
            }
            if (progCb) {
                progCb(targetAddr, scannedCount, totalCount);
            }
            if (multiProgCb) {
                multiProgCb(currentBaud, targetAddr, scannedCount, totalCount);
            }

            const auto probe = ProtocolBuilder::buildQueryPan(targetAddr);
            const auto sentTime = std::chrono::steady_clock::now();

            {
                std::scoped_lock rxLock(m_rxMutex);
                m_rxBuffer.clear();
                m_currentProbePacket = probe;
                m_currentProbeAddress = targetAddr;
                m_foundResponse = false;
                m_matchedResponse.clear();
                m_matchedPan = 0U;
            }

            std::shared_ptr<ITransport> activeTrans;
            {
                std::scoped_lock lock(m_mutex);
                activeTrans = m_transport;
            }

            if (!activeTrans || !activeTrans->sendData(probe)) {
                continue;
            }

            std::unique_lock<std::mutex> rxLock(m_rxMutex);
            m_rxCv.wait_for(rxLock, std::chrono::milliseconds(config.timeoutMs),
                [this] { return m_foundResponse || m_stopRequested.load(); });

            if (m_stopRequested.load()) {
                break;
            }

            if (m_foundResponse) {
                const auto endTime = std::chrono::steady_clock::now();
                const auto latencyMs = static_cast<std::uint32_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(endTime - sentTime).count());

                DiscoveredDevice dev;
                dev.address = targetAddr;
                dev.baudRate = (currentBaud > 0U) ? currentBaud : originalBaud;
                dev.responseTimeMs = latencyMs;
                dev.rawResponse = m_matchedResponse;
                dev.hasPanPosition = true;
                dev.panCentidegrees = m_matchedPan;

                DeviceDiscoveredCallback discCb;
                {
                    std::scoped_lock lock(m_mutex);
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
                m_rxCv.wait_for(delayLock, std::chrono::milliseconds(config.interCommandDelayMs),
                    [this] { return m_stopRequested.load(); });
            }
        }
    }

    if (trans && originalBaud > 0U && !config.baudRates.empty()) {
        trans->setBaudRate(originalBaud);
    }

    if (trans) {
        trans->setDataCallback(nullptr);
    }
    std::shared_ptr<CallbackGate> gate;
    {
        std::scoped_lock lock(m_mutex);
        gate = m_gate;
    }
    if (gate) {
        gate->close();
    }

    m_state.store(ScanState::Idle);

    std::vector<DiscoveredDevice> results;
    ScanStateChangedCallback stateCb;
    ScanFinishedCallback finCb;
    {
        std::scoped_lock lock(m_mutex);
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
}

} // namespace PelcoD
