/// @file QBusScanner.cpp
/// @brief Implementation of Qt wrapper for PelcoD::BusScanner.

#include "QBusScanner.h"

#include <QMetaObject>
#include <algorithm>
#include <utility>

namespace PelcoDQt {

QBusScanner::QBusScanner(std::shared_ptr<PelcoD::ITransport> transport, QObject* parent)
    : QObject(parent)
    , m_scanner(std::make_unique<PelcoD::BusScanner>(std::move(transport)))
{
    setupCallbacks();
}

QBusScanner::~QBusScanner()
{
    if (m_scanner) {
        m_scanner->stopScan();
    }
}

void QBusScanner::setupCallbacks()
{
    m_scanner->setDeviceDiscoveredCallback([this](const PelcoD::DiscoveredDevice& dev) {
        const int addr = static_cast<int>(dev.address);
        const int respTime = static_cast<int>(dev.responseTimeMs);
        const bool hasPan = dev.hasPanPosition;
        const int pan = static_cast<int>(dev.panCentidegrees);

        QMetaObject::invokeMethod(
            this, [this, addr, respTime, hasPan, pan] {
                emit deviceDiscovered(addr, respTime, hasPan, pan);
            }, Qt::QueuedConnection);
    });

    m_scanner->setScanProgressCallback([this](std::uint8_t currentAddress, std::size_t scannedCount, std::size_t totalCount) {
        const int cur = static_cast<int>(currentAddress);
        const int scanned = static_cast<int>(scannedCount);
        const int total = static_cast<int>(totalCount);
        const int pct = total > 0 ? (scanned * 100 / total) : 0;

        QMetaObject::invokeMethod(
            this, [this, cur, scanned, total, pct] {
                emit progressUpdated(cur, scanned, total, pct);
            }, Qt::QueuedConnection);
    });

    m_scanner->setScanStateChangedCallback([this](PelcoD::ScanState state) {
        QMetaObject::invokeMethod(
            this, [this, state] {
                emit stateChanged(state);
            }, Qt::QueuedConnection);
    });

    m_scanner->setScanFinishedCallback([this](const std::vector<PelcoD::DiscoveredDevice>& devices) {
        const int count = static_cast<int>(devices.size());
        QMetaObject::invokeMethod(
            this, [this, count] {
                emit scanFinished(count);
            }, Qt::QueuedConnection);
    });
}

void QBusScanner::setTransport(std::shared_ptr<PelcoD::ITransport> transport)
{
    if (m_scanner) {
        m_scanner->setTransport(std::move(transport));
    }
}

bool QBusScanner::startScan(int startAddress, int endAddress, int timeoutMs)
{
    if (!m_scanner) {
        return false;
    }

    PelcoD::ScanConfig cfg;
    cfg.startAddress = static_cast<std::uint8_t>(std::clamp(startAddress, 1, 254));
    cfg.endAddress = static_cast<std::uint8_t>(std::clamp(endAddress, 1, 254));
    cfg.timeoutMs = static_cast<std::uint32_t>(std::max(10, timeoutMs));

    return m_scanner->startScan(cfg);
}

void QBusScanner::stopScan()
{
    if (m_scanner) {
        m_scanner->stopScan();
    }
}

void QBusScanner::pauseScan()
{
    if (m_scanner) {
        m_scanner->pauseScan();
    }
}

void QBusScanner::resumeScan()
{
    if (m_scanner) {
        m_scanner->resumeScan();
    }
}

bool QBusScanner::isScanning() const
{
    return m_scanner ? m_scanner->isScanning() : false;
}

bool QBusScanner::isPaused() const
{
    return m_scanner ? m_scanner->isPaused() : false;
}

PelcoD::ScanState QBusScanner::getState() const
{
    return m_scanner ? m_scanner->getState() : PelcoD::ScanState::Idle;
}

std::vector<PelcoD::DiscoveredDevice> QBusScanner::discoveredDevices() const
{
    return m_scanner ? m_scanner->getDiscoveredDevices() : std::vector<PelcoD::DiscoveredDevice> {};
}

} // namespace PelcoDQt
