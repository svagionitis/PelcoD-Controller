/// @file TestOnvifPtzConformance.cpp
/// @brief Conformance tests for PTZ numeric sanitization, coordinate mapping, and auto-stop (H9 & H10).

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestClient.h"
#include <Onvif/HttplibInclude.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/adapters/PelcoDPtzAdapter.h>
#include <PelcoDCore/PelcoDDevice.h>
#include <PelcoDSim/MockPelcoDDevice.h>

namespace {

std::atomic<int> g_ptzPort { 19400 };

[[nodiscard]] std::string makeSoapRequest(const std::string& payloadXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
           "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
           "  <SOAP-ENV:Body>\r\n"
           "    "
        + payloadXml
        + "\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>\r\n";
}

/// @class PtzConformanceTest
/// @brief Fixture provisioning an OnvifServer backed by PelcoDPtzAdapter and MockPelcoDDevice.
class PtzConformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_port = g_ptzPort.fetch_add(1);

        m_mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
        m_device = std::make_shared<PelcoD::PelcoDDevice>(m_mockTransport, std::uint8_t { 1U });
        ASSERT_TRUE(m_device->start());

        m_adapter = std::make_shared<Onvif::PelcoDPtzAdapter>(m_device);

        Onvif::OnvifServerConfig config {};
        config.bindAddress = "127.0.0.1";
        config.port = m_port;
        config.deviceName = "PTZ Conformance Device";
        config.defaultUsers = OnvifTest::defaultTestUsers();
        m_server = std::make_unique<Onvif::OnvifServer>(config);
        m_server->setPtzHandler(m_adapter);
        ASSERT_TRUE(m_server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        m_client = std::make_unique<OnvifTest::OnvifTestClient>("127.0.0.1", m_port, OnvifTest::defaultAdmin());
    }

    void TearDown() override
    {
        if (m_server) {
            m_server->stop();
        }
        if (m_device) {
            m_device->stop();
        }
    }

    int m_port { 0 };
    std::shared_ptr<PelcoD::MockPelcoDDevice> m_mockTransport {};
    std::shared_ptr<PelcoD::PelcoDDevice> m_device {};
    std::shared_ptr<Onvif::PelcoDPtzAdapter> m_adapter {};
    std::unique_ptr<Onvif::OnvifServer> m_server {};
    std::unique_ptr<OnvifTest::OnvifTestClient> m_client {};
};

} // namespace

// ============================================================================
// H9: Float-to-Integer Numeric Sanitization & NaN/Inf Rejection
// ============================================================================

