/**
 * @file PelcoDFujinon.h
 * @brief Umbrella header for Fujinon SX800 / SX801 specialized camera protocol extensions.
 */

#pragma once

#include "FujinonBuilder.h"
#include "FujinonParser.h"
#include "FujinonSX800Device.h"
#include "FujinonTypes.h"

namespace PelcoD {
namespace Fujinon {
    // Canonical namespace aliases for Fujinon camera extensions
    using PelcoD::FujinonBuilder;
    using PelcoD::FujinonParser;
    using PelcoD::FujinonStatus;
    using PelcoD::FujinonSX800Device;
} // namespace Fujinon
} // namespace PelcoD
