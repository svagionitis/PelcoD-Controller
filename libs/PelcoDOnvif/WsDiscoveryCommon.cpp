/// @file WsDiscoveryCommon.cpp
/// @brief Implementation of shared WS-Discovery UUID generator.

#include "WsDiscoveryCommon.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace PelcoD::Onvif {

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

} // namespace PelcoD::Onvif
