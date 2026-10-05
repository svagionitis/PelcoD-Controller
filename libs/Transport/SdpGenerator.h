#pragma once

/// @file SdpGenerator.h
/// @brief RFC 4566 Session Description Protocol (SDP) file generator and parser.
/// @see https://datatracker.ietf.org/doc/html/rfc4566
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-RTSP.pdf (Sections 7, 8, 11)

#include <cstdint>
#include <string>
#include <string_view>

namespace Transport {

/// @enum SdpPayloadType
/// @brief RTP and elementary stream payload types per RFC 3551 / RFC 6184 / RFC 2250.
enum class SdpPayloadType : std::uint8_t {
    Mpeg2Ts = 33U, ///< Static payload type 33: MPEG2-TS (RFC 2250)
    Mjpeg = 26U, ///< Static payload type 26: Motion JPEG (RFC 2435)
    H264 = 96U, ///< Dynamic payload type 96: H.264 AVC (RFC 6184)
    H265 = 97U, ///< Dynamic payload type 97: H.265 HEVC (RFC 7798)
    Custom = 0U
};

/// @enum SdpProtocol
/// @brief Media transport protocol layer in SDP media descriptions.
enum class SdpProtocol : std::uint8_t {
    RtpAvp = 0U, ///< RTP/AVP (RTP profile for audio and video per RFC 3551)
    Udp = 1U ///< Direct raw UDP transport
};

/// @struct SdpSessionParams
/// @brief Parameters describing a unicast or multicast video streaming session.
struct SdpSessionParams {
    std::string sessionName { "Sightline SLA Video Stream" }; ///< Session title (s=)
    std::string originAddress { "127.0.0.1" }; ///< Creator IP address (o=)
    std::uint64_t sessionId { 0U }; ///< Unique session identifier (o=)
    std::string destinationIp { "127.0.0.1" }; ///< Target unicast IP or multicast group (c=)
    std::uint16_t destinationPort { 15004U }; ///< Outbound video UDP/RTP port (m=)
    std::uint8_t ttl { 16U }; ///< Time-to-Live for multicast streams
    bool isMulticast { false }; ///< Multicast session flag
    SdpPayloadType payloadType { SdpPayloadType::Mpeg2Ts }; ///< Stream encoding payload
    SdpProtocol transportProtocol { SdpProtocol::RtpAvp }; ///< Transport protocol
    std::uint32_t clockRate { 90000U }; ///< RTP timestamp clock rate (default 90kHz)
    int bitrateKbps { 0 }; ///< Target bandwidth in kbps (b=AS:<kbps>, 0 to omit)
    std::string spropParams {}; ///< Base64 SPS/PPS parameters for H.264/H.265
    int frameWidth { 0 }; ///< Video frame width in pixels (optional telemetry)
    int frameHeight { 0 }; ///< Video frame height in pixels (optional telemetry)
};

/// @brief Type alias for session stream parameters.
using SdpStreamParams = SdpSessionParams;

/// @class SdpGenerator
/// @brief Generates and parses RFC 4566 Session Description Protocol (.sdp) descriptors.
class SdpGenerator {
public:
    /// @brief Generates standard RFC 4566 formatted SDP text.
    /// @param[in] params Session configuration parameters.
    /// @return Formatted SDP text string with CRLF line terminators.
    [[nodiscard]] static std::string generate(const SdpSessionParams& params);

    /// @brief Writes formatted SDP text directly to a file on disk.
    /// @param[in] path Target file path.
    /// @param[in] params Session configuration parameters.
    /// @return True if file was successfully written.
    [[nodiscard]] static bool saveToFile(std::string_view path, const SdpSessionParams& params);

    /// @brief Parses an RFC 4566 SDP text descriptor into session parameters.
    /// @param[in] sdpContent Raw SDP text content.
    /// @return Extracted SdpSessionParams.
    [[nodiscard]] static SdpSessionParams parse(std::string_view sdpContent);

    /// @brief Checks whether the given IPv4 address resides in the multicast range (224.0.0.0 - 239.255.255.255).
    /// @param[in] ip IPv4 dotted-decimal string.
    /// @return True if IP is multicast.
    [[nodiscard]] static bool isMulticast(std::string_view ip) noexcept;
};

} // namespace Transport
