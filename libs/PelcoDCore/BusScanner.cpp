/// @file BusScanner.cpp
/// @brief Implementation of RS-485 bus address auto-discovery scanner.

#include "BusScanner.h"

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace PelcoD {

template <typename CallbackT>
bool BusScanner::removeEntry(std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>>& list, std::uint64_t id,
    std::mutex& mtx, std::shared_ptr<CallbackGate>& outGate)
{
    std::scoped_lock lock(mtx);
    const auto& current = *list;
    auto it = std::find_if(current.begin(), current.end(), [id](const auto& entry) { return entry.id == id; });
    if (it == current.end()) {
        return false;
    }
    outGate = it->gate;
    auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>();
    nextList->reserve(current.size() - 1U);
    for (const auto& entry : current) {
        if (entry.id != id) {
            nextList->push_back(entry);
        }
    }
    list = std::move(nextList);
    return true;
}

template <typename CallbackT, typename... Args>
void BusScanner::dispatchSafely(
    const std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>>& list, Args&&... args)
{
    for (const auto& entry : *list) {
        if (entry.gate) {
            const CallbackGate::Pass pass(*entry.gate);
            if (pass) {
                try {
                    entry.cb(std::forward<Args>(args)...);
                } catch (const std::exception& /*e*/) {
                    // SEI CERT ERR50-CPP
                } catch (...) {
                }
            }
        }
    }
}

BusScanner::BusScanner(std::shared_ptr<ITransport> transport)
    : m_transport(std::move(transport))
{
}

BusScanner::~BusScanner()
{
    stopScan();
    clearCallbacks();
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

template <typename CallbackT>
Connection BusScanner::registerHubCallback(
    CallbackT cb, std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>> CallbackHub::* listMember)
{
    if (!cb) {
        return Connection { };
    }
    const auto id = m_hub->nextId.fetch_add(1U, std::memory_order_relaxed);
    auto gate = std::make_shared<CallbackGate>();

    {
        std::scoped_lock lock(m_hub->mutex);
        const auto& current = *(m_hub.get()->*listMember);
        auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>();
        nextList->reserve(current.size() + 1U);
        for (const auto& entry : current) {
            nextList->push_back(entry);
        }
        nextList->push_back(CallbackEntry<CallbackT> { id, std::move(cb), gate });
        m_hub.get()->*listMember = std::move(nextList);
    }

    std::weak_ptr<CallbackHub> weakHub = m_hub;
    return Connection([weakHub, id, listMember, gate]() {
        if (auto hub = weakHub.lock()) {
            std::shared_ptr<CallbackGate> outGate;
            const bool removed = removeEntry(hub.get()->*listMember, id, hub->mutex, outGate);
            if (removed && outGate) {
                outGate->close();
            }
        } else if (gate) {
            gate->close();
        }
    });
}

Connection BusScanner::addDeviceDiscoveredCallback(DeviceDiscoveredCallback cb)
{
    return registerHubCallback(std::move(cb), &CallbackHub::discoveredList);
}

Connection BusScanner::addScanProgressCallback(ScanProgressCallback cb)
{
    return registerHubCallback(std::move(cb), &CallbackHub::progressList);
}

Connection BusScanner::addBaudRateChangedCallback(BaudRateChangedCallback cb)
{
    return registerHubCallback(std::move(cb), &CallbackHub::baudRateList);
}

Connection BusScanner::addMultiBaudProgressCallback(MultiBaudProgressCallback cb)
{
    return registerHubCallback(std::move(cb), &CallbackHub::multiBaudProgressList);
}

Connection BusScanner::addScanStateChangedCallback(ScanStateChangedCallback cb)
{
    return registerHubCallback(std::move(cb), &CallbackHub::stateList);
}

Connection BusScanner::addScanFinishedCallback(ScanFinishedCallback cb)
{
    return registerHubCallback(std::move(cb), &CallbackHub::finishedList);
}

void BusScanner::setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb)
{
    m_legacyDiscoveredConn = addDeviceDiscoveredCallback(std::move(cb));
}

void BusScanner::setScanProgressCallback(ScanProgressCallback cb)
{
    m_legacyProgressConn = addScanProgressCallback(std::move(cb));
}

void BusScanner::setBaudRateChangedCallback(BaudRateChangedCallback cb)
{
    m_legacyBaudRateConn = addBaudRateChangedCallback(std::move(cb));
}

void BusScanner::setMultiBaudProgressCallback(MultiBaudProgressCallback cb)
{
    m_legacyMultiBaudProgressConn = addMultiBaudProgressCallback(std::move(cb));
}

void BusScanner::setScanStateChangedCallback(ScanStateChangedCallback cb)
{
    m_legacyStateConn = addScanStateChangedCallback(std::move(cb));
}

void BusScanner::setScanFinishedCallback(ScanFinishedCallback cb)
{
    m_legacyFinishedConn = addScanFinishedCallback(std::move(cb));
}

void BusScanner::clearCallbacks()
{
    std::vector<std::shared_ptr<CallbackGate>> gatesToClose;
    {
        std::scoped_lock lock(m_hub->mutex);
        for (const auto& entry : *m_hub->discoveredList) {
            if (entry.gate) {
                gatesToClose.push_back(entry.gate);
            }
        }
        for (const auto& entry : *m_hub->progressList) {
            if (entry.gate) {
                gatesToClose.push_back(entry.gate);
            }
        }
        for (const auto& entry : *m_hub->baudRateList) {
            if (entry.gate) {
                gatesToClose.push_back(entry.gate);
            }
        }
        for (const auto& entry : *m_hub->multiBaudProgressList) {
            if (entry.gate) {
                gatesToClose.push_back(entry.gate);
            }
        }
        for (const auto& entry : *m_hub->stateList) {
            if (entry.gate) {
                gatesToClose.push_back(entry.gate);
            }
        }
        for (const auto& entry : *m_hub->finishedList) {
            if (entry.gate) {
                gatesToClose.push_back(entry.gate);
            }
        }

        m_hub->discoveredList = std::make_shared<const std::vector<CallbackEntry<DeviceDiscoveredCallback>>>();
        m_hub->progressList = std::make_shared<const std::vector<CallbackEntry<ScanProgressCallback>>>();
        m_hub->baudRateList = std::make_shared<const std::vector<CallbackEntry<BaudRateChangedCallback>>>();
        m_hub->multiBaudProgressList = std::make_shared<const std::vector<CallbackEntry<MultiBaudProgressCallback>>>();
        m_hub->stateList = std::make_shared<const std::vector<CallbackEntry<ScanStateChangedCallback>>>();
        m_hub->finishedList = std::make_shared<const std::vector<CallbackEntry<ScanFinishedCallback>>>();
    }
    for (auto& gate : gatesToClose) {
        if (gate) {
            gate->close();
        }
    }
}

void BusScanner::notifyStateChanged(ScanState state)
{
    std::shared_ptr<const std::vector<CallbackEntry<ScanStateChangedCallback>>> snapshot;
    {
        std::scoped_lock lock(m_hub->mutex);
        snapshot = m_hub->stateList;
    }
    dispatchSafely(snapshot, state);
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
    }

    notifyStateChanged(ScanState::Scanning);
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

    {
        std::scoped_lock lock(m_mutex);
        trans = m_transport;
        gate = m_gate;
        if (m_state.load() != ScanState::Idle) {
            m_state.store(ScanState::Idle);
            shouldEmitIdle = true;
        }
    }

    if (trans) {
        trans->setDataCallback(nullptr);
    }
    if (gate) {
        gate->close();
    }

    if (shouldEmitIdle) {
        notifyStateChanged(ScanState::Idle);
    }
}

