#pragma once

/// @file Stanag4586Bridge.h
/// @brief NATO STANAG 4586 Tactical UAV / C2 DLI Interoperability Bridge.
/// @details Translates NATO STANAG 4586 DLI messages (#2000, #2001, #2002, #2003, #2004)
///          to and from the IPayload polymorphic hardware abstraction layer.

#include "IPayload.h"
#include "PayloadKlvGenerator.h"
#include "Stanag4586Types.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

namespace PayloadHal {

/// @enum StanagParseResult
/// @brief Result status returned when parsing inbound STANAG 4586 DLI packets.
enum class StanagParseResult {
    Success,              ///< Packet parsed and dispatched successfully
    BufferTooShort,       ///< Buffer length smaller than minimum packet size or declared length
    InvalidSync,          ///< Preamble sync bytes do not match 0x4586
    CrcMismatch,          ///< Calculated CRC-16 does not match packet checksum
    UnsupportedMessageId, ///< Message ID is not in the recognized DLI set
    InvalidPayloadSize,   ///< Message length does not match expected payload specification
    StationMismatch,      ///< Message station ID does not target this payload station
    DispatchFailed        ///< HAL rejected the commanded operation
};

/// @struct StanagBridgeConfig
/// @brief Configuration settings for the STANAG 4586 DLI Interoperability Bridge.
struct StanagBridgeConfig {
    std::uint32_t localNodeId { 0x00010001U };    ///< DLI Source ID for outgoing packets
    std::uint32_t remoteC2NodeId { 0x00000001U }; ///< Target C2 / GCS Node ID
    std::uint8_t stationId { 0U };                ///< Payload mounting station index (0..15)
    bool autoRespondToConfigRequests { true };    ///< Auto-reply with #2000 upon receipt of #2001
    bool enablePeriodicTelemetry { false };       ///< Automatically start periodic #2002 reporting
    std::chrono::milliseconds telemetryInterval { 100 }; ///< Default reporting period (100ms = 10Hz)
    PlatformNavData defaultNavData {};            ///< Default host navigation state
    double groundElevationM { 0.0 };              ///< Ground elevation MSL for target projection
};

/// @class Stanag4586Bridge
/// @brief Interoperability bridge connecting NATO STANAG 4586 C2 Ground Control Stations
///        with the IPayload surveillance station.
class Stanag4586Bridge : public std::enable_shared_from_this<Stanag4586Bridge> {
public:
    /// @brief Callback invoked when a serialized outgoing STANAG 4586 packet is generated.
    using PacketTxCallback = std::function<void(const std::vector<std::uint8_t>& packet)>;

    /// @brief Callback invoked when a Message #2000 (Configuration) is produced.
    using Message2000Callback = std::function<void(const Stanag4586Message2000& msg)>;

    /// @brief Callback invoked when a Message #2002 (Operating State) is produced.
    using Message2002Callback = std::function<void(const Stanag4586Message2002& msg)>;

    /// @brief Callback invoked when an inbound Message #2003 (Operating Command) is executed.
    using Message2003Callback = std::function<void(const Stanag4586Message2003& cmd)>;

    /// @brief Callback invoked when an inbound Message #2004 (Steering Command) is executed.
    using Message2004Callback = std::function<void(const Stanag4586Message2004& cmd)>;

    /// @brief Constructs a STANAG 4586 Interoperability Bridge bound to an IPayload station.
    /// @param[in] payload Pointer to the composite IPayload station.
    /// @param[in] config Bridge configuration parameters.
    explicit Stanag4586Bridge(std::shared_ptr<IPayload> payload,
                              StanagBridgeConfig config = {}) noexcept;

    virtual ~Stanag4586Bridge();

    // Disable copy semantics to protect thread and hardware binding
    Stanag4586Bridge(const Stanag4586Bridge&) = delete;
    Stanag4586Bridge& operator=(const Stanag4586Bridge&) = delete;
    Stanag4586Bridge(Stanag4586Bridge&&) noexcept = delete;
    Stanag4586Bridge& operator=(Stanag4586Bridge&&) noexcept = delete;

    // --- Configuration & Host Navigation State ---

    /// @brief Updates the bridge configuration.
    /// @param[in] config New configuration.
    void setConfig(const StanagBridgeConfig& config);

    /// @brief Retrieves the current bridge configuration.
    [[nodiscard]] StanagBridgeConfig config() const;

    /// @brief Updates the host platform navigation telemetry (GPS, heading, pitch, roll).
    /// @param[in] nav Updated platform navigation data.
    void updateNavData(const PlatformNavData& nav);

    /// @brief Retrieves the latest cached platform navigation telemetry.
    [[nodiscard]] PlatformNavData navData() const;

    // --- Packet Ingestion & Dispatching ---

    /// @brief Parses an inbound byte buffer representing a framed STANAG 4586 DLI packet.
    /// @param[in] data Pointer to raw byte data.
    /// @param[in] length Number of bytes in buffer.
    /// @return StanagParseResult indicating parsing and dispatch outcome.
    [[nodiscard]] StanagParseResult processInboundBytes(const std::uint8_t* data, std::size_t length);

