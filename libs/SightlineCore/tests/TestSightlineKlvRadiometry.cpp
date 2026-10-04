#include "modules/SightlineKlvRadiometryBridge.h"
#include "modules/SightlineRadiometry.h"
#include "modules/SightlineDetectionParser.h"
#include "Klv/KlvEncoder.h"
#include "Klv/KlvParser.h"
#include "Klv/VmtiEncoder.h"
#include "Klv/VmtiParser.h"

#include <gtest/gtest.h>
#include <vector>

using namespace Sightline;

TEST(TestSightlineKlvRadiometry, WavelengthBandMapping) {
    EXPECT_EQ(SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor::FlirTau2LowRes), 0x10U);
    EXPECT_EQ(SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor::FlirTau2HighRes), 0x10U);
    EXPECT_EQ(SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor::FlirBosonHighGain), 0x10U);
    EXPECT_EQ(SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor::FlirBosonLowGain), 0x10U);
    EXPECT_EQ(SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor::DrsTamarisk), 0x10U);
    EXPECT_EQ(SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor::CustomLinear), 0x10U);

    EXPECT_EQ(SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor::FlirTau2LowRes), "FLIR Tau 2 (TLinear Low)");
    EXPECT_EQ(SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor::FlirTau2HighRes), "FLIR Tau 2 (TLinear High)");
    EXPECT_EQ(SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor::FlirBosonHighGain), "FLIR Boson (High Gain)");
    EXPECT_EQ(SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor::FlirBosonLowGain), "FLIR Boson (Low Gain)");
    EXPECT_EQ(SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor::DrsTamarisk), "DRS Tamarisk (Superframe)");
    EXPECT_EQ(SightlineKlvRadiometryBridge::sensorModelName(RadiometricSensor::CustomLinear), "Calibrated Thermal IR");
}

TEST(TestSightlineKlvRadiometry, TargetPackRadiometryConversion) {
    RadiometricSpotStats stats {};
    stats.trackId = 5U;
    stats.cameraIndex = 0U;
    stats.meanTemp = SightlineRadiometry::fromCelsius(37.0F); // 310.15 K
    stats.minTemp = SightlineRadiometry::fromCelsius(35.0F);
    stats.maxTemp = SightlineRadiometry::fromCelsius(39.0F);
    stats.stdDevKelvin = 0.8F;

    TrackCoordinate track {};
    track.trackId = 5U;
    track.centerCol = 960.0;
    track.centerRow = 540.0;
    track.width = 80.0;
    track.height = 60.0;
    track.confidence = 92U;
    track.isPrimary = true;
    track.isCoasting = false;

    // 1. Build Target Pack in Kelvin (standard physical unit)
    const auto packKelvin = SightlineKlvRadiometryBridge::buildTargetPack(stats, track, TemperatureScale::Kelvin);
    EXPECT_EQ(packKelvin.targetId, 5U);
    ASSERT_TRUE(packKelvin.centroid.has_value());
    EXPECT_EQ(packKelvin.centroid->col, 960U);
    EXPECT_EQ(packKelvin.centroid->row, 540U);
    ASSERT_TRUE(packKelvin.boundingBox.has_value());
    EXPECT_EQ(packKelvin.boundingBox->topLeft.col, 920U);
    EXPECT_EQ(packKelvin.boundingBox->topLeft.row, 510U);
    EXPECT_EQ(packKelvin.boundingBox->bottomRight.col, 1000U);
    EXPECT_EQ(packKelvin.boundingBox->bottomRight.row, 570U);
    ASSERT_TRUE(packKelvin.confidence.has_value());
    EXPECT_EQ(*packKelvin.confidence, 92U);
    ASSERT_TRUE(packKelvin.priority.has_value());
    EXPECT_EQ(*packKelvin.priority, 1U); // Primary
    ASSERT_TRUE(packKelvin.detectionStatus.has_value());
    EXPECT_EQ(*packKelvin.detectionStatus, 1U); // Active/Tracking
    ASSERT_TRUE(packKelvin.targetIntensity.has_value());
    EXPECT_NEAR(*packKelvin.targetIntensity, 310.15F, 1e-4F);

    // 2. Build Target Pack in Celsius
    const auto packCelsius = SightlineKlvRadiometryBridge::buildTargetPack(stats, track, TemperatureScale::Celsius);
    ASSERT_TRUE(packCelsius.targetIntensity.has_value());
    EXPECT_NEAR(*packCelsius.targetIntensity, 37.0F, 1e-4F);
}

