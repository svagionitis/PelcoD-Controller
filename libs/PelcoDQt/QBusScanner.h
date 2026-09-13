#pragma once

/// @file QBusScanner.h
/// @brief Qt wrapper providing signals and slots for PelcoD::BusScanner.

#include "BusScanner.h"
#include "ITransport.h"

#include <QObject>
#include <memory>
#include <vector>

namespace PelcoDQt {

/// @class QBusScanner
/// @brief Qt QObject adapter for PelcoD::BusScanner with thread-safe signals.
class QBusScanner : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a QBusScanner wrapping a PelcoD::BusScanner.
    /// @param[in] transport Underlying transport used for probing.
    /// @param[in] parent Qt parent QObject.
    explicit QBusScanner(std::shared_ptr<PelcoD::ITransport> transport = nullptr, QObject* parent = nullptr);

    /// @brief Destructor stopping any active discovery scan.
    ~QBusScanner() override;

    // Non-copyable, non-movable
    QBusScanner(const QBusScanner&) = delete;
    QBusScanner& operator=(const QBusScanner&) = delete;
    QBusScanner(QBusScanner&&) = delete;
    QBusScanner& operator=(QBusScanner&&) = delete;

    /// @brief Check if discovery scan is actively executing.
    /// @return true if scanning, false otherwise.
    [[nodiscard]] bool isScanning() const;

    /// @brief Check if discovery scan is temporarily paused.
    /// @return true if paused, false otherwise.
    [[nodiscard]] bool isPaused() const;

    /// @brief Get current operational state of scanner engine.
    /// @return ScanState enumeration value.
    [[nodiscard]] PelcoD::ScanState getState() const;

    /// @brief Retrieve snapshot of discovered devices detected so far.
    /// @return Vector of DiscoveredDevice records.
    [[nodiscard]] std::vector<PelcoD::DiscoveredDevice> discoveredDevices() const;

public slots:
    /// @brief Assign or update transport used for bus scanning.
    /// @param[in] transport Shared pointer to active ITransport.
    void setTransport(std::shared_ptr<PelcoD::ITransport> transport);

    /// @brief Start asynchronous address probe across specified range.
    /// @param[in] startAddress Lowest bus address to probe (1–254).
    /// @param[in] endAddress Highest bus address to probe (1–254).
    /// @param[in] timeoutMs Per-device response timeout in milliseconds.
    /// @return true if scan successfully started, false on invalid parameters.
    bool startScan(int startAddress = 1, int endAddress = 32, int timeoutMs = 150);

    /// @brief Stop active discovery scan.
    void stopScan();

    /// @brief Pause active discovery scan.
    void pauseScan();

    /// @brief Resume paused discovery scan.
    void resumeScan();

signals:
    /// @brief Emitted on GUI thread when an active Pelco-D device is identified.
    /// @param address Discovered device address (1–254).
    /// @param responseTimeMs Round-trip latency in milliseconds.
    /// @param hasPan True if response decoded pan position.
    /// @param panCentidegrees Pan angle in hundredths of a degree.
    void deviceDiscovered(int address, int responseTimeMs, bool hasPan, int panCentidegrees);

    /// @brief Emitted on GUI thread when progress changes after each probed address.
    /// @param currentAddress Address currently probed.
    /// @param scannedCount Total addresses tested so far.
    /// @param totalCount Total addresses in target range.
    /// @param percent Integer percentage completion (0–100).
    void progressUpdated(int currentAddress, int scannedCount, int totalCount, int percent);

    /// @brief Emitted on GUI thread when scanner state transitions.
    /// @param state New operational state.
    void stateChanged(PelcoD::ScanState state);

    /// @brief Emitted on GUI thread when address range probe has fully completed.
    /// @param totalFound Total number of responsive devices discovered.
    void scanFinished(int totalFound);

private:
    void setupCallbacks();

    std::unique_ptr<PelcoD::BusScanner> m_scanner;
};

} // namespace PelcoDQt
