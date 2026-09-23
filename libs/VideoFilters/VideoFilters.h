/**
 * @file VideoFilters.h
 * @brief Aggregate umbrella header including all computer vision and tactical video filter category modules.
 */

#pragma once

#include "ColorFilters.h"
#include "GeometricFilters.h"
#include "OverlayFilters.h"
#include "SpatialFilters.h"
#include "StreamHealthOsdFilter.h"
#include "ThermalFilters.h"
#include "TrackingFilters.h"

#if defined(PELCOD_HAS_FILTERS)

namespace Video {
using namespace Filters;
} // namespace Video

#endif // PELCOD_HAS_FILTERS