TEST_F(PtzConformanceTest, PtzSanitNanVel)
{
    // 1. ContinuousMove with NaN pan velocity
    {
        const std::string xml = makeSoapRequest("<tptz:ContinuousMove>\r\n"
                                                "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                                "  <tptz:Velocity>\r\n"
                                                "    <tt:PanTilt x=\"NaN\" y=\"0.5\"/>\r\n"
                                                "  </tptz:Velocity>\r\n"
                                                "</tptz:ContinuousMove>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 400);
        EXPECT_NE(res->body.find("ter:InvalidArgVal"), std::string::npos);
        EXPECT_FALSE(m_mockTransport->isMoving());
    }

    // 2. ContinuousMove with Infinity tilt velocity
    {
        const std::string xml = makeSoapRequest("<tptz:ContinuousMove>\r\n"
                                                "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                                "  <tptz:Velocity>\r\n"
                                                "    <tt:PanTilt x=\"0.5\" y=\"INF\"/>\r\n"
                                                "  </tptz:Velocity>\r\n"
                                                "</tptz:ContinuousMove>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 400);
        EXPECT_NE(res->body.find("ter:InvalidArgVal"), std::string::npos);
        EXPECT_FALSE(m_mockTransport->isMoving());
    }
}

TEST_F(PtzConformanceTest, PtzSanitNanAbs)
{
    // AbsoluteMove with NaN pan coordinate
    const std::string xml = makeSoapRequest("<tptz:AbsoluteMove>\r\n"
                                            "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                            "  <tptz:Position>\r\n"
                                            "    <tt:PanTilt x=\"NaN\" y=\"0.5\"/>\r\n"
                                            "  </tptz:Position>\r\n"
                                            "</tptz:AbsoluteMove>");

    const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_NE(res->body.find("ter:InvalidArgVal"), std::string::npos);
}

TEST_F(PtzConformanceTest, PtzDirectNanAbort)
{
    // Direct invocation of PelcoDPtzAdapter methods with NaN / Inf must safely abort
    const auto initialPan = m_mockTransport->getInternalState().panCentidegrees;

    m_adapter->handleContinuousMove(std::numeric_limits<float>::quiet_NaN(), 0.5f, 0.0f);
    EXPECT_FALSE(m_mockTransport->isMoving());

    m_adapter->handleContinuousMove(1.0f, std::numeric_limits<float>::infinity(), 0.0f);
    EXPECT_FALSE(m_mockTransport->isMoving());

    m_adapter->handleAbsoluteMove(std::numeric_limits<float>::quiet_NaN(), 0.5f, 0.0f);
    EXPECT_EQ(m_mockTransport->getInternalState().panCentidegrees, initialPan);
}

// ============================================================================
// H10: ProfileToken Validation
// ============================================================================

TEST_F(PtzConformanceTest, PtzBadProfile)
{
    // 1. Unknown ProfileToken in ContinuousMove
    {
        const std::string xml = makeSoapRequest("<tptz:ContinuousMove>\r\n"
                                                "  <tptz:ProfileToken>NonExistentToken</tptz:ProfileToken>\r\n"
                                                "  <tptz:Velocity>\r\n"
                                                "    <tt:PanTilt x=\"0.5\" y=\"0.5\"/>\r\n"
                                                "  </tptz:Velocity>\r\n"
                                                "</tptz:ContinuousMove>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
        EXPECT_FALSE(m_mockTransport->isMoving());
    }

    // 2. Missing ProfileToken in ContinuousMove
    {
        const std::string xml = makeSoapRequest("<tptz:ContinuousMove>\r\n"
                                                "  <tptz:Velocity>\r\n"
                                                "    <tt:PanTilt x=\"0.5\" y=\"0.5\"/>\r\n"
                                                "  </tptz:Velocity>\r\n"
                                                "</tptz:ContinuousMove>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
    }

    // 3. Unknown ProfileToken in Stop
    {
        const std::string xml = makeSoapRequest("<tptz:Stop>\r\n"
                                                "  <tptz:ProfileToken>InvalidToken</tptz:ProfileToken>\r\n"
                                                "</tptz:Stop>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
    }
}

// ============================================================================
// H10: PTZ Coordinate Space Semantics & Negative Coordinates
// ============================================================================

TEST_F(PtzConformanceTest, PtzAbsGenericMap)
{
    // GenericSpace: x=0.0 -> 180 deg (18000 cDeg), y=0.0 -> 45 deg (4500 cDeg)
    const std::string xml
        = makeSoapRequest("<tptz:AbsoluteMove>\r\n"
                          "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                          "  <tptz:Position>\r\n"
                          "    <tt:PanTilt x=\"0.0\" y=\"0.0\" "
                          "space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\"/>\r\n"
                          "    <tt:Zoom x=\"0.5\"/>\r\n"
                          "  </tptz:Position>\r\n"
                          "</tptz:AbsoluteMove>");

    const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto state = m_mockTransport->getInternalState();
    EXPECT_EQ(state.panCentidegrees, 18000U);
    EXPECT_EQ(state.tiltCentidegrees, 4500U);
}

TEST_F(PtzConformanceTest, PtzAbsNegative)
{
    // GenericSpace: x=-1.0 -> 0 deg (0 cDeg), y=-1.0 -> 0 deg (0 cDeg)
    {
        const std::string xml = makeSoapRequest("<tptz:AbsoluteMove>\r\n"
                                                "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                                "  <tptz:Position>\r\n"
                                                "    <tt:PanTilt x=\"-1.0\" y=\"-1.0\"/>\r\n"
                                                "  </tptz:Position>\r\n"
                                                "</tptz:AbsoluteMove>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 200);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const auto state = m_mockTransport->getInternalState();
        EXPECT_EQ(state.panCentidegrees, 0U);
        EXPECT_EQ(state.tiltCentidegrees, 0U);
    }

    // GenericSpace: x=-0.5 -> 90 deg (9000 cDeg), y=1.0 -> 90 deg (9000 cDeg)
    {
        const std::string xml = makeSoapRequest("<tptz:AbsoluteMove>\r\n"
                                                "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                                "  <tptz:Position>\r\n"
                                                "    <tt:PanTilt x=\"-0.5\" y=\"1.0\"/>\r\n"
                                                "  </tptz:Position>\r\n"
                                                "</tptz:AbsoluteMove>");

        const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 200);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const auto state = m_mockTransport->getInternalState();
        EXPECT_EQ(state.panCentidegrees, 9000U);
        EXPECT_EQ(state.tiltCentidegrees, 9000U);
    }
}

TEST_F(PtzConformanceTest, PtzAbsOmitAxis)
{
    // 1. Move to baseline pan 18000, tilt 4500
    m_adapter->handleAbsoluteMove(0.0f, 0.0f, 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(m_mockTransport->getInternalState().panCentidegrees, 18000U);
    EXPECT_EQ(m_mockTransport->getInternalState().tiltCentidegrees, 4500U);

    // 2. Command only Pan (x=0.5 -> 270 deg / 27000 cDeg), omitting tilt and zoom
    const std::string xml = makeSoapRequest("<tptz:AbsoluteMove>\r\n"
                                            "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                            "  <tptz:Position>\r\n"
                                            "    <tt:PanTilt x=\"0.5\"/>\r\n"
                                            "  </tptz:Position>\r\n"
                                            "</tptz:AbsoluteMove>");

    const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto state = m_mockTransport->getInternalState();
    EXPECT_EQ(state.panCentidegrees, 27000U);
    // Tilt must remain untouched at 4500
    EXPECT_EQ(state.tiltCentidegrees, 4500U);
}

TEST_F(PtzConformanceTest, PtzRelMoveGeneric)
{
    // RelativeMove with valid translation
    const std::string xml = makeSoapRequest("<tptz:RelativeMove>\r\n"
                                            "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                            "  <tptz:Translation>\r\n"
                                            "    <tt:PanTilt x=\"0.1\" y=\"-0.1\"/>\r\n"
                                            "  </tptz:Translation>\r\n"
                                            "</tptz:RelativeMove>");

    const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    // RelativeMove with NaN translation must fail with HTTP 400
    const std::string badXml = makeSoapRequest("<tptz:RelativeMove>\r\n"
                                               "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                               "  <tptz:Translation>\r\n"
                                               "    <tt:PanTilt x=\"NaN\" y=\"-0.1\"/>\r\n"
                                               "  </tptz:Translation>\r\n"
                                               "</tptz:RelativeMove>");

    const auto badRes = m_client->Post("/onvif/ptz_service", badXml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(badRes);
    EXPECT_EQ(badRes->status, 400);
    EXPECT_NE(badRes->body.find("ter:InvalidArgVal"), std::string::npos);
}

TEST_F(PtzConformanceTest, PtzAbsSpherical)
{
    // SphericalSpace specifies direct degrees: x=120.0 deg (12000 cDeg), y=30.0 deg (3000 cDeg)
    const std::string xml
        = makeSoapRequest("<tptz:AbsoluteMove>\r\n"
                          "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                          "  <tptz:Position>\r\n"
                          "    <tt:PanTilt x=\"120.0\" y=\"30.0\" "
                          "space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace\"/>\r\n"
                          "  </tptz:Position>\r\n"
                          "</tptz:AbsoluteMove>");

    const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto state = m_mockTransport->getInternalState();
    EXPECT_EQ(state.panCentidegrees, 12000U);
    EXPECT_EQ(state.tiltCentidegrees, 3000U);
}

// ============================================================================
// H10: ContinuousMove Timeout Auto-Stop
// ============================================================================

TEST_F(PtzConformanceTest, PtzTimeoutAutoStop)
{
    // 1. Send ContinuousMove with PT0.15S timeout (150 ms)
    const std::string xml = makeSoapRequest("<tptz:ContinuousMove>\r\n"
                                            "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                            "  <tptz:Velocity>\r\n"
                                            "    <tt:PanTilt x=\"1.0\" y=\"0.0\"/>\r\n"
                                            "  </tptz:Velocity>\r\n"
                                            "  <tptz:Timeout>PT0.15S</tptz:Timeout>\r\n"
                                            "</tptz:ContinuousMove>");

    const auto res = m_client->Post("/onvif/ptz_service", xml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    // Movement must be actively recorded by PTZ adapter
    EXPECT_TRUE(m_adapter->handleGetStatus().isMoving);

    // 2. Wait 250 ms (> 150 ms timeout + processing), auto-stop must trigger asynchronously
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_FALSE(m_adapter->handleGetStatus().isMoving);
}

TEST_F(PtzConformanceTest, PtzStopCancelsTimer)
{
    // 1. Send ContinuousMove with PT2S timeout (2000 ms)
    const std::string moveXml = makeSoapRequest("<tptz:ContinuousMove>\r\n"
                                                "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                                "  <tptz:Velocity>\r\n"
                                                "    <tt:PanTilt x=\"1.0\" y=\"0.0\"/>\r\n"
                                                "  </tptz:Velocity>\r\n"
                                                "  <tptz:Timeout>PT2S</tptz:Timeout>\r\n"
                                                "</tptz:ContinuousMove>");

    const auto resMove = m_client->Post("/onvif/ptz_service", moveXml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(resMove);
    EXPECT_EQ(resMove->status, 200);
    EXPECT_TRUE(m_adapter->handleGetStatus().isMoving);

    // 2. Explicitly send Stop
    const std::string stopXml = makeSoapRequest("<tptz:Stop>\r\n"
                                                "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                                "  <tptz:PanTilt>true</tptz:PanTilt>\r\n"
                                                "  <tptz:Zoom>true</tptz:Zoom>\r\n"
                                                "</tptz:Stop>");

    const auto resStop = m_client->Post("/onvif/ptz_service", stopXml, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(resStop);
    EXPECT_EQ(resStop->status, 200);
    EXPECT_FALSE(m_adapter->handleGetStatus().isMoving);

    // 3. Wait another 150 ms to ensure background timer cancels cleanly without spurious activity
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_FALSE(m_adapter->handleGetStatus().isMoving);
}
