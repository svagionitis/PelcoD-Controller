#pragma once

/// @file FlirPfecDevice.h
/// @brief Hardware controller for FLIR M-Series marine thermal/visible PTZ camera systems.

#include "NmeaSentenceBuilder.h"
#include "NmeaSentenceParser.h"
#include "NmeaStreamAccumulator.h"
#include "PfecTypes.h"
#include "Transport/ITransport.h"

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

namespace Nmea {

/// @class FlirPfecDevice
/// @brief Asynchronous thread-safe driver for FLIR M-Series marine PTZ camera heads over NMEA 0183 ($PFEC).
class FlirPfecDevice {
public:
    using PositionCallback = std::function<void(const PfecGimbalPosition&)>;

    /// @brief Constructs a FlirPfecDevice wrapping an underlying transport channel.
    /// @param[in] transport Shared pointer to transport (Serial RS-422, UDP, TCP).
    /// @param[in] maxAccumulatorBuffer Inbound stream buffer capacity.
    explicit FlirPfecDevice(std::shared_ptr<Transport::ITransport> transport, std::size_t maxAccumulatorBuffer = 1024U);

    virtual ~FlirPfecDevice();

    // Non-copyable, non-movable
    FlirPfecDevice(const FlirPfecDevice&) = delete;
    FlirPfecDevice& operator=(const FlirPfecDevice&) = delete;
    FlirPfecDevice(FlirPfecDevice&&) = delete;
    FlirPfecDevice& operator=(FlirPfecDevice&&) = delete;

    /// @brief Starts communication and spawns the background telemetry polling thread.
    /// @param[in] pollingInterval Interval between position inquiry queries ($PFEC,GPpos).
    ///                            Set to 0 to disable automatic polling.
    /// @return True if transport is open and thread started.
    [[nodiscard]] bool start(std::chrono::milliseconds pollingInterval = std::chrono::milliseconds(50));

    /// @brief Stops the polling thread, detaches callbacks, and closes the transport.
    void stop();

    /// @brief Checks whether the underlying transport is currently connected and open.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Accesses the underlying transport channel.
    [[nodiscard]] std::shared_ptr<Transport::ITransport> transport() const noexcept;

    // --- Motion & Positioning ---

    /// @brief Drives pan and tilt axes using normalized velocity ratios.
    /// @param[in] panVel Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Left, +1.0 = Max Right).
    /// @param[in] tiltVel Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Down, +1.0 = Max Up).
    /// @return True if command was dispatched.
    bool setVelocity(float panVel, float tiltVel);

    /// @brief Commands the gimbal to slew to absolute azimuth and elevation angles.
    /// @param[in] panDeg Target azimuth angle in degrees [0.0 .. 360.0) or [-180.0 .. +180.0].
    /// @param[in] tiltDeg Target elevation angle in degrees [-90.0 .. +90.0].
    /// @return True if target angle command was dispatched.
    bool setAbsoluteAngles(double panDeg, double tiltDeg);

    /// @brief Halts all pan and tilt gimbal motion immediately.
    /// @return True if stop command was dispatched.
    bool stopMotion();

    // --- Preset Management ---

    /// @brief Saves the current gimbal orientation as a numbered preset.
    /// @param[in] presetId Preset number [1 .. 255].
    /// @return True if preset save command was dispatched.
    bool savePreset(std::uint8_t presetId);

    /// @brief Commands the gimbal to slew to a saved preset orientation.
    /// @param[in] presetId Preset number [1 .. 255].
    /// @return True if preset recall command was dispatched.
    bool recallPreset(std::uint8_t presetId);

    // --- Optics & Thermal Controls ---

    /// @brief Sets optical continuous zoom rate.
    /// @param[in] zoomVel Normalized zoom rate [-1.0 .. +1.0] (-1.0 = Wide, +1.0 = Tele, 0.0 = Stop).
    /// @return True if zoom command was dispatched.
    bool setZoomRate(float zoomVel);

    /// @brief Selects the active video camera sensor (daylight visible or thermal IR).
    /// @param[in] sensor Target optical sensor.
    /// @return True if sensor select command was dispatched.
    bool selectSensor(FlirSensorType sensor);

    /// @brief Configures the thermal infrared false color palette.
    /// @param[in] palette Color palette (White-Hot, Black-Hot, Ironbow, etc.).
    /// @return True if palette command was dispatched.
    bool setColorPalette(FlirColorPalette palette);

    /// @brief Triggers Non-Uniformity Correction (NUC / flat field calibration) on the thermal core.
    /// @return True if NUC command was dispatched.
    bool triggerNuc();

    /// @brief Queries current gimbal position manually ($PFEC,GPpos).
    /// @return True if query was dispatched.
    bool queryPosition();

    // --- Telemetry & Subscriptions ---

    /// @brief Retrieves the latest cached gimbal orientation.
    [[nodiscard]] PfecGimbalPosition currentPosition() const;

    /// @brief Registers a subscriber callback for gimbal position updates.
    /// @param[in] cb Callback invoked when a $PFEC,GPpos response is received.
    /// @return Unique subscription ID.
    std::size_t addPositionCallback(PositionCallback cb);

    /// @brief Unregisters a position subscriber callback.
    /// @param[in] id Subscription ID returned by addPositionCallback.
    void removePositionCallback(std::size_t id);

    /// @brief Ingests simulated or raw sentences directly into the accumulator.
    /// @param[in] rawData Raw byte data.
    void feedRawBytes(const std::vector<std::uint8_t>& rawData);

private:
    void handleIncomingBytes(const std::vector<std::uint8_t>& data);
    void handleTransportState(Transport::TransportState state, const std::string& errorMsg);
    void pollerLoop();
    bool sendSentence(const std::string& sentence);

    std::shared_ptr<Transport::ITransport> m_transport;
    NmeaStreamAccumulator m_accumulator;

    mutable std::mutex m_lifecycleMutex;
    std::atomic<bool> m_running { false };

    std::chrono::milliseconds m_pollingInterval { 50 };
    std::thread m_pollerThread {};
    std::condition_variable m_cv {};
    std::mutex m_pollerMutex {};

    mutable std::mutex m_posMutex;
    PfecGimbalPosition m_currentPosition {};

    mutable std::mutex m_callbackMutex;
    std::size_t m_nextCallbackId { 1U };
    std::shared_ptr<const std::vector<std::pair<std::size_t, PositionCallback>>> m_positionCallbacks {
        std::make_shared<std::vector<std::pair<std::size_t, PositionCallback>>>()
    };
};

} // namespace Nmea