    /// @brief Parses an inbound string_view representing a framed STANAG 4586 DLI packet.
    /// @param[in] packet Inbound packet view.
    /// @return StanagParseResult indicating parsing and dispatch outcome.
    [[nodiscard]] StanagParseResult processInboundPacket(std::string_view packet);

    /// @brief Directly ingests and executes Message #2001 (Configuration Request).
    /// @param[in] req Decoded request structure.
    /// @return true if request was valid and responded to, false otherwise.
    bool dispatchMessage2001(const Stanag4586Message2001& req);

    /// @brief Directly ingests and executes Message #2003 (Operating Command).
    /// @param[in] cmd Decoded operating command.
    /// @return true if commanded operation was successfully routed to IPayload, false otherwise.
    bool dispatchMessage2003(const Stanag4586Message2003& cmd);

    /// @brief Directly ingests and executes Message #2004 (Steering Command).
    /// @param[in] cmd Decoded steering command.
    /// @return true if steering command was successfully dispatched to IPayload, false otherwise.
    bool dispatchMessage2004(const Stanag4586Message2004& cmd);

    // --- Outbound Message Serialization ---

    /// @brief Builds a strongly-typed Message #2000 snapshot from the current IPayload station.
    /// @param[in] stationId Station index (defaults to config.stationId).
    /// @return Populated Stanag4586Message2000 structure.
    [[nodiscard]] Stanag4586Message2000 buildMessage2000(std::optional<std::uint8_t> stationId = std::nullopt) const;

    /// @brief Serializes a framed NATO STANAG 4586 packet containing Message #2000.
    /// @param[in] stationId Station index (defaults to config.stationId).
    /// @return Serialized Big-Endian byte vector with DLI header and CRC-16.
    [[nodiscard]] std::vector<std::uint8_t> serializeMessage2000(
        std::optional<std::uint8_t> stationId = std::nullopt) const;

    /// @brief Builds a strongly-typed Message #2002 snapshot from the current IPayload station.
    /// @param[in] nav Optional platform navigation data; if nullopt, cached nav data is used.
    /// @return Populated Stanag4586Message2002 structure.
    [[nodiscard]] Stanag4586Message2002 buildMessage2002(
        std::optional<PlatformNavData> nav = std::nullopt) const;

    /// @brief Serializes a framed NATO STANAG 4586 packet containing Message #2002.
    /// @param[in] nav Optional platform navigation data; if nullopt, cached nav data is used.
    /// @return Serialized Big-Endian byte vector with DLI header and CRC-16.
    [[nodiscard]] std::vector<std::uint8_t> serializeMessage2002(
        std::optional<PlatformNavData> nav = std::nullopt) const;

    // --- Periodic Telemetry Thread ---

    /// @brief Starts background periodic transmission of Message #2002 telemetry reports.
    /// @param[in] interval Period between telemetry updates.
    void startPeriodicTelemetry(std::chrono::milliseconds interval);

    /// @brief Stops the background periodic telemetry transmission thread.
    void stopPeriodicTelemetry();

    /// @brief Checks if the periodic telemetry transmission thread is active.
    [[nodiscard]] bool isPeriodicTelemetryRunning() const noexcept;

    // --- Callbacks ---

    /// @brief Sets callback for transmitting serialized STANAG 4586 DLI packets.
    void setPacketTxCallback(PacketTxCallback cb);

    /// @brief Sets listener callback for generated Message #2000 reports.
    void setMessage2000Callback(Message2000Callback cb);

    /// @brief Sets listener callback for generated Message #2002 reports.
    void setMessage2002Callback(Message2002Callback cb);

    /// @brief Sets listener callback for received Message #2003 commands.
    void setMessage2003Callback(Message2003Callback cb);

    /// @brief Sets listener callback for received Message #2004 commands.
    void setMessage2004Callback(Message2004Callback cb);

    /// @brief Generates monotonically increasing DLI sequence numbers.
    [[nodiscard]] std::uint32_t nextSequenceNumber() noexcept;

private:
    [[nodiscard]] std::vector<std::uint8_t> framePayload(
        StanagMessageId msgId, std::uint8_t stationId, const std::uint8_t* payloadData, std::size_t payloadLen) const;

    void periodicTelemetryLoop();

    std::shared_ptr<IPayload> m_payload {};
    mutable std::mutex m_mutex;
    StanagBridgeConfig m_config {};
    PlatformNavData m_navData {};
    mutable std::atomic<std::uint32_t> m_sequenceNumber { 1U };

    // Periodic telemetry thread
    std::thread m_telemetryThread {};
    std::atomic<bool> m_telemetryRunning { false };

    // Registered callbacks
    PacketTxCallback m_packetTxCallback {};
    Message2000Callback m_msg2000Callback {};
    Message2002Callback m_msg2002Callback {};
    Message2003Callback m_msg2003Callback {};
    Message2004Callback m_msg2004Callback {};
};

} // namespace PayloadHal
