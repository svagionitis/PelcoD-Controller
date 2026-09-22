/// @file TestViscaBusScanner.cpp
/// @brief Unit tests for ViscaBusScanner daisy-chain discovery and camera enumeration.

#include "MockViscaTransport.h"
#include "ViscaBusScanner.h"
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace Visca;
using namespace Visca::Testing;

/// @brief Tests daisy-chain enumeration of a single camera on the bus.
/// @details Verifies that AddressSet and CAM_VersionInq successfully identify camera parameters.
TEST(TestViscaBusScanner, DiscoverSingleCamera)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(1);
    mockCamera->open();

    ViscaBusScanner scanner(mockCamera);

    size_t progressCalls = 0;
    size_t foundCalls = 0;

    const auto cameras = scanner.scanBus([&](size_t /*curr*/, size_t /*total*/) { ++progressCalls; },
        [&](const DiscoveredCamera& cam) {
            ++foundCalls;
            EXPECT_EQ(cam.address, 1U);
            EXPECT_EQ(cam.vendorId, 0x0020);
            EXPECT_EQ(cam.modelId, 0x0711);
            EXPECT_EQ(cam.maxSockets, 2U);
        });

    EXPECT_EQ(cameras.size(), 1U);
    EXPECT_EQ(foundCalls, 1U);
    EXPECT_GT(progressCalls, 0U);
}

/// @brief Tests error callback when scanning with a closed or null transport interface.
TEST(TestViscaBusScanner, ClosedTransportReportsError)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(1);
    mockCamera->close(); // explicitly closed

    ViscaBusScanner scanner(mockCamera);

    ScanError reportedError = ScanError::None;
    std::string reportedMsg;

    const auto cameras = scanner.scanBus(
        nullptr,
        nullptr,
        [&](ScanError err, const std::string& msg) {
            reportedError = err;
            reportedMsg = msg;
        });

    EXPECT_TRUE(cameras.empty());
    EXPECT_EQ(reportedError, ScanError::TransportNotOpen);
    EXPECT_FALSE(reportedMsg.empty());
}

/// @brief Tests error callback when transport sendData fails during AddressSet.
TEST(TestViscaBusScanner, SendFailureReportsError)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(1);
    mockCamera->open();
    mockCamera->setSendFailure(true);

    ViscaBusScanner scanner(mockCamera);

    ScanError reportedError = ScanError::None;
    std::string reportedMsg;

    const auto cameras = scanner.scanBus(
        nullptr,
        nullptr,
        [&](ScanError err, const std::string& msg) {
            reportedError = err;
            reportedMsg = msg;
        });

    EXPECT_TRUE(cameras.empty());
    EXPECT_EQ(reportedError, ScanError::AddressSetSendFailed);
    EXPECT_FALSE(reportedMsg.empty());
}

/// @brief Tests error reporting when AddressSet times out, triggering single-camera fallback.
TEST(TestViscaBusScanner, AddressSetTimeoutFallback)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(1);
    mockCamera->open();
    mockCamera->setDropAddressSet(true); // AddressSet will time out, but versionInquiry works

    ViscaBusScanner scanner(mockCamera);

    bool receivedTimeout = false;

    const auto cameras = scanner.scanBus(
        nullptr,
        nullptr,
        [&](ScanError err, const std::string& /*msg*/) {
            if (err == ScanError::AddressSetTimeout) {
                receivedTimeout = true;
            }
        });

    EXPECT_TRUE(receivedTimeout);
    EXPECT_EQ(cameras.size(), 1U);
    EXPECT_EQ(cameras[0].address, 1U);
}

/// @brief Tests early abort when cancellation predicate is signalled.
TEST(TestViscaBusScanner, CancellationAbortsEarly)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(1);
    mockCamera->open();

    ViscaBusScanner scanner(mockCamera);

    bool cancelledReported = false;

    // Cancel immediately before scanning addresses
    const auto cameras = scanner.scanBus(
        nullptr,
        nullptr,
        [&](ScanError err, const std::string& /*msg*/) {
            if (err == ScanError::Cancelled) {
                cancelledReported = true;
            }
        },
        []() { return true; });

    EXPECT_TRUE(cameras.empty());
    EXPECT_TRUE(cancelledReported);
}

/// @brief Verifies error string utility conversions.
TEST(TestViscaBusScanner, ScanErrorToStringConversion)
{
    EXPECT_STREQ(scanErrorToString(ScanError::None), "None");
    EXPECT_STREQ(scanErrorToString(ScanError::TransportNotOpen), "TransportNotOpen");
    EXPECT_STREQ(scanErrorToString(ScanError::AddressSetSendFailed), "AddressSetSendFailed");
    EXPECT_STREQ(scanErrorToString(ScanError::AddressSetTimeout), "AddressSetTimeout");
    EXPECT_STREQ(scanErrorToString(ScanError::VersionInquirySendFailed), "VersionInquirySendFailed");
    EXPECT_STREQ(scanErrorToString(ScanError::VersionInquiryTimeout), "VersionInquiryTimeout");
    EXPECT_STREQ(scanErrorToString(ScanError::Cancelled), "Cancelled");
}
