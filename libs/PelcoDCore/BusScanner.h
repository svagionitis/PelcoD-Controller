#pragma once

/// @file BusScanner.h
/// @brief Multi-drop RS-485 bus address auto-discovery scanner.

#include "ITransport.h"
#include "PelcoDFrame.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @struct DiscoveredDevice
/// @brief Attributes of a device detected during bus scanning.
struct DiscoveredDevice {
    std::uint8_t address { 1U };
    std::uint32_t responseTimeMs { 0U };
    bool hasPanPosition { false };
    std::uint16_t panCentidegrees { 0U };
    std::vector<std::uint8_t> rawResponse {};
};

/// @enum ScanState
/// @brief Current operational state of the bus scanner.
enum class ScanState : std::uint8_t { Idle, Scanning, Paused };

/// @struct ScanConfig
/// @brief Configuration settings controlling address probe range and timing.
struct ScanConfig {
    std::uint8_t startAddress { 1U };
    std::uint8_t endAddress { 32U };
    std::uint32_t timeoutMs { 150U };
    std::uint32_t interCommandDelayMs { 20U };
};

/// @class BusScanner
/// @brief Automated discovery engine probing RS-485 and IP networks for Pelco-D devices.
class BusScanner {
public:
    using DeviceDiscoveredCallback = std::function<void(const DiscoveredDevice& device)>;
    using ScanProgressCallback
        = std::function<void(std::uint8_t currentAddress, std::size_t scannedCount, std::size_t totalCount)>;
    using ScanStateChangedCallback = std::function<void(ScanState state)>;
    using ScanFinishedCallback = std::function<void(const std::vector<DiscoveredDevice>& discoveredDevices)>;

    /// @brief Construct a BusScanner with an optional transport instance.
    /// @param[in] transport Underlying transport used for probing addresses.
    explicit BusScanner(std::shared_ptr<ITransport> transport = nullptr);

    /// @brief Destructor stopping any active background scan.
    ~BusScanner();

    // Non-copyable, non-movable
    BusScanner(const BusScanner&) = delete;
    BusScanner& operator=(const BusScanner&) = delete;
    BusScanner(BusScanner&&) = delete;
    BusScanner& operator=(BusScanner&&) = delete;

    /// @brief Configure the transport interface for probing.
    /// @param[in] transport Pointer to the ITransport instance.
    void setTransport(std::shared_ptr<ITransport> transport);

    /// @brief Retrieve the currently configured transport interface.
    /// @return Shared pointer to active ITransport or nullptr.
    [[nodiscard]] std::shared_ptr<ITransport> getTransport() const;

    /// @brief Initiate asynchronous address range discovery scan.
    /// @param[in] config Probe configuration controlling address limits and timeouts.
    /// @return true if scan started successfully, false on invalid parameters or busy state.
    bool startScan(const ScanConfig& config = ScanConfig {});

    /// @brief Terminate any currently active or paused discovery scan.
    void stopScan();

    /// @brief Pause active discovery scan.
    void pauseScan();

    /// @brief Resume paused discovery scan.
    void resumeScan();

    /// @brief Check if discovery scan is actively executing.
    /// @return true if actively probing, false otherwise.
    [[nodiscard]] bool isScanning() const noexcept;

    /// @brief Check if discovery scan is temporarily paused.
    /// @return true if paused, false otherwise.
    [[nodiscard]] bool isPaused() const noexcept;

    /// @brief Get current operational state of scanner engine.
    /// @return Current ScanState enumeration value.
    [[nodiscard]] ScanState getState() const noexcept;

    /// @brief Retrieve snapshot of discovered devices detected so far.
    /// @return Vector of DiscoveredDevice records.
    [[nodiscard]] std::vector<DiscoveredDevice> getDiscoveredDevices() const;

    /// @brief Register callback invoked when a device is successfully identified.
    /// @param[in] cb Callback receiving DiscoveredDevice descriptor.
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb);

    /// @brief Register callback invoked after probing each bus address.
    /// @param[in] cb Callback receiving current address, scanned count, and total count.
    void setScanProgressCallback(ScanProgressCallback cb);

    /// @brief Register callback invoked on scan operational state transitions.
    /// @param[in] cb Callback receiving new ScanState value.
    void setScanStateChangedCallback(ScanStateChangedCallback cb);

    /// @brief Register callback invoked when address range probe has fully completed.
    /// @param[in] cb Callback receiving final list of all discovered devices.
    void setScanFinishedCallback(ScanFinishedCallback cb);

private:
    void onDataReceived(const std::vector<std::uint8_t>& data);
    void scanWorker(ScanConfig config);

    mutable std::mutex m_mutex;
    std::shared_ptr<ITransport> m_transport;

    std::thread m_worker;
    std::atomic<bool> m_stopRequested { false };
    std::atomic<bool> m_pauseRequested { false };
    std::condition_variable m_pauseCv;
    std::atomic<ScanState> m_state { ScanState::Idle };

    // Response synchronization
    std::mutex m_rxMutex;
    std::condition_variable m_rxCv;
    std::vector<std::uint8_t> m_rxBuffer;
    std::uint8_t m_currentProbeAddress { 0U };
    bool m_foundResponse { false };
    std::vector<std::uint8_t> m_matchedResponse;

    std::vector<DiscoveredDevice> m_discoveredDevices;

    DeviceDiscoveredCallback m_discoveredCb;
    ScanProgressCallback m_progressCb;
    ScanStateChangedCallback m_stateCb;
    ScanFinishedCallback m_finishedCb;
};

} // namespace PelcoD
