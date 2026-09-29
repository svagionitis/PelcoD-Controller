#pragma once

/// @file N2kAddressClaimer.h
/// @brief ISO 11783-5 and SAE J1939 compliant dynamic address claiming controller for NMEA 2000.

#include "N2kTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>

namespace Nmea::N2k {

/// @brief Operational state of node address claiming.
enum class AddressClaimState : std::uint8_t {
    Unclaimed = 0U,       ///< Node has not started claiming
    WaitingForClaim = 1U, ///< Address claim broadcast sent; awaiting 250ms contention window
    Claimed = 2U,         ///< Address successfully claimed and established
    CannotClaim = 3U      ///< Address contention lost and no arbitrary addresses available
};

/// @class N2kAddressClaimer
/// @brief Dynamic address claiming state machine conforming to ISO 11783-5 and SAE J1939-81.
class N2kAddressClaimer {
public:
    using CanSendCallback = std::function<void(const CanFrame& frame)>;

    explicit N2kAddressClaimer(std::uint64_t name = 0xC0002046000003E9ULL,
                               std::uint8_t preferredAddress = 0x23U);
    ~N2kAddressClaimer() = default;

    // Non-copyable, non-movable
    N2kAddressClaimer(const N2kAddressClaimer&) = delete;
    N2kAddressClaimer& operator=(const N2kAddressClaimer&) = delete;
    N2kAddressClaimer(N2kAddressClaimer&&) = delete;
    N2kAddressClaimer& operator=(N2kAddressClaimer&&) = delete;

    /// @brief Starts address claiming sequence by transmitting PGN 60928.
    void startClaiming();

    /// @brief Processes an inbound CAN frame to detect address contention or ISO requests.
    /// @param[in] frame Inbound 29-bit CAN frame.
    void processCanFrame(const CanFrame& frame);

    /// @brief Periodic poll to check if the 250ms contention window has elapsed.
    /// @param[in] now Current steady clock time.
    void pollTimer(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// @brief Returns current address claim state.
    [[nodiscard]] AddressClaimState claimState() const noexcept;

    /// @brief Returns the current active claimed address (or 254 if unclaimed/lost).
    [[nodiscard]] std::uint8_t claimedAddress() const noexcept;

    /// @brief Returns the 64-bit NAME of this node.
    [[nodiscard]] std::uint64_t getName() const noexcept;

    /// @brief Sets callback for transmitting outbound CAN frames.
    void setSendCallback(CanSendCallback cb);

    /// @brief Composes 64-bit NAME field from individual parameters.
    [[nodiscard]] static std::uint64_t composeName(std::uint32_t uniqueNumber,
                                                   std::uint16_t manufacturerCode,
                                                   std::uint8_t deviceFunction,
                                                   std::uint8_t deviceClass,
                                                   bool arbitraryAddress = true) noexcept;

private:
    void sendAddressClaim(std::uint8_t address);
    void handleAddressContention(std::uint8_t contendingAddress, std::uint64_t contendingName);

    mutable std::mutex m_mutex {};
    std::uint64_t m_name { 0ULL };
    std::uint8_t m_preferredAddress { 0x23U };
    std::uint8_t m_currentAddress { 0x23U };
    AddressClaimState m_state { AddressClaimState::Unclaimed };
    std::chrono::steady_clock::time_point m_claimStartTime {};
    CanSendCallback m_sendCallback {};
};

} // namespace Nmea::N2k
