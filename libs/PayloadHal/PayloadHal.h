#pragma once

/// @file PayloadHal.h
/// @brief Master aggregate header for the Payload Hardware Abstraction Layer (PayloadHal).

#include "DemRayCaster.h"
#include "GeoLockController.h"
#include "GeoreferenceUtils.h"
#include "GridDemProvider.h"
#include "ICameraPayload.h"
#include "IDemProvider.h"
#include "IDevice.h"
#include "ILaserIlluminator.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "PayloadAutoTrackerBridge.h"
#include "PayloadFactory.h"
#include "PayloadKlvGenerator.h"
#include "PayloadTypes.h"
#include "adapters/OnvifPayloadAdapter.h"
#include "adapters/PelcoDFujinonPayloadAdapter.h"
#include "adapters/PelcoDPtzAdapter.h"
#include "adapters/PelcoDViscaCompositePayload.h"
#include "adapters/ViscaSonyAdapter.h"
#include "sim/SimulatedPayload.h"
