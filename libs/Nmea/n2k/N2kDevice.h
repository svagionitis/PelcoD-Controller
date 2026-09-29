#pragma once

#include "N2kDecoder.h"
#include "N2kFastPacketAssembler.h"
#include "N2kTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Nmea::N2k {

/// @class N2kDevice
/// @brief High-level NMEA 2000 controller for CAN bus stream ingestion and telemetry dispatch.
/// @details Ingests 29-bit CAN frames, manages multi-frame Fast Packet reassembly, decodes
///          standard marine navigation, attitude, wind, and AIS PGNs, and notifies registered subscribers.
/// @note Thread-safe. Uses copy-on-write subscription lists to eliminate deadlocks.
class N2kDevice {
public:
    using PositionCallback = std::function<void(const PositionRapid&)>;
    using CogSogCallback = std::function<void(const CogSogRapid&)>;
    using HeadingCallback = std::function<void(const VesselHeading&)>;
    using AttitudeCallback = std::function<void(const Attitude&)>;
    using AisClassACallback = std::function<void(const AisClassAPosition&)>;
    using AisClassBCallback = std::function<void(const AisClassBPosition&)>;
    using WindCallback = std::function<void(const WindData&)>;
    using RawPgnCallback = std::function<void(const N2kMessage&)>;

    /// @brief Constructs an N2kDevice controller.
    N2kDevice();

    /// @brief Destructor.
    virtual ~N2kDevice() = default;

    // Non-copyable, non-movable for safe subscription usage
    N2kDevice(const N2kDevice&) = delete;
    N2kDevice& operator=(const N2kDevice&) = delete;
    N2kDevice(N2kDevice&&) = delete;
    N2kDevice& operator=(N2kDevice&&) = delete;

    // --- Stream Ingestion ---

    /// @brief Ingests a single CAN frame (e.g. from SocketCAN or hardware bus).
    /// @param[in] frame CAN frame with 29-bit ID and payload.
    void onCanFrame(const CanFrame& frame);

    /// @brief Ingests multiple CAN frames in bulk.
    /// @param[in] frames Array of CAN frames.
    /// @param[in] count Number of frames.
    void onCanFrames(const CanFrame* frames, std::size_t count);

    /// @brief Ingests raw SocketCAN struct can_frame byte sequence (16 bytes per frame).
    /// @param[in] bytes Raw byte buffer.
    /// @param[in] len Length of buffer in bytes.
    void onRawSocketCanData(const std::uint8_t* bytes, std::size_t len);

    /// @brief Ingests an already completed/reassembled N2kMessage directly.
    /// @param[in] msg Reassembled N2K message.
    void onN2kMessage(const N2kMessage& msg);

    // --- Subscription Management (Copy-On-Write) ---

    /// @brief Subscribes to PGN 129025 Rapid Position updates.
    [[nodiscard]] std::size_t addPositionCallback(PositionCallback cb);
    void removePositionCallback(std::size_t id);

    /// @brief Subscribes to PGN 129026 COG & SOG updates.
    [[nodiscard]] std::size_t addCogSogCallback(CogSogCallback cb);
    void removeCogSogCallback(std::size_t id);

    /// @brief Subscribes to PGN 127250 Vessel Heading updates.
    [[nodiscard]] std::size_t addHeadingCallback(HeadingCallback cb);
    void removeHeadingCallback(std::size_t id);

    /// @brief Subscribes to PGN 127257 Attitude updates.
    [[nodiscard]] std::size_t addAttitudeCallback(AttitudeCallback cb);
    void removeAttitudeCallback(std::size_t id);

    /// @brief Subscribes to PGN 129038 AIS Class A position reports.
    [[nodiscard]] std::size_t addAisClassACallback(AisClassACallback cb);
    void removeAisClassACallback(std::size_t id);

    /// @brief Subscribes to PGN 129039 AIS Class B position reports.
    [[nodiscard]] std::size_t addAisClassBCallback(AisClassBCallback cb);
    void removeAisClassBCallback(std::size_t id);

    /// @brief Subscribes to PGN 130306 Wind Data updates.
    [[nodiscard]] std::size_t addWindCallback(WindCallback cb);
    void removeWindCallback(std::size_t id);

    /// @brief Subscribes to arbitrary raw PGN messages.
    [[nodiscard]] std::size_t addPgnCallback(std::uint32_t pgn, RawPgnCallback cb);
    void removePgnCallback(std::size_t id);

    // --- Telemetry Cache Accessors ---

    [[nodiscard]] std::optional<PositionRapid> position() const;
    [[nodiscard]] std::optional<CogSogRapid> cogSog() const;
    [[nodiscard]] std::optional<VesselHeading> heading() const;
    [[nodiscard]] std::optional<Attitude> attitude() const;
    [[nodiscard]] std::optional<WindData> wind() const;
    [[nodiscard]] std::optional<AisClassAPosition> aisClassATarget(std::uint32_t mmsi) const;
    [[nodiscard]] std::optional<AisClassBPosition> aisClassBTarget(std::uint32_t mmsi) const;

    /// @brief Returns the total count of currently tracked AIS targets.
    [[nodiscard]] std::size_t aisTargetCount() const;

    /// @brief Prunes AIS targets older than the specified TTL.
    void pruneAisTargets(std::chrono::seconds ttl = std::chrono::seconds(180));

    /// @brief Returns reference to internal Fast Packet assembler.
    [[nodiscard]] N2kFastPacketAssembler& assembler() noexcept
    {
        return m_assembler;
    }

private:
    template <typename T> using CallbackList = std::shared_ptr<const std::vector<std::pair<std::size_t, T>>>;

    template <typename T> [[nodiscard]] std::size_t registerCallback(CallbackList<T>& list, T cb);

    template <typename T> void unregisterCallback(CallbackList<T>& list, std::size_t id);

    N2kFastPacketAssembler m_assembler {};

    // Telemetry state mutex
    mutable std::mutex m_stateMutex {};
    std::optional<PositionRapid> m_latestPosition {};
    std::optional<CogSogRapid> m_latestCogSog {};
    std::optional<VesselHeading> m_latestHeading {};
    std::optional<Attitude> m_latestAttitude {};
    std::optional<WindData> m_latestWind {};

    struct TrackedAisA {
        AisClassAPosition data {};
        std::chrono::steady_clock::time_point timestamp {};
    };
    struct TrackedAisB {
        AisClassBPosition data {};
        std::chrono::steady_clock::time_point timestamp {};
    };
    std::unordered_map<std::uint32_t, TrackedAisA> m_aisClassATargets {};
    std::unordered_map<std::uint32_t, TrackedAisB> m_aisClassBTargets {};

    // Callback mutex & lists
    mutable std::mutex m_callbackMutex {};
    std::size_t m_nextSubscriptionId { 1U };
    CallbackList<PositionCallback> m_posCallbacks {};
    CallbackList<CogSogCallback> m_cogSogCallbacks {};
    CallbackList<HeadingCallback> m_headingCallbacks {};
    CallbackList<AttitudeCallback> m_attitudeCallbacks {};
    CallbackList<AisClassACallback> m_aisACallbacks {};
    CallbackList<AisClassBCallback> m_aisBCallbacks {};
    CallbackList<WindCallback> m_windCallbacks {};

    struct PgnSubscription {
        std::size_t id { 0U };
        std::uint32_t pgn { 0U };
        RawPgnCallback cb {};
    };
    std::shared_ptr<const std::vector<PgnSubscription>> m_pgnCallbacks {};
};

} // namespace Nmea::N2k
