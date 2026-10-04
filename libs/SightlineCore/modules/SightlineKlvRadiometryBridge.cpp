#include "SightlineKlvRadiometryBridge.h"
#include "SightlineKlvBuilder.h"
#include "Klv/KlvEncoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Sightline {

std::string SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor sensor) {
    switch (sensor) {
    case RadiometricSensor::FlirTau2LowRes:
        return "FLIR Tau 2 (TLinear Low)";
    case RadiometricSensor::FlirTau2HighRes:
        return "FLIR Tau 2 (TLinear High)";
    case RadiometricSensor::FlirBosonHighGain:
        return "FLIR Boson (High Gain)";
    case RadiometricSensor::FlirBosonLowGain:
        return "FLIR Boson (Low Gain)";
    case RadiometricSensor::DrsTamarisk:
        return "DRS Tamarisk (Superframe)";
    case RadiometricSensor::CustomLinear:
    default:
        return "Calibrated Thermal IR";
    }
}

Klv::VTargetPack SightlineKlvRadiometryBridge::buildTargetPack(
    const RadiometricSpotStats& stats,
    const TrackCoordinate& track,
    TemperatureScale scale) noexcept {
    Klv::VTargetPack pack {};
    pack.targetId = static_cast<std::uint32_t>(track.trackId);

    const auto colRound = std::round(track.centerCol);
    const auto rowRound = std::round(track.centerRow);
    const auto colVal = static_cast<std::uint32_t>(std::clamp(colRound, 1.0, 65535.0));
    const auto rowVal = static_cast<std::uint32_t>(std::clamp(rowRound, 1.0, 65535.0));
    pack.centroid = Klv::PixelCoord { colVal, rowVal };

    const double halfW = (track.width > 0.0) ? (track.width * 0.5) : 1.0;
    const double halfH = (track.height > 0.0) ? (track.height * 0.5) : 1.0;
    const auto tlCol = static_cast<std::uint32_t>(std::clamp(std::round(track.centerCol - halfW), 1.0, 65535.0));
    const auto tlRow = static_cast<std::uint32_t>(std::clamp(std::round(track.centerRow - halfH), 1.0, 65535.0));
    const auto brCol = static_cast<std::uint32_t>(std::clamp(std::round(track.centerCol + halfW), 1.0, 65535.0));
    const auto brRow = static_cast<std::uint32_t>(std::clamp(std::round(track.centerRow + halfH), 1.0, 65535.0));
    pack.boundingBox = Klv::PixelBoundingBox {
        Klv::PixelCoord { tlCol, tlRow },
        Klv::PixelCoord { brCol, brRow }
    };

    pack.confidence = track.confidence;
    pack.priority = track.isPrimary ? static_cast<std::uint8_t>(1U) : static_cast<std::uint8_t>(2U);
    pack.detectionStatus = track.isCoasting ? static_cast<std::uint8_t>(2U) : static_cast<std::uint8_t>(1U);

    pack.targetIntensity = SightlineRadiometry::fromKelvinToScale(stats.meanTemp.kelvin, scale);

    return pack;
}

