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
    // Canonical namespace for Fujinon camera extensions
} // namespace Fujinon

// Aliases for backward compatibility
using namespace Fujinon;
} // namespace PelcoD