TEST(TestSightlineKlvRadiometry, VmtiLocalSetGenerationAndRoundTrip) {
    std::vector<RadiometricSpotStats> statsList;
    RadiometricSpotStats s1 {};
    s1.trackId = 1U;
    s1.meanTemp = SightlineRadiometry::fromCelsius(25.0F); // 298.15 K
    statsList.push_back(s1);

    RadiometricSpotStats s2 {};
    s2.trackId = 2U;
    s2.meanTemp = SightlineRadiometry::fromCelsius(45.0F); // 318.15 K
    statsList.push_back(s2);

    std::vector<TrackCoordinate> trackList;
    TrackCoordinate t1 {};
    t1.trackId = 1U;
    t1.centerCol = 400.0;
    t1.centerRow = 300.0;
    t1.width = 40.0;
    t1.height = 30.0;
    t1.confidence = 88U;
    t1.isPrimary = true;
    trackList.push_back(t1);

    TrackCoordinate t2 {};
    t2.trackId = 2U;
    t2.centerCol = 1200.0;
    t2.centerRow = 700.0;
    t2.width = 60.0;
    t2.height = 50.0;
    t2.confidence = 75U;
    t2.isPrimary = false;
    t2.isCoasting = true;
    trackList.push_back(t2);

    const auto vmtiSet = SightlineKlvRadiometryBridge::buildVmtiLocalSet(
        statsList, trackList, 1920U, 1080U, 1700000000123456ULL, TemperatureScale::Kelvin);

    EXPECT_EQ(vmtiSet.version, 6U);
    EXPECT_EQ(vmtiSet.systemName.value_or(""), "Sightline SLA VMTI");
    EXPECT_EQ(vmtiSet.frameWidth, 1920U);
    EXPECT_EQ(vmtiSet.frameHeight, 1080U);
    ASSERT_EQ(vmtiSet.targets.size(), 2U);

    // Encode to MISB ST 0903 binary packet
    const auto encodedVmti = Klv::VmtiEncoder::encode(vmtiSet, true);
    ASSERT_FALSE(encodedVmti.empty());
    EXPECT_TRUE(Klv::VmtiParser::isVmti(encodedVmti.data(), encodedVmti.size()));

    // Parse back and verify integrity
    Klv::VmtiLocalSet decodedVmti {};
    const auto status = Klv::VmtiParser::parse(encodedVmti.data(), encodedVmti.size(), decodedVmti, true);
    EXPECT_EQ(status, Klv::KlvStatus::Success);
    ASSERT_EQ(decodedVmti.targets.size(), 2U);

    // Target 1
    const auto& dt1 = decodedVmti.targets[0];
    EXPECT_EQ(dt1.targetId, 1U);
    ASSERT_TRUE(dt1.centroid.has_value());
    EXPECT_EQ(dt1.centroid->col, 400U);
    EXPECT_EQ(dt1.centroid->row, 300U);
    ASSERT_TRUE(dt1.confidence.has_value());
    EXPECT_EQ(*dt1.confidence, 88U);
    ASSERT_TRUE(dt1.targetIntensity.has_value());
    EXPECT_NEAR(*dt1.targetIntensity, 298.15F, 1e-4F);

    // Target 2
    const auto& dt2 = decodedVmti.targets[1];
    EXPECT_EQ(dt2.targetId, 2U);
    ASSERT_TRUE(dt2.centroid.has_value());
    EXPECT_EQ(dt2.centroid->col, 1200U);
    EXPECT_EQ(dt2.centroid->row, 700U);
    ASSERT_TRUE(dt2.detectionStatus.has_value());
    EXPECT_EQ(*dt2.detectionStatus, 2U); // Coasting
    ASSERT_TRUE(dt2.targetIntensity.has_value());
    EXPECT_NEAR(*dt2.targetIntensity, 318.15F, 1e-4F);
}

