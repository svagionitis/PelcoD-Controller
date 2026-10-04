#pragma once

/// @file SightlineMessages.h
/// @brief Umbrella header aggregating all Sightline SLA functional modules.
/// @details Conforms 1-to-1 with the official Sightline Command & Control documentation:
/// https://knowledge.sightlineintelligence.com/releases/IDD/current/modules.html

#include "SightlineTypes.h"

// 1-to-1 official Sightline functional modules
#include "modules/SightlineBlending.h"
#include "modules/SightlineCapture.h"
#include "modules/SightlineClassification.h"
#include "modules/SightlineCompression.h"
#include "modules/SightlineDetection.h"
#include "modules/SightlineDisplay.h"
#include "modules/SightlineEnhancement.h"
#include "modules/SightlineFocus.h"
#include "modules/SightlineGeneral.h"
#include "modules/SightlineKlv.h"
#include "modules/SightlineLanding.h"
#include "modules/SightlineNetwork.h"
#include "modules/SightlineNuc.h"
#include "modules/SightlineOverlay.h"
#include "modules/SightlineRecording.h"
#include "modules/SightlineSerial.h"
#include "modules/SightlineStabilization.h"
#include "modules/SightlineTelemetry.h"
#include "modules/SightlineTracking.h"
#include "modules/SightlineRadiometry.h"
#include "modules/SightlineIsothermBuilder.h"
