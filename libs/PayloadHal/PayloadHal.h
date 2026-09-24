#pragma once

/// @file PayloadHal.h
/// @brief Master aggregate header for the Payload Hardware Abstraction Layer (PayloadHal).

#include "PayloadTypes.h"
#include "IDevice.h"
#include "IPanTiltUnit.h"
#include "ILaserRangeFinder.h"
#include "ICameraPayload.h"
#include "IPayload.h"
#include "GeoreferenceUtils.h"
#include "PayloadFactory.h"
#include "adapters/PelcoDPtzAdapter.h"
#include "adapters/PelcoDFujinonPayloadAdapter.h"
#include "adapters/ViscaSonyAdapter.h"
#include "sim/SimulatedPayload.h"
