#pragma once

/// @file SightlineClassification.h
/// @brief Sightline SLA Classification Module (AI & Deep learning classifiers).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__classification.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-Detection-Modes.pdf

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

/// @enum PretrainedClassifierModel
/// @brief Sightline official pre-trained deep learning classifier models (EAN Sec 4.2).
enum class PretrainedClassifierModel : std::uint8_t {
    None = 0U, ///< No classifier model active
    Drone = 1U, ///< sla_drone.cls (75k drone / 75k non-drone, 10x10 min)
    DroneLarge = 2U, ///< sla_drone_large.cls (96k drone / 140k non-drone)
    VehiclePerson = 3U, ///< sla_vehicle_person.cls (26k vehicle, 11k person, 19k background)
    Custom = 4U ///< Custom user-trained model (.cls or .slaod)
};

/// @enum DroneReportingMode
/// @brief Drone classifier reporting granularity (EAN Sec 4.4.1).
enum class DroneReportingMode : std::uint8_t {
    Standard = 0U, ///< Reports Drone or Background
    Detailed = 1U ///< Reports Drone, Fixed Wing Drone, Background, or Vehicle
};

/// @struct MsgCustomAIDetect
/// @brief AI inference engine configuration and model execution (Message ID 0xBA).
struct MsgCustomAIDetect {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t modelId { 0U };
    std::uint8_t confidenceThreshold { 50U };
    std::uint8_t nmsThreshold { 45U };
};

/// @struct MsgVMTIChips
/// @brief Extracted thumbnail chips of moving detections for downlinks (Message ID 0xAD).
struct MsgVMTIChips {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t chipIndex { 0U };
    std::uint8_t totalChips { 1U };
    std::uint16_t chipWidth { 0U };
    std::uint16_t chipHeight { 0U };
    std::vector<std::uint8_t> chipData {};
};

/// @struct MsgVMTIFields
/// @brief Configures MISB ST 0903 VMTI metadata field insertion into KLV streams (Message ID 0xBF).
struct MsgVMTIFields {
    std::uint8_t cameraIndex { 0U };
    std::uint32_t enabledFieldsMask { 0xFFFFFFFFU };
    std::uint8_t reportRate { 1U };
};

/// @struct MsgKlvClassFilters
/// @brief Filter criteria dictating detected object classes generating KLV (Message ID 0xC1).
struct MsgKlvClassFilters {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t classMask { 0xFFFFU };
    std::uint8_t minConfidence { 50U };
};

/// @struct MsgKlvMetricFilters
/// @brief Target metric dimensions and spatial horizon filters via KLV metadata (EAN Sec 4.4.4).
/// @details Supported in Sightline firmware 3.11.x and above.
struct MsgKlvMetricFilters {
    std::uint8_t cameraIndex { 0U };
    float minTargetWidthM { 0.0F }; ///< Minimum target width in meters (0.0 .. 200.0)
    float maxTargetWidthM { 200.0F }; ///< Maximum target width in meters (0.0 .. 200.0)
    float minTargetHeightM { 0.0F }; ///< Minimum target height in meters (0.0 .. 200.0)
    float maxTargetHeightM { 200.0F }; ///< Maximum target height in meters (0.0 .. 200.0)
    bool filterAboveHorizon { false }; ///< Exclude detections located above scene horizon
    bool filterBelowHorizon { false }; ///< Exclude detections located below scene horizon
    double minLatitude { -90.0 }; ///< Geographic bounding box minimum latitude
    double maxLatitude { 90.0 }; ///< Geographic bounding box maximum latitude
    double minLongitude { -180.0 }; ///< Geographic bounding box minimum longitude
    double maxLongitude { 180.0 }; ///< Geographic bounding box maximum longitude
};

/// @struct MsgClassifierConfig
/// @brief Comprehensive classifier configuration and compute assignment (EAN Sec 4.1, 4.5).
struct MsgClassifierConfig {
    std::uint8_t cameraIndex { 0U };
    PretrainedClassifierModel model { PretrainedClassifierModel::None };
    std::string customModelName {};
    std::uint8_t maxPerFrame { 3U }; ///< Max classifications per frame (1..10)
    std::uint16_t minDimensions { 10U }; ///< Minimum target pixel dimension (0..1000)
    DroneReportingMode droneReporting { DroneReportingMode::Standard };
    std::uint8_t detectionPadding { 4U }; ///< Padding pixels around detection for classification
    std::uint8_t updateRate { 3U }; ///< History update rate incorporating classifications
    bool useNpu { true }; ///< Force inference onto NPU (NPU_CONTROL system value)
    bool asyncExecution { true }; ///< Run asynchronously to avoid frame drops (CLASSIFY_ASYNC)
};

/// @struct MsgTrackingMultiClass
/// @brief Deep learning multi-class categorization telemetry (Message ID 0xBD).
struct MsgTrackingMultiClass {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t primaryClass { 0U };
    std::uint8_t confidence { 0U };
    std::uint8_t flags { 0U };
};

/// @struct MsgCustomClassifier
/// @brief Custom neural network classifier configuration (Message ID 0xA7).
struct MsgCustomClassifier {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t classifierType { 0U };
    std::uint8_t enable { 0U };
    std::uint8_t confidence { 50U };
};

/// @struct MsgClassifierParameters
/// @brief Classifier execution bounds and non-max suppression thresholds (Message ID 0xA9).
struct MsgClassifierParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t modelIndex { 0U };
    std::uint8_t nmsThreshold { 45U };
    std::uint16_t maxDetections { 100U };
};

} // namespace Sightline
