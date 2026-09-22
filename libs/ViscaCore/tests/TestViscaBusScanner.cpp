/// @file TestViscaBusScanner.cpp
/// @brief Unit tests for ViscaBusScanner daisy-chain discovery and camera enumeration.

#include "MockViscaTransport.h"
#include "ViscaBusScanner.h"
#include <gtest/gtest.h>

#include <memory>
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
