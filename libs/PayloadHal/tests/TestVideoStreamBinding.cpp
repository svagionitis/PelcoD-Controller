#include <gtest/gtest.h>

#include "CameraStreamBinder.h"
#include "GridDemProvider.h"
#include "PayloadFactory.h"
#include "PayloadHal.h"
#include "VideoStreamTypes.h"
#include "sim/SimulatedPayload.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace PayloadHal;

// =============================================================================
// 1. VideoStreamTypes Tests
// =============================================================================

TEST(TestVideoStreamBinding, ProtocolDeductionAndStrings)
{
    EXPECT_EQ(deduceTransportProtocol("rtsp://192.168.1.50:554/live"), StreamTransportProtocol::Rtsp);
    EXPECT_EQ(deduceTransportProtocol("rtsps://camera.lan/stream"), StreamTransportProtocol::Rtsp);
    EXPECT_EQ(deduceTransportProtocol("sim://daylight"), StreamTransportProtocol::Simulated);
    EXPECT_EQ(deduceTransportProtocol("v4l2:///dev/video0"), StreamTransportProtocol::V4L2);
    EXPECT_EQ(deduceTransportProtocol("/dev/video2"), StreamTransportProtocol::V4L2);
    EXPECT_EQ(deduceTransportProtocol("dshow://Integrated Camera"), StreamTransportProtocol::DirectShow);
    EXPECT_EQ(deduceTransportProtocol("udp://@239.255.0.1:1234"), StreamTransportProtocol::UdpMpegTs);
    EXPECT_EQ(deduceTransportProtocol("rtp://127.0.0.1:5004"), StreamTransportProtocol::UdpMpegTs);
    EXPECT_EQ(deduceTransportProtocol("webrtc://server:8443/stream"), StreamTransportProtocol::WebRtc);
    EXPECT_EQ(deduceTransportProtocol("whep://live.example.com/whep"), StreamTransportProtocol::WebRtc);

    EXPECT_STREQ(videoStreamProfileToString(VideoStreamProfile::Primary), "Primary");
    EXPECT_STREQ(videoStreamProfileToString(VideoStreamProfile::Secondary), "Secondary");
    EXPECT_STREQ(videoStreamProfileToString(VideoStreamProfile::Thermal), "Thermal");
    EXPECT_STREQ(videoStreamProfileToString(VideoStreamProfile::Snapshot), "Snapshot");

    EXPECT_STREQ(streamTransportProtocolToString(StreamTransportProtocol::Rtsp), "RTSP");
    EXPECT_STREQ(streamTransportProtocolToString(StreamTransportProtocol::Simulated), "Simulated");
    EXPECT_STREQ(streamTransportProtocolToString(StreamTransportProtocol::V4L2), "V4L2");
    EXPECT_STREQ(streamTransportProtocolToString(StreamTransportProtocol::DirectShow), "DirectShow");
    EXPECT_STREQ(streamTransportProtocolToString(StreamTransportProtocol::UdpMpegTs), "UDP/MPEG-TS");
    EXPECT_STREQ(streamTransportProtocolToString(StreamTransportProtocol::WebRtc), "WebRTC");
}

// =============================================================================
// 2. SimulatedPayload Video Stream Binding Tests
// =============================================================================

TEST(TestVideoStreamBinding, SimulatedPayloadStreams)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(payload->connect());

    // Check defaults
    EXPECT_EQ(payload->videoStreamUri(), "sim://daylight");
    EXPECT_EQ(payload->videoStreamUri(VideoStreamProfile::Primary), "sim://daylight");
    EXPECT_EQ(payload->videoStreamUri(VideoStreamProfile::Secondary), "sim://daylight-sub");
    EXPECT_EQ(payload->videoStreamUri(VideoStreamProfile::Snapshot), "sim://daylight/snapshot.jpg");
    EXPECT_EQ(payload->thermalVideoStreamUri(), "sim://thermal");

    // Check allVideoStreams
    const auto allStreams = payload->allVideoStreams();
    EXPECT_GE(allStreams.size(), 4U);

    bool hasDaylight = false;
    bool hasThermal = false;
    for (const auto& s : allStreams) {
        if (s.uri == "sim://daylight") {
            hasDaylight = true;
            EXPECT_EQ(s.profile, VideoStreamProfile::Primary);
            EXPECT_EQ(s.transport, StreamTransportProtocol::Simulated);
            EXPECT_EQ(s.width, 1920);
            EXPECT_EQ(s.height, 1080);
        } else if (s.uri == "sim://thermal") {
            hasThermal = true;
            EXPECT_EQ(s.profile, VideoStreamProfile::Thermal);
            EXPECT_EQ(s.transport, StreamTransportProtocol::Simulated);
            EXPECT_EQ(s.width, 640);
            EXPECT_EQ(s.height, 512);
        }
    }
    EXPECT_TRUE(hasDaylight);
    EXPECT_TRUE(hasThermal);

    // Modify primary stream URI
    auto primCam = payload->primaryCamera();
    ASSERT_NE(primCam, nullptr);
    EXPECT_TRUE(primCam->setVideoStreamUri("rtsp://10.0.0.100:554/live.sdp", VideoStreamProfile::Primary));
    EXPECT_EQ(payload->videoStreamUri(), "rtsp://10.0.0.100:554/live.sdp");

    const auto desc = primCam->streamDescriptor(VideoStreamProfile::Primary);
    ASSERT_TRUE(desc.has_value());
    EXPECT_EQ(desc->uri, "rtsp://10.0.0.100:554/live.sdp");
    EXPECT_EQ(desc->transport, StreamTransportProtocol::Rtsp);
}

