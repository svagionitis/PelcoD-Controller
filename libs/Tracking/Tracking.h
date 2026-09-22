/**
 * @file Tracking.h
 * @brief Umbrella header for the protocol-agnostic Tracking subsystem, including state estimation,
 *        camera models, closed-loop PID auto-tracking, latency estimation, and plant dynamics identification.
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