Klv::VmtiLocalSet SightlineKlvRadiometryBridge::buildVmtiLocalSet(
    const std::vector<RadiometricSpotStats>& statsList,
    const std::vector<TrackCoordinate>& trackList,
    std::uint32_t frameWidth,
    std::uint32_t frameHeight,
    std::uint64_t timestampUs,
    TemperatureScale scale) noexcept {
    Klv::VmtiLocalSet vmti {};
    if (timestampUs > 0ULL) {
        vmti.precisionTimeStampUs = timestampUs;
    } else {
        vmti.precisionTimeStampUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    vmti.systemName = "Sightline SLA VMTI";
    vmti.version = 6U;
    vmti.frameWidth = frameWidth;
    vmti.frameHeight = frameHeight;

    for (const auto& trk : trackList) {
        const auto it = std::find_if(statsList.begin(), statsList.end(), [&trk](const RadiometricSpotStats& s) {
            return s.trackId == trk.trackId;
        });
        if (it != statsList.end()) {
            vmti.targets.push_back(buildTargetPack(*it, trk, scale));
        } else {
            RadiometricSpotStats dummy {};
            dummy.trackId = trk.trackId;
            dummy.cameraIndex = 0U;
            vmti.targets.push_back(buildTargetPack(dummy, trk, scale));
        }
    }

    vmti.totalTargetsDetected = static_cast<std::uint32_t>(vmti.targets.size());
    vmti.numTargetsReported = static_cast<std::uint32_t>(vmti.targets.size());
    return vmti;
}

Klv::UasDatalinkMessage SightlineKlvRadiometryBridge::buildUasMessage(
    const std::vector<RadiometricSpotStats>& statsList,
    const std::vector<TrackCoordinate>& trackList,
    RadiometricSensor sensor,
    std::uint32_t frameWidth,
    std::uint32_t frameHeight,
    std::uint64_t timestampUs,
    TemperatureScale scale) noexcept {
    Klv::UasDatalinkMessage msg {};
    const auto ts = (timestampUs > 0ULL) ? timestampUs : static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    msg.precisionTimeStampUs = ts;
    msg.imageSourceSensor = sensorModelName(sensor);
    msg.wavelengthBands = toWavelengthMask(sensor);
    msg.vmti = buildVmtiLocalSet(statsList, trackList, frameWidth, frameHeight, ts, scale);
    return msg;
}

MsgSetVmti SightlineKlvRadiometryBridge::buildMsgSetVmti(
    const std::vector<TrackCoordinate>& tracks,
    std::uint16_t displayId) noexcept {
    MsgSetVmti msg {};
    msg.displayId = displayId;
    msg.targets.reserve(tracks.size());

    for (const auto& trk : tracks) {
        VmtiTargetPack tp {};
        tp.targetId = trk.trackId;
        tp.confidence = trk.confidence;
        tp.col = static_cast<std::uint16_t>(std::clamp(std::round(trk.centerCol), 0.0, 65535.0));
        tp.row = static_cast<std::uint16_t>(std::clamp(std::round(trk.centerRow), 0.0, 65535.0));
        tp.width = static_cast<std::uint16_t>(std::clamp(std::round(trk.width), 0.0, 65535.0));
        tp.height = static_cast<std::uint16_t>(std::clamp(std::round(trk.height), 0.0, 65535.0));
        tp.newTargetDetectionFlag = 1U;
        msg.targets.push_back(tp);
    }
    return msg;
}

MsgSetVmti SightlineKlvRadiometryBridge::buildMsgSetVmti(
    const TrackCoordinate& track,
    std::uint16_t displayId) noexcept {
    return buildMsgSetVmti(std::vector<TrackCoordinate>{ track }, displayId);
}

MsgTagData SightlineKlvRadiometryBridge::buildWavelengthTag(
    RadiometricSensor sensor,
    std::uint16_t displayId) noexcept {
    MsgTagData msg {};
    msg.reserved1 = 0U;
    msg.reserved2 = 0U;
    msg.tagId = static_cast<std::uint8_t>(Klv::Tag::WavelengthBands); // 95U
    msg.tagSubId = 0U;
    msg.reservedInternal = 0U;
    msg.displayId = displayId;
    msg.data = { toWavelengthMask(sensor) };
    return msg;
}

MsgSetMetadataFrameValues SightlineKlvRadiometryBridge::buildFrameValues(
    const TrackCoordinate& track,
    double slantRangeM,
    std::uint16_t displayId) noexcept {
    MsgSetMetadataFrameValues msg {};
    msg.validDataMask = 0x001FU; // bits 0..4
    msg.targetTrackGateWidth = static_cast<std::uint8_t>(std::clamp(std::round(track.width), 0.0, 255.0));
    msg.targetTrackGateHeight = static_cast<std::uint8_t>(std::clamp(std::round(track.height), 0.0, 255.0));
    msg.slantRange = static_cast<std::uint32_t>(std::clamp(std::round(slantRangeM), 0.0, 4294967295.0));
    msg.displayId = displayId;
    return msg;
}

std::vector<std::uint8_t> SightlineKlvRadiometryBridge::buildSetVmtiPacket(
    const MsgSetVmti& msg) {
    return SightlineKlvBuilder::buildSetVmti(msg);
}

std::vector<std::uint8_t> SightlineKlvRadiometryBridge::buildTagDataPacket(
    const MsgTagData& msg) {
    return SightlineKlvBuilder::buildTagData(msg);
}

std::vector<std::uint8_t> SightlineKlvRadiometryBridge::buildFrameValuesPacket(
    const MsgSetMetadataFrameValues& msg) {
    return SightlineKlvBuilder::buildSetMetadataFrameValues(msg);
}

std::vector<std::uint8_t> SightlineKlvRadiometryBridge::encodeUasPacket(
    const Klv::UasDatalinkMessage& msg) {
    return Klv::KlvEncoder::encode(msg);
}

} // namespace Sightline