// =============================================================================
// 3. PayloadFactory Stream Parameter Parsing Tests
// =============================================================================

TEST(TestVideoStreamBinding, FactoryUriStreamParameters)
{
    const std::string uri = "sim://?"
                            "video=rtsp://192.168.1.100:554/ch0&"
                            "substream=rtsp://192.168.1.100:554/ch1&"
                            "thermal=rtsp://192.168.1.101:554/ir&"
                            "snapshot=http://192.168.1.100/snapshot.jpg";

    auto payload = PayloadFactory::createFromUri(uri);
    ASSERT_NE(payload, nullptr);

    EXPECT_EQ(payload->videoStreamUri(VideoStreamProfile::Primary), "rtsp://192.168.1.100:554/ch0");
    EXPECT_EQ(payload->videoStreamUri(VideoStreamProfile::Secondary), "rtsp://192.168.1.100:554/ch1");
    EXPECT_EQ(payload->videoStreamUri(VideoStreamProfile::Snapshot), "http://192.168.1.100/snapshot.jpg");
    EXPECT_EQ(payload->thermalVideoStreamUri(), "rtsp://192.168.1.101:554/ir");
}

// =============================================================================
// 4. CameraStreamBinder Frame Synchronization Tests
// =============================================================================

TEST(TestVideoStreamBinding, BinderBasicFrameIngestion)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(payload->connect());

    // Slew PTU to known orientation
    auto ptu = payload->panTilt();
    ASSERT_NE(ptu, nullptr);
    EXPECT_TRUE(ptu->setAbsoluteAngles(45.0, -15.0));

    // Drive camera zoom
    auto cam = payload->primaryCamera();
    ASSERT_NE(cam, nullptr);
    EXPECT_TRUE(cam->setZoomNormalized(0.5));

    CameraStreamBinder binder(payload, VideoStreamProfile::Primary);
    EXPECT_EQ(binder.descriptor().profile, VideoStreamProfile::Primary);

    // Create raw video frame
    RawVideoFrame raw;
    raw.frameNumber = 101;
    raw.width = 1920;
    raw.height = 1080;
    raw.pixelFormat = "BGR24";
    raw.data.resize(1920 * 1080 * 3, 0x7F);
    raw.timestamp = std::chrono::system_clock::now();

    const auto syncd = binder.bindFrame(raw);
    EXPECT_EQ(syncd.frame.frameNumber, 101U);
    EXPECT_EQ(syncd.frame.width, 1920);
    EXPECT_EQ(syncd.frame.height, 1080);
    EXPECT_NEAR(syncd.gimbalTelemetry.panAngleDeg, 45.0, 0.1);
    EXPECT_NEAR(syncd.gimbalTelemetry.tiltAngleDeg, -15.0, 0.1);
    EXPECT_NEAR(syncd.cameraTelemetry.normalizedZoom, 0.5, 0.05);
    EXPECT_GT(syncd.cameraTelemetry.horizontalFovDeg, 0.0);

    // No platform state set yet -> no ground intersection
    EXPECT_FALSE(syncd.targetGroundIntersection.has_value());
    EXPECT_FALSE(syncd.frustumCorners.has_value());
}

TEST(TestVideoStreamBinding, BinderGeoreferenceCalculation)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(payload->connect());

    auto ptu = payload->panTilt();
    ASSERT_NE(ptu, nullptr);
    EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, -30.0)); // Looking straight ahead and 30 deg down

    CameraStreamBinder binder(payload, VideoStreamProfile::Primary);

    // Set host aircraft at (37.7749 N, -122.4194 W, 1000m MSL), heading North (0 deg)
    const Klv::GeoPoint3D platformPos { 37.7749, -122.4194, 1000.0 };
    binder.setPlatformState(platformPos, 0.0);

    RawVideoFrame raw;
    raw.frameNumber = 1;
    raw.timestamp = std::chrono::system_clock::now();

    const auto syncd = binder.bindFrame(raw);
    ASSERT_TRUE(syncd.targetGroundIntersection.has_value());
    ASSERT_TRUE(syncd.frustumCorners.has_value());

    // Target ground intersection should be north of platform (latitude > 37.7749)
    EXPECT_GT(syncd.targetGroundIntersection->latitudeDeg, platformPos.latitudeDeg);
    EXPECT_NEAR(syncd.targetGroundIntersection->longitudeDeg, platformPos.longitudeDeg, 0.001);
    EXPECT_NEAR(syncd.targetGroundIntersection->altitudeM, 0.0, 1.0);

    // Frustum corners should be valid
    EXPECT_GT(syncd.frustumCorners->topLeft.latitudeDeg, platformPos.latitudeDeg);
    EXPECT_GT(syncd.frustumCorners->topRight.latitudeDeg, platformPos.latitudeDeg);
}

