/// @file N2kAddressClaimer.cpp
/// @brief Implementation of ISO 11783-5 and SAE J1939-81 Dynamic Address Claiming for NMEA 2000.

#include "N2kAddressClaimer.h"

#include <cstring>

namespace Nmea::N2k {

N2kAddressClaimer::N2kAddressClaimer(std::uint64_t name, std::uint8_t preferredAddress)
    : m_name { name }
    , m_preferredAddress { preferredAddress }
    , m_currentAddress { preferredAddress }
{
}

void N2kAddressClaimer::startClaiming()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_state = AddressClaimState::WaitingForClaim;
    m_currentAddress = m_preferredAddress;
    m_claimStartTime = std::chrono::steady_clock::now();
    sendAddressClaim(m_currentAddress);
}

void N2kAddressClaimer::processCanFrame(const CanFrame& frame)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const N2kHeader hdr = N2kHeader::fromCanId(frame.id);

    // 1. Handle ISO Request (PGN 59904)
    if (hdr.pgn == static_cast<std::uint32_t>(Pgn::IsoRequest) && frame.dlc >= 3U) {
        const auto reqPgn = static_cast<std::uint32_t>(frame.data[0]) |
                            (static_cast<std::uint32_t>(frame.data[1]) << 8U) |
                            (static_cast<std::uint32_t>(frame.data[2]) << 16U);

        if (reqPgn == static_cast<std::uint32_t>(Pgn::IsoAddressClaim)) {
            const std::uint8_t addrToSend = (m_state == AddressClaimState::Claimed ||
                                             m_state == AddressClaimState::WaitingForClaim)
                                                ? m_currentAddress
                                                : 254U;
            sendAddressClaim(addrToSend);
        }
        return;
    }

    // 2. Handle ISO Commanded Address (PGN 65240)
    if (hdr.pgn == static_cast<std::uint32_t>(Pgn::IsoCommandedAddress) && frame.dlc >= 8U) {
        std::uint64_t targetName { 0ULL };
        for (std::size_t i { 0U }; i < 8U; ++i) {
            targetName |= (static_cast<std::uint64_t>(frame.data[i]) << (i * 8U));
        }
        if (targetName == m_name) {
            // Commanded to switch address (if 9th byte provided or in subsequent byte)
            const std::uint8_t newAddr = (frame.dlc >= 9U) ? frame.data[8] : 0x24U;
            m_currentAddress = newAddr;
            m_state = AddressClaimState::WaitingForClaim;
            m_claimStartTime = std::chrono::steady_clock::now();
            sendAddressClaim(m_currentAddress);
        }
        return;
    }

    // 3. Handle Address Contention (PGN 60928)
    if (hdr.pgn == static_cast<std::uint32_t>(Pgn::IsoAddressClaim)) {
        if (hdr.sourceAddress == 254U) {
            return; // Null address; no contention
        }

        if (hdr.sourceAddress == m_currentAddress &&
            (m_state == AddressClaimState::Claimed || m_state == AddressClaimState::WaitingForClaim)) {
            std::uint64_t remoteName { 0ULL };
            for (std::size_t i { 0U }; i < 8U; ++i) {
                remoteName |= (static_cast<std::uint64_t>(frame.data[i]) << (i * 8U));
            }
            handleAddressContention(hdr.sourceAddress, remoteName);
        }
    }
}

void N2kAddressClaimer::pollTimer(std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (m_state == AddressClaimState::WaitingForClaim) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_claimStartTime).count();
        if (elapsed >= 250) {
            m_state = AddressClaimState::Claimed;
        }
    }
}

AddressClaimState N2kAddressClaimer::claimState() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_state;
}

std::uint8_t N2kAddressClaimer::claimedAddress() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (m_state == AddressClaimState::Claimed) {
        return m_currentAddress;
    }
    return 254U;
}

std::uint64_t N2kAddressClaimer::getName() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_name;
}

void N2kAddressClaimer::setSendCallback(CanSendCallback cb)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_sendCallback = std::move(cb);
}

std::uint64_t N2kAddressClaimer::composeName(std::uint32_t uniqueNumber,
                                             std::uint16_t manufacturerCode,
                                             std::uint8_t deviceFunction,
                                             std::uint8_t deviceClass,
                                             bool arbitraryAddress) noexcept
{
    std::uint64_t name { 0ULL };
    name |= (static_cast<std::uint64_t>(uniqueNumber) & 0x1FFFFFULL);
    name |= ((static_cast<std::uint64_t>(manufacturerCode) & 0x7FFULL) << 21U);
    name |= ((static_cast<std::uint64_t>(deviceFunction) & 0xFFULL) << 40U);
    name |= ((static_cast<std::uint64_t>(deviceClass) & 0x7FULL) << 49U);
    name |= (static_cast<std::uint64_t>(4ULL) << 60U); // Industry Group = 4 (Marine)
    if (arbitraryAddress) {
        name |= (1ULL << 63U); // Arbitrary Address Capable (bit 63)
    }
    return name;
}

void N2kAddressClaimer::sendAddressClaim(std::uint8_t address)
{
    N2kHeader hdr {};
    hdr.priority = 6U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::IsoAddressClaim);
    hdr.sourceAddress = address;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    for (std::size_t i { 0U }; i < 8U; ++i) {
        frame.data[i] = static_cast<std::uint8_t>((m_name >> (i * 8U)) & 0xFFU);
    }

    if (m_sendCallback) {
        m_sendCallback(frame);
    }
}

void N2kAddressClaimer::handleAddressContention(std::uint8_t /*contendingAddress*/, std::uint64_t contendingName)
{
    if (m_name < contendingName) {
        // Local node wins arbitration (lower numeric 64-bit NAME takes priority)
        sendAddressClaim(m_currentAddress);
    } else {
        // Local node loses arbitration
        const bool arbitraryCapable = ((m_name >> 63U) & 0x01ULL) != 0ULL;
        if (arbitraryCapable) {
            // Select next candidate address in 128..247 range
            if (m_currentAddress >= 128U && m_currentAddress < 247U) {
                ++m_currentAddress;
            } else {
                m_currentAddress = 128U;
            }
            m_state = AddressClaimState::WaitingForClaim;
            m_claimStartTime = std::chrono::steady_clock::now();
            sendAddressClaim(m_currentAddress);
        } else {
            // Cannot claim address; announce null address 254
            m_state = AddressClaimState::CannotClaim;
            sendAddressClaim(254U);
        }
    }
}

} // namespace Nmea::N2k
