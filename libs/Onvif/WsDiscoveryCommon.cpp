/// @file WsDiscoveryCommon.cpp
/// @brief Implementation of shared WS-Discovery UUID generator and AppSequence tracker.

#include "WsDiscoveryCommon.h"

#include <cstdint>

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace Onvif {

std::string generateRandomUuid()
{
    std::random_device rd {};
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint32_t> dis(0, 0xFFFFFFFF);

    const std::uint32_t d1 = dis(gen);
    const std::uint16_t d2 = static_cast<std::uint16_t>(dis(gen) & 0xFFFF);
    const std::uint16_t d3 = static_cast<std::uint16_t>((dis(gen) & 0x0FFF) | 0x4000); // version 4
    const std::uint16_t d4 = static_cast<std::uint16_t>((dis(gen) & 0x3FFF) | 0x8000); // variant 1
    const std::uint32_t d5a = dis(gen);
    const std::uint16_t d5b = static_cast<std::uint16_t>(dis(gen) & 0xFFFF);

    std::ostringstream ss {};
    ss << std::hex << std::setfill('0') << std::setw(8) << d1 << '-' << std::setw(4) << d2 << '-' << std::setw(4) << d3
       << '-' << std::setw(4) << d4 << '-' << std::setw(8) << d5a << std::setw(4) << d5b;
    return ss.str();
}

AppSequenceTracker::AppSequenceTracker() noexcept
{
    const auto now = std::chrono::system_clock::now();
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    m_instanceId = static_cast<std::uint32_t>(sec & 0xFFFFFFFF);
    if (m_instanceId == 0U) {
        m_instanceId = 1U;
    }
}

AppSequenceTracker::AppSequenceTracker(std::uint32_t instanceId) noexcept
    : m_instanceId(instanceId == 0U ? 1U : instanceId)
{
}

std::uint32_t AppSequenceTracker::nextMessageNum() noexcept
{
    return m_messageNumber.fetch_add(1U, std::memory_order_relaxed);
}

std::uint32_t AppSequenceTracker::instanceId() const noexcept
{
    return m_instanceId;
}

std::string AppSequenceTracker::formatHeader()
{
    const auto num = nextMessageNum();
    std::ostringstream ss {};
    ss << "    <d:AppSequence InstanceId=\"" << m_instanceId << "\" MessageNumber=\"" << num << "\"/>\r\n";
    return ss.str();
}

} // namespace Onvif
