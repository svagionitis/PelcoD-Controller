/**
 * @file PelcoDSim.h
 * @brief Umbrella header for Pelco-D virtual device emulation and kinematics simulation.
 */

#pragma once

#include "KinematicsSimulator.h"
#include "LatencyPipeline.h"
#include "MockPelcoDDevice.h"

namespace PelcoD {
namespace Sim {
    // Canonical namespace for simulation and mock devices
    using PelcoD::KinematicsConfig;
    using PelcoD::MotionMode;
    using PelcoD::KinematicsSimulator;
    using PelcoD::LatencyProfile;
    using PelcoD::LatencyPipeline;
    using PelcoD::PresetPosition;
    using PelcoD::MockDeviceState;
    using PelcoD::MockPelcoDDevice;
} // namespace Sim
} // namespace PelcoD