TEST(TestVideoStreamBinding, BinderDemIntersection)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(payload->connect());

    // Setup a 200m plateau DEM provider
    std::vector<float> elevations(10 * 10, 200.0f);
    auto dem = std::make_shared<GridDemProvider>(37.70, 37.85, -122.50, -122.35, 10, 10, std::move(elevations));
    payload->setDemProvider(dem);

    auto ptu = payload->panTilt();
    ASSERT_NE(ptu, nullptr);
    EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, -45.0));

    CameraStreamBinder binder(payload, VideoStreamProfile::Primary);
    const Klv::GeoPoint3D platformPos { 37.7749, -122.4194, 1200.0 };
    binder.setPlatformState(platformPos, 0.0);

    RawVideoFrame raw;
    raw.frameNumber = 5;
    raw.timestamp = std::chrono::system_clock::now();

    const auto syncd = binder.bindFrame(raw);
    ASSERT_TRUE(syncd.targetGroundIntersection.has_value());
    // Should intersect plateau near 200m elevation
    EXPECT_NEAR(syncd.targetGroundIntersection->altitudeM, 200.0, 15.0);
}

TEST(TestVideoStreamBinding, TelemetryInterpolation)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_NE(payload, nullptr);

    CameraStreamBinder binder(payload, VideoStreamProfile::Primary);

    const auto baseTime = std::chrono::system_clock::now();

    // Record sample at t=0
    GimbalTelemetry g0 {};
    g0.panAngleDeg = 10.0;
    g0.tiltAngleDeg = -20.0;
    g0.panRateDegPerSec = 10.0;
    g0.tiltRateDegPerSec = -5.0;
    g0.timestamp = baseTime;
    binder.recordGimbalTelemetry(g0);

    // Record sample at t=100ms
    GimbalTelemetry g1 {};
    g1.panAngleDeg = 20.0;
    g1.tiltAngleDeg = -30.0;
    g1.panRateDegPerSec = 10.0;
    g1.tiltRateDegPerSec = -5.0;
    g1.timestamp = baseTime + std::chrono::milliseconds(100);
    binder.recordGimbalTelemetry(g1);

    // Query frame at t=50ms (halfway)
    RawVideoFrame raw;
    raw.timestamp = baseTime + std::chrono::milliseconds(50);
    const auto syncd = binder.bindFrame(raw);

    EXPECT_NEAR(syncd.gimbalTelemetry.panAngleDeg, 15.0, 0.01);
    EXPECT_NEAR(syncd.gimbalTelemetry.tiltAngleDeg, -25.0, 0.01);
    EXPECT_NEAR(syncd.gimbalTelemetry.panRateDegPerSec, 10.0, 0.01);
    EXPECT_NEAR(syncd.gimbalTelemetry.tiltRateDegPerSec, -5.0, 0.01);

    // Test pan wrap-around across 360 deg
    GimbalTelemetry gw0 {};
    gw0.panAngleDeg = 350.0;
    gw0.timestamp = baseTime + std::chrono::milliseconds(200);
    binder.recordGimbalTelemetry(gw0);

    GimbalTelemetry gw1 {};
    gw1.panAngleDeg = 10.0;
    gw1.timestamp = baseTime + std::chrono::milliseconds(300);
    binder.recordGimbalTelemetry(gw1);

    RawVideoFrame rawWrap;
    rawWrap.timestamp = baseTime + std::chrono::milliseconds(250);
    const auto syncdWrap = binder.bindFrame(rawWrap);
    // Midpoint between 350° and 10° rotating forward is 0.0° / 360.0°
    EXPECT_TRUE(std::abs(syncdWrap.gimbalTelemetry.panAngleDeg) < 0.1
                || std::abs(syncdWrap.gimbalTelemetry.panAngleDeg - 360.0) < 0.1);
}

TEST(TestVideoStreamBinding, AsynchronousFrameCallback)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_NE(payload, nullptr);
    ASSERT_TRUE(payload->connect());

    CameraStreamBinder binder(payload, VideoStreamProfile::Primary);

    std::uint64_t receivedFrameNumber = 0;
    bool callbackFired = false;

    binder.setFrameCallback([&](const SynchronizedVideoFrame& syncd) {
        receivedFrameNumber = syncd.frame.frameNumber;
        callbackFired = true;
    });

    RawVideoFrame raw;
    raw.frameNumber = 42;
    raw.width = 640;
    raw.height = 480;
    binder.ingestFrame(raw);

    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(receivedFrameNumber, 42U);
}
