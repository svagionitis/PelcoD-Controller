/**
 * @file VideoFilters.h
 * @brief Aggregate umbrella header including all computer vision and tactical video filter category modules.
 */

#pragma once

#include "ColorFilters.h"
#include "SpatialFilters.h"
#include "GeometricFilters.h"
#include "ThermalFilters.h"
#include "TrackingFilters.h"
#include "OverlayFilters.h"

#if defined(PELCOD_HAS_FILTERS)

namespace PelcoD::Video {
using namespace Filters;
} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS
