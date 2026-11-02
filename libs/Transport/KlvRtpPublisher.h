#pragma once

/// @file KlvRtpPublisher.h
/// @brief RFC 6597 Standalone KLV over RTP network publisher for STANAG 4609 Profile 2.
/// @see IETF RFC 6597 "RTP Payload Format for SMPTE ST 336 KLV Data"
/// @see IETF RFC 3550 "RTP: A Transport Protocol for Real-Time Applications"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "KlvRtpTypes.h"

namespace Transport {

/// @class KlvRtpPublisher
/// @brief Publishes SMPTE ST 336 KLV frames directly over UDP/RTP per RFC 6597.
/// @details Supports dynamic MTU segmentation, monotonic 90 kHz RTP timestamping,
///          unicast and multicast IP routing, and zero-allocation socket transmission.
class KlvRtpPublisher {
public:
    /// @brief Constructs KLV RTP publisher with configuration settings.
    /// @param[in] config Initial network and RTP configuration.
    explicit KlvRtpPublisher(KlvRtpConfig config = {});

    /// @brief Destructor closing active socket descriptors.
    ~KlvRtpPublisher();

    KlvRtpPublisher(const KlvRtpPublisher&) = delete;
    KlvRtpPublisher& operator=(const KlvRtpPublisher&) = delete;
    KlvRtpPublisher(KlvRtpPublisher&&) noexcept;
    KlvRtpPublisher& operator=(KlvRtpPublisher&&) noexcept;

    /// @brief Opens UDP socket and configures multicast parameters if applicable.
    /// @return True if socket was created and destination initialized.
    [[nodiscard]] bool start();

    /// @brief Closes active network socket and resets sequence counters.
    void stop();

    /// @brief Returns whether the socket is active and ready to transmit.
    /// @return True if publisher is running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Packages and transmits a SMPTE ST 336 KLV frame over RTP.
    /// @param[in] klvData Pointer to raw KLV bytes.
    /// @param[in] size Length of KLV frame in bytes.
    /// @param[in] timestampUs Presentation timestamp in microseconds since epoch.
    /// @return True if all generated RTP packets were transmitted successfully.
    [[nodiscard]] bool sendKlvPacket(const std::uint8_t* klvData, std::size_t size, std::uint64_t timestampUs);

    /// @brief Returns current operational statistics snapshot.
    [[nodiscard]] KlvRtpStats stats() const noexcept;

    /// @brief Resets transmission statistics counters.
    void resetStats() noexcept;

    /// @brief Retrieves the active configuration.
    [[nodiscard]] KlvRtpConfig config() const;

    /// @brief Reconfigures network publisher dynamically.
    /// @param[in] config New configuration parameters.
    /// @return True if configuration was applied successfully.
    [[nodiscard]] bool setConfig(const KlvRtpConfig& config);

    /// @brief Generates RFC 4566 Session Description Protocol text for this stream.
    /// @return Formatted SDP text descriptor.
    [[nodiscard]] std::string generateSdp() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Transport