TEST(TestSightlineKlvRadiometry, UasDatalinkMessageEmbeddingAndRoundTrip) {
    std::vector<RadiometricSpotStats> statsList;
    RadiometricSpotStats s {};
    s.trackId = 10U;
    s.meanTemp = SightlineRadiometry::fromCelsius(36.5F); // 309.65 K
    statsList.push_back(s);

    std::vector<TrackCoordinate> trackList;
    TrackCoordinate trk {};
    trk.trackId = 10U;
    trk.centerCol = 640.0;
    trk.centerRow = 480.0;
    trk.width = 50.0;
    trk.height = 40.0;
    trk.confidence = 96U;
    trk.isPrimary = true;
    trackList.push_back(trk);

    auto uasMsg = SightlineKlvRadiometryBridge::buildUasMessage(
        statsList, trackList, RadiometricSensor::FlirBosonHighGain, 1280U, 720U, 1710000000000000ULL);

    uasMsg.missionId = "RECON-THERMAL-01";
    uasMsg.sensorLatitudeDeg = 37.5;
    uasMsg.sensorLongitudeDeg = 23.5;
    uasMsg.sensorTrueAltitudeM = 500.0;

    // Encode to full STANAG 4609 / MISB ST 0601 packet
    const auto packet = SightlineKlvRadiometryBridge::encodeUasPacket(uasMsg);
    ASSERT_FALSE(packet.empty());

    // Parse back
    Klv::UasDatalinkMessage parsedMsg {};
    const auto parseStatus = Klv::KlvParser::parse(packet.data(), packet.size(), parsedMsg, true);
    EXPECT_EQ(parseStatus, Klv::KlvStatus::Success);

    EXPECT_EQ(parsedMsg.missionId.value_or(""), "RECON-THERMAL-01");
    ASSERT_TRUE(parsedMsg.imageSourceSensor.has_value());
    EXPECT_EQ(*parsedMsg.imageSourceSensor, "FLIR Boson (High Gain)");
    ASSERT_TRUE(parsedMsg.wavelengthBands.has_value());
    EXPECT_EQ(*parsedMsg.wavelengthBands, 0x10U); // LWIR

    ASSERT_TRUE(parsedMsg.vmti.has_value());
    EXPECT_EQ(parsedMsg.vmti->frameWidth, 1280U);
    EXPECT_EQ(parsedMsg.vmti->frameHeight, 720U);
    ASSERT_EQ(parsedMsg.vmti->targets.size(), 1U);
    EXPECT_EQ(parsedMsg.vmti->targets[0].targetId, 10U);
    ASSERT_TRUE(parsedMsg.vmti->targets[0].targetIntensity.has_value());
    EXPECT_NEAR(*parsedMsg.vmti->targets[0].targetIntensity, 309.65F, 1e-4F);
}

TEST(TestSightlineKlvRadiometry, SightlineHardwareInjectionMessages) {
    TrackCoordinate trk {};
    trk.trackId = 7U;
    trk.centerCol = 320.0;
    trk.centerRow = 240.0;
    trk.width = 64.0;
    trk.height = 48.0;
    trk.confidence = 90U;

    // 1. Message 0x84 (MsgSetVmti)
    const auto setVmti = SightlineKlvRadiometryBridge::buildMsgSetVmti(trk, 0x0002U);
    ASSERT_EQ(setVmti.targets.size(), 1U);
    EXPECT_EQ(setVmti.targets[0].targetId, 7U);
    EXPECT_EQ(setVmti.targets[0].col, 320U);
    EXPECT_EQ(setVmti.targets[0].row, 240U);
    EXPECT_EQ(setVmti.targets[0].width, 64U);
    EXPECT_EQ(setVmti.targets[0].height, 48U);
    EXPECT_EQ(setVmti.targets[0].confidence, 90U);

    const auto vmtiPacket = SightlineKlvRadiometryBridge::buildSetVmtiPacket(setVmti);
    ASSERT_FALSE(vmtiPacket.empty());
    EXPECT_EQ(vmtiPacket[0], 0x51);
    EXPECT_EQ(vmtiPacket[1], 0xAC);
    EXPECT_EQ(vmtiPacket[3], 0x84); // Message ID 0x84

    // 2. Message 0x96 (MsgTagData) for Tag 95 Wavelength Bands
    const auto tagData = SightlineKlvRadiometryBridge::buildWavelengthTag(RadiometricSensor::FlirTau2HighRes, 0x0002U);
    EXPECT_EQ(tagData.tagId, 95U);
    ASSERT_EQ(tagData.data.size(), 1U);
    EXPECT_EQ(tagData.data[0], 0x10U);

    const auto tagPacket = SightlineKlvRadiometryBridge::buildTagDataPacket(tagData);
    ASSERT_FALSE(tagPacket.empty());
    EXPECT_EQ(tagPacket[0], 0x51);
    EXPECT_EQ(tagPacket[1], 0xAC);
    EXPECT_EQ(tagPacket[3], 0x96); // Message ID 0x96

    // 3. Message 0x15 (MsgSetMetadataFrameValues)
    const auto frameVals = SightlineKlvRadiometryBridge::buildFrameValues(trk, 1500.0, 0x0002U);
    EXPECT_EQ(frameVals.targetTrackGateWidth, 64U);
    EXPECT_EQ(frameVals.targetTrackGateHeight, 48U);
    EXPECT_EQ(frameVals.slantRange, 1500U);

    const auto framePacket = SightlineKlvRadiometryBridge::buildFrameValuesPacket(frameVals);
    ASSERT_FALSE(framePacket.empty());
    EXPECT_EQ(framePacket[0], 0x51);
    EXPECT_EQ(framePacket[1], 0xAC);
    EXPECT_EQ(framePacket[3], 0x15); // Message ID 0x15
}
