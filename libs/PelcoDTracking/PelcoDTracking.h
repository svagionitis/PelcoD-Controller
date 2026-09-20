/**
 * @file PelcoDTracking.h
 * @brief Umbrella header for the PelcoDTracking subsystem, including state estimation, camera models,
 *        closed-loop PID auto-tracking, latency estimation, and plant dynamics identification.
 */

#pragma once

#include "ChirpCalibrator.h"
#include "ExtendedKalmanFilter.h"
#include "LatencyCalibrator.h"
#include "LatencyEstimator.h"
#include "OscillationDetector.h"
#include "PidController.h"
#include "PlantIdentifier.h"
#include "PtzAutoTracker.h"
#include "PtzCameraModel.h"
#include "PtzSphericalEstimator.h"
#include "TransientShockDetector.h"
#include "UnscentedKalmanFilter.h"

namespace PelcoD {
namespace Tracking {
    // Classes in namespace PelcoD::Tracking
} // namespace Tracking

// Canonical aliases for backward compatibility
using namespace Tracking;
} // namespace PelcoD