void BusScanner::pauseScan()
{
    if (m_state.load() == ScanState::Scanning) {
        m_pauseRequested.store(true);
        m_state.store(ScanState::Paused);
        notifyStateChanged(ScanState::Paused);
    }
}

void BusScanner::resumeScan()
{
    if (m_state.load() == ScanState::Paused) {
        m_pauseRequested.store(false);
        m_state.store(ScanState::Scanning);
        m_pauseCv.notify_all();
        notifyStateChanged(ScanState::Scanning);
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

    // RAII guard guaranteeing restoration of original baud rate upon worker exit (M11)
    struct BaudRestorer {
        std::shared_ptr<ITransport> transport;
        std::uint32_t baud;
        bool active;
        ~BaudRestorer()
        {
            if (active && transport && baud > 0U) {
                transport->setBaudRate(baud);
            }
        }
    } baudRestorer { trans, originalBaud, !config.baudRates.empty() };

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

        if (trans && currentBaud > 0U && currentBaud != trans->getBaudRate()) {
            if (!trans->setBaudRate(currentBaud)) {
                // Transport rejected baud rate modification (e.g. shared multiplexer or fixed port)
                continue;
            }
        }

        std::shared_ptr<const std::vector<CallbackEntry<BaudRateChangedCallback>>> baudSnapshot;
        {
            std::scoped_lock lock(m_hub->mutex);
            baudSnapshot = m_hub->baudRateList;
        }
        dispatchSafely(baudSnapshot, currentBaud);

        if (config.baudSwitchDelayMs > 0U && currentBaud > 0U) {
            std::unique_lock<std::mutex> delayLock(m_rxMutex);
            m_rxCv.wait_for(delayLock, std::chrono::milliseconds(config.baudSwitchDelayMs),
                [this] { return m_stopRequested.load(); });
            if (m_stopRequested.load()) {
                break;
            }
        }

        for (std::uint32_t addr { config.startAddress }; addr <= config.endAddress; ++addr) {
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

            std::shared_ptr<const std::vector<CallbackEntry<ScanProgressCallback>>> progSnapshot;
            std::shared_ptr<const std::vector<CallbackEntry<MultiBaudProgressCallback>>> multiProgSnapshot;
            {
                std::scoped_lock lock(m_hub->mutex);
                progSnapshot = m_hub->progressList;
                multiProgSnapshot = m_hub->multiBaudProgressList;
            }
            dispatchSafely(progSnapshot, targetAddr, scannedCount, totalCount);
            dispatchSafely(multiProgSnapshot, currentBaud, targetAddr, scannedCount, totalCount);

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

                {
                    std::scoped_lock lock(m_mutex);
                    m_discoveredDevices.push_back(dev);
                }

                std::shared_ptr<const std::vector<CallbackEntry<DeviceDiscoveredCallback>>> discSnapshot;
                {
                    std::scoped_lock lock(m_hub->mutex);
                    discSnapshot = m_hub->discoveredList;
                }
                dispatchSafely(discSnapshot, dev);
            }

            rxLock.unlock();

            if (config.interCommandDelayMs > 0U) {
                std::unique_lock<std::mutex> delayLock(m_rxMutex);
                m_rxCv.wait_for(delayLock, std::chrono::milliseconds(config.interCommandDelayMs),
                    [this] { return m_stopRequested.load(); });
            }
        }
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
    {
        std::scoped_lock lock(m_mutex);
        results = m_discoveredDevices;
    }

    std::shared_ptr<const std::vector<CallbackEntry<ScanFinishedCallback>>> finSnapshot;
    {
        std::scoped_lock lock(m_hub->mutex);
        finSnapshot = m_hub->finishedList;
    }
    dispatchSafely(finSnapshot, results);

    notifyStateChanged(ScanState::Idle);
}

} // namespace PelcoD
