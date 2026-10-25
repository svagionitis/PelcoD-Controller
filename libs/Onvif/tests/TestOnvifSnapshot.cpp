/// @file TestOnvifSnapshot.cpp
/// @brief Conformance tests for ONVIF Profile S Snapshot delivery and JPEG generator (P2 Phase 4).

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Onvif/HttplibInclude.h"
#include "Onvif/JpegGenerator.h"
#include "Onvif/OnvifClient.h"
#include "Onvif/OnvifServer.h"
#include "Onvif/OnvifServerTypes.h"
#include "OnvifTestClient.h"
#include "OnvifTestFixtures.h"

using namespace Onvif;

namespace {

struct TestServerContext {
    OnvifServerConfig config {};
    int port { static_cast<int>(Onvif::Test::getFreeEphemeralPort()) };
    std::unique_ptr<OnvifServer> server {};

    TestServerContext(bool enableAuth = false)
    {
        config.port = port;
        config.bindAddress = "127.0.0.1";
        config.auth.enabled = enableAuth;
        config.defaultUsers = OnvifTest::defaultTestUsers();

        server = std::make_unique<OnvifServer>(config);
        EXPECT_TRUE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestServerContext()
    {
        if (server) {
            server->stop();
        }
    }
};

} // namespace

// ============================================================================
// 1. Snapshot HTTP Endpoint Conformance (Task 4.2)
// ============================================================================

TEST(TestOnvifSnapshot, EndpointReturnsValid200Jpeg)
{
    TestServerContext ctx { false };
    httplib::Client client("127.0.0.1", ctx.port);

    const auto res = client.Get("/onvif/snapshot");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "image/jpeg");
    EXPECT_FALSE(res->body.empty());

    // Validate JPEG byte payload
    std::vector<std::uint8_t> jpegBytes(res->body.size());
    for (std::size_t i = 0; i < res->body.size(); ++i) {
        jpegBytes[i] = static_cast<std::uint8_t>(res->body[i]);
    }

    EXPECT_TRUE(JpegGenerator::isValidJpeg(jpegBytes));

    int width { 0 };
    int height { 0 };
    EXPECT_TRUE(JpegGenerator::getDimensions(jpegBytes, width, height));
    EXPECT_EQ(width, 640);
    EXPECT_EQ(height, 480);
}

TEST(TestOnvifSnapshot, EndpointRequiresAuthentication)
{
    TestServerContext ctx { true }; // Auth enabled

    // Unauthenticated GET should be rejected with HTTP 401
    httplib::Client unauthClient("127.0.0.1", ctx.port);
    const auto unauthRes = unauthClient.Get("/onvif/snapshot");
    ASSERT_TRUE(unauthRes);
    EXPECT_EQ(unauthRes->status, 401);

    // Authenticated GET via OnvifTestClient should succeed with HTTP 200
    OnvifTest::OnvifTestClient authClient("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());
    const auto authRes = authClient.Get("/onvif/snapshot");
    ASSERT_TRUE(authRes);
    EXPECT_EQ(authRes->status, 200);
    EXPECT_EQ(authRes->get_header_value("Content-Type"), "image/jpeg");
}

TEST(TestOnvifSnapshot, CustomSnapshotProvider)
{
    TestServerContext ctx { false };

    // Register a custom frame provider returning synthetic RGB converted to JPEG
    std::vector<std::uint8_t> dummyRgb(320 * 240 * 3, 128U);
    const std::vector<std::uint8_t> expectedJpeg = JpegGenerator::encodeRgb(dummyRgb.data(), 320, 240, 75);

    ctx.server->setSnapshotProvider([&expectedJpeg]() { return expectedJpeg; });

    httplib::Client client("127.0.0.1", ctx.port);
    const auto res = client.Get("/onvif/snapshot");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->body.size(), expectedJpeg.size());

    int width { 0 };
    int height { 0 };
    std::vector<std::uint8_t> jpegBytes(res->body.size());
    for (std::size_t i = 0; i < res->body.size(); ++i) {
        jpegBytes[i] = static_cast<std::uint8_t>(res->body[i]);
    }
    EXPECT_TRUE(JpegGenerator::getDimensions(jpegBytes, width, height));
    EXPECT_EQ(width, 320);
    EXPECT_EQ(height, 240);
}

// ============================================================================
// 2. JpegGenerator Direct Validation (Task 4.1)
// ============================================================================

TEST(TestOnvifSnapshot, JpegGeneratorValidation)
{
    // Empty data or invalid markers
    std::vector<std::uint8_t> invalidData { 0x00, 0x01, 0x02 };
    EXPECT_FALSE(JpegGenerator::isValidJpeg(invalidData));

    int width { 0 };
    int height { 0 };
    EXPECT_FALSE(JpegGenerator::getDimensions(invalidData, width, height));

    // Valid synthetic 640x480 test frame
    const auto frame640 = JpegGenerator::createTestFrame(640, 480, "2026-10-22 15:45:00 UTC", "Camera 1");
    EXPECT_TRUE(JpegGenerator::isValidJpeg(frame640));
    EXPECT_TRUE(JpegGenerator::getDimensions(frame640, width, height));
    EXPECT_EQ(width, 640);
    EXPECT_EQ(height, 480);

    // Valid synthetic 1920x1080 test frame
    const auto frame1080 = JpegGenerator::createTestFrame(1920, 1080, "2026-10-22 15:45:00 UTC", "FHD Sensor");
    EXPECT_TRUE(JpegGenerator::isValidJpeg(frame1080));
    EXPECT_TRUE(JpegGenerator::getDimensions(frame1080, width, height));
    EXPECT_EQ(width, 1920);
    EXPECT_EQ(height, 1080);
}

TEST(TestOnvifSnapshot, JpegGeneratorRgbEncoding)
{
    constexpr int kW = 64;
    constexpr int kH = 64;
    std::vector<std::uint8_t> rgb(kW * kH * 3);

    // Generate color blocks
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const std::size_t idx = static_cast<std::size_t>((y * kW + x) * 3);
            rgb[idx] = static_cast<std::uint8_t>((x * 255) / kW);
            rgb[idx + 1] = static_cast<std::uint8_t>((y * 255) / kH);
            rgb[idx + 2] = 200U;
        }
    }

    const auto encoded = JpegGenerator::encodeRgb(rgb.data(), kW, kH, 85);
    EXPECT_TRUE(JpegGenerator::isValidJpeg(encoded));

    int w { 0 };
    int h { 0 };
    EXPECT_TRUE(JpegGenerator::getDimensions(encoded, w, h));
    EXPECT_EQ(w, kW);
    EXPECT_EQ(h, kH);
}

TEST(TestOnvifSnapshot, OnvifClientGetSnapshot)
{
    TestServerContext ctx { false };
    const std::string deviceUrl = "http://127.0.0.1:" + std::to_string(ctx.port) + "/onvif/device_service";
    OnvifClient client(deviceUrl);

    const std::string snapshotUri = "http://127.0.0.1:" + std::to_string(ctx.port) + "/onvif/snapshot";
    const auto snapshot = client.getSnapshot(snapshotUri);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_FALSE(snapshot->empty());
    EXPECT_TRUE(JpegGenerator::isValidJpeg(*snapshot));

    int width { 0 };
    int height { 0 };
    EXPECT_TRUE(JpegGenerator::getDimensions(*snapshot, width, height));
    EXPECT_EQ(width, 640);
    EXPECT_EQ(height, 480);
}
