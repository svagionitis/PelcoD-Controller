#include "Nmea/NmeaChecksum.h"
#include "Nmea/NmeaDevice.h"
#include "Nmea/NmeaSentenceBuilder.h"
#include "Nmea/NmeaSentenceParser.h"
#include "Nmea/route/NmeaRouteManager.h"
#include "PayloadHal/GeoLockController.h"
#include "PayloadHal/NmeaSlavingBridge.h"
#include "PayloadHal/sim/SimulatedPayload.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

namespace Nmea {
namespace {

    class MockTestTransport : public Transport::BaseTransport {
    public:
        MockTestTransport() = default;
        bool open() override
        {
            m_open.store(true);
            notifyState(Transport::TransportState::Connected, "Mock Connected");
            return true;
        }
        void close() override
        {
            m_open.store(false);
            notifyState(Transport::TransportState::Disconnected, "Mock Disconnected");
        }
        bool isOpen() const noexcept override
        {
            return m_open.load();
        }
        bool sendData(const std::vector<std::uint8_t>& data) override
        {
            m_sentData.push_back(data);
            return true;
        }
        void injectSentence(std::string_view sentence)
        {
            std::string toSend(sentence);
            if (toSend.empty() || toSend.back() != '\n') {
                toSend += "\r\n";
            }
            invokeDataCallback(std::vector<std::uint8_t>(toSend.begin(), toSend.end()));
        }

        std::vector<std::vector<std::uint8_t>> m_sentData {};

    private:
        std::atomic<bool> m_open { false };
    };

    TEST(TestNmeaRouteAndSar, RmbParsingAndBuilding)
    {
        // Valid RMB sentence:
        // Status: A, XTE: 0.25 NM, Steer: L, TO: WPT02, FROM: WPT01, Lat: 37 45.000 N, Lon: 122 25.000 W
        // Range: 4.8 NM, Bearing: 125.0 True, VMG: 10.5 kts, Arrival: V (not arrived), FAA: A (Autonomous)
        const std::string rmbSentence
            = NmeaChecksum::frameSentence("GPRMB,A,0.25,L,WPT02,WPT01,3745.000,N,12225.000,W,4.8,125.0,10.5,V,A");

        RmbData rmb {};
        ASSERT_TRUE(NmeaSentenceParser::parseRmb(rmbSentence, rmb, true));
        EXPECT_TRUE(rmb.valid);
        EXPECT_TRUE(rmb.statusActive);
        EXPECT_DOUBLE_EQ(rmb.crossTrackErrorNmi, 0.25);
        EXPECT_EQ(rmb.directionToSteer, 'L');
        EXPECT_EQ(rmb.destWaypointId, "WPT02");
        EXPECT_EQ(rmb.originWaypointId, "WPT01");
        EXPECT_NEAR(rmb.destCoordinates.latitudeDeg, 37.75, 1e-4);
        EXPECT_NEAR(rmb.destCoordinates.longitudeDeg, -122.41666, 1e-4);
        EXPECT_DOUBLE_EQ(rmb.rangeToDestNmi, 4.8);
        EXPECT_DOUBLE_EQ(rmb.bearingToDestTrueDeg, 125.0);
        EXPECT_DOUBLE_EQ(rmb.closingVelocityKnots, 10.5);
        EXPECT_FALSE(rmb.arrivalAlarm);
        EXPECT_EQ(rmb.faaMode, NmeaFaaMode::Autonomous);

        // Roundtrip serialization
        const std::string built = NmeaSentenceBuilder::buildRmb(rmb, "GP");
        EXPECT_TRUE(NmeaChecksum::validate(built));

        RmbData rmbRoundtrip {};
        ASSERT_TRUE(NmeaSentenceParser::parseRmb(built, rmbRoundtrip, true));
        EXPECT_TRUE(rmbRoundtrip.valid);
        EXPECT_TRUE(rmbRoundtrip.statusActive);
        EXPECT_NEAR(rmbRoundtrip.crossTrackErrorNmi, 0.25, 0.01);
        EXPECT_EQ(rmbRoundtrip.destWaypointId, "WPT02");
        EXPECT_EQ(rmbRoundtrip.originWaypointId, "WPT01");
        EXPECT_NEAR(rmbRoundtrip.destCoordinates.latitudeDeg, 37.75, 1e-3);
        EXPECT_NEAR(rmbRoundtrip.destCoordinates.longitudeDeg, -122.41666, 1e-3);
    }

    TEST(TestNmeaRouteAndSar, RteMultiSentenceReassembly)
    {
        NmeaRouteManager routeManager {};

        // Route with 6 waypoints spanning 3 sentences
        const std::string rte1 = NmeaChecksum::frameSentence("GPRTE,3,1,c,SAR_EXP_SQ,WPT01,WPT02");
        const std::string rte2 = NmeaChecksum::frameSentence("GPRTE,3,2,c,SAR_EXP_SQ,WPT03,WPT04");
        const std::string rte3 = NmeaChecksum::frameSentence("GPRTE,3,3,c,SAR_EXP_SQ,WPT05,WPT06");

        RteData d1 {};
        ASSERT_TRUE(NmeaSentenceParser::parseRte(rte1, d1, true));
        EXPECT_EQ(d1.totalSentences, 3U);
        EXPECT_EQ(d1.sentenceNumber, 1U);
        EXPECT_EQ(d1.waypointIds.size(), 2U);
        EXPECT_FALSE(routeManager.ingestRte(d1)); // Not complete yet

        RteData d2 {};
        ASSERT_TRUE(NmeaSentenceParser::parseRte(rte2, d2, true));
        EXPECT_FALSE(routeManager.ingestRte(d2)); // Still not complete

        RteData d3 {};
        ASSERT_TRUE(NmeaSentenceParser::parseRte(rte3, d3, true));
        EXPECT_TRUE(routeManager.ingestRte(d3)); // Completed!

        const auto routeOpt = routeManager.route("SAR_EXP_SQ");
        ASSERT_TRUE(routeOpt.has_value());
        EXPECT_EQ(routeOpt->routeName, "SAR_EXP_SQ");
        EXPECT_TRUE(routeOpt->isComplete);
        ASSERT_EQ(routeOpt->waypoints.size(), 6U);
        EXPECT_EQ(routeOpt->waypoints[0].id, "WPT01");
        EXPECT_EQ(routeOpt->waypoints[1].id, "WPT02");
        EXPECT_EQ(routeOpt->waypoints[2].id, "WPT03");
        EXPECT_EQ(routeOpt->waypoints[3].id, "WPT04");
        EXPECT_EQ(routeOpt->waypoints[4].id, "WPT05");
        EXPECT_EQ(routeOpt->waypoints[5].id, "WPT06");
    }

    TEST(TestNmeaRouteAndSar, WplWaypointResolution)
    {
        NmeaRouteManager routeManager {};

        // Ingest WPL waypoints
        const std::string wpl1 = NmeaChecksum::frameSentence("GPWPL,3745.000,N,12225.000,W,WPT01");
        const std::string wpl2 = NmeaChecksum::frameSentence("GPWPL,3746.000,N,12224.000,W,WPT02");

        WplData w1 {};
        ASSERT_TRUE(NmeaSentenceParser::parseWpl(wpl1, w1, true));
        routeManager.ingestWpl(w1);

        WplData w2 {};
        ASSERT_TRUE(NmeaSentenceParser::parseWpl(wpl2, w2, true));
        routeManager.ingestWpl(w2);

        const auto wpt1Opt = routeManager.waypoint("WPT01");
        ASSERT_TRUE(wpt1Opt.has_value());
        EXPECT_NEAR(wpt1Opt->latitudeDeg, 37.75, 1e-4);
        EXPECT_NEAR(wpt1Opt->longitudeDeg, -122.41666, 1e-4);

        // Now ingest a 1-sentence route containing these waypoints
        const std::string rte = NmeaChecksum::frameSentence("GPRTE,1,1,w,PATROL_A,WPT01,WPT02");
        RteData rd {};
        ASSERT_TRUE(NmeaSentenceParser::parseRte(rte, rd, true));
        ASSERT_TRUE(routeManager.ingestRte(rd));

        const auto activeRoute = routeManager.activeRoute();
        ASSERT_TRUE(activeRoute.has_value());
        EXPECT_EQ(activeRoute->routeName, "PATROL_A");
        ASSERT_EQ(activeRoute->waypoints.size(), 2U);
        ASSERT_TRUE(activeRoute->waypoints[0].coordinates.has_value());
        EXPECT_NEAR(activeRoute->waypoints[0].coordinates->latitudeDeg, 37.75, 1e-4);
        ASSERT_TRUE(activeRoute->waypoints[1].coordinates.has_value());
        EXPECT_NEAR(activeRoute->waypoints[1].coordinates->latitudeDeg, 37.76666, 1e-4);
    }

    TEST(TestNmeaRouteAndSar, LookAheadAndSarSweepCalculation)
    {
        NmeaRouteManager routeManager {};

        // Establish WPT01 and WPT02
        routeManager.setWaypoint("WPT01", { 37.0, -122.0 });
        routeManager.setWaypoint("WPT02", { 37.0, -121.0 }); // Direct East

        // Establish RMB active leg from WPT01 to WPT02
        RmbData rmb {};
        rmb.statusActive = true;
        rmb.originWaypointId = "WPT01";
        rmb.destWaypointId = "WPT02";
        rmb.destCoordinates = { 37.0, -121.0 };
        rmb.rangeToDestNmi = 47.0;
        rmb.bearingToDestTrueDeg = 90.0;
        rmb.valid = true;
        routeManager.ingestRmb(rmb);

        EXPECT_TRUE(routeManager.hasActiveLeg());

        // Compute 10,000 meters look ahead from WPT01 along leg (due East)
        const auto lookOpt = routeManager.computeLookAhead(10000.0);
        ASSERT_TRUE(lookOpt.has_value());
        EXPECT_NEAR(lookOpt->latitudeDeg, 37.0, 1e-3);
        EXPECT_GT(lookOpt->longitudeDeg, -122.0); // Shifted East

        // Compute SAR sweep point: 500m forward, 200m to the right (+90 deg -> South)
        const NmeaCoordinates shipPos { 37.0, -122.0 };
        const auto sweepRight = routeManager.computeSarSweepPoint(shipPos, 200.0, 500.0);
        ASSERT_TRUE(sweepRight.has_value());
        EXPECT_GT(sweepRight->longitudeDeg, -122.0); // Forward East
        EXPECT_LT(sweepRight->latitudeDeg, 37.0); // Displaced South (Right of East)

        // Compute SAR sweep point: 500m forward, 200m to the left (-90 deg -> North)
        const auto sweepLeft = routeManager.computeSarSweepPoint(shipPos, -200.0, 500.0);
        ASSERT_TRUE(sweepLeft.has_value());
        EXPECT_GT(sweepLeft->longitudeDeg, -122.0); // Forward East
        EXPECT_GT(sweepLeft->latitudeDeg, 37.0); // Displaced North (Left of East)
    }

    TEST(TestNmeaRouteAndSar, NmeaDeviceRouteDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> rmbDispatched { false };
        device.addRmbCallback([&rmbDispatched](const RmbData& data) {
            if (data.destWaypointId == "WPT02") {
                rmbDispatched.store(true);
            }
        });

        std::atomic<bool> wplDispatched { false };
        device.addWplCallback([&wplDispatched](const WplData& data) {
            if (data.waypointId == "WPT02") {
                wplDispatched.store(true);
            }
        });

        transport->injectSentence(NmeaChecksum::frameSentence("GPWPL,3745.000,N,12225.000,W,WPT02"));
        EXPECT_TRUE(wplDispatched.load());

        transport->injectSentence(
            NmeaChecksum::frameSentence("GPRMB,A,0.10,R,WPT02,WPT01,3745.000,N,12225.000,W,5.0,090.0,12.0,V,A"));
        EXPECT_TRUE(rmbDispatched.load());

        const auto rmbOpt = device.lastRmb();
        ASSERT_TRUE(rmbOpt.has_value());
        EXPECT_EQ(rmbOpt->destWaypointId, "WPT02");
        EXPECT_DOUBLE_EQ(rmbOpt->crossTrackErrorNmi, 0.10);
    }

    TEST(TestNmeaRouteAndSar, NmeaSlavingBridgeRouteLegAndSweep)
    {
        auto transport = std::make_shared<MockTestTransport>();
        auto device = std::make_shared<NmeaDevice>(transport);
        ASSERT_TRUE(device->start());

        // Platform navigation at (37.0, -122.0) heading 090 True
        transport->injectSentence("$GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42");
        transport->injectSentence("$HEHDT,090.0,T*1B");

        auto payload = std::make_shared<PayloadHal::SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        auto geoLock = std::make_shared<PayloadHal::GeoLockController>(payload);
        PayloadHal::NmeaSlavingBridge bridge(device, geoLock);

        RmbData rmb {};
        rmb.statusActive = true;
        rmb.destWaypointId = "DEST";
        rmb.originWaypointId = "ORIG";
        rmb.destCoordinates = { 37.05, -121.95 };
        rmb.rangeToDestNmi = 4.0;
        rmb.bearingToDestTrueDeg = 045.0;
        rmb.closingVelocityKnots = 15.0;
        rmb.valid = true;

        // 1. Slave to Route Leg
        ASSERT_TRUE(bridge.slaveToRouteLeg(rmb));
        EXPECT_TRUE(bridge.isSlaving());
        auto st = bridge.status();
        EXPECT_EQ(st.targetType, PayloadHal::MarineTargetType::RouteLeg);
        EXPECT_NEAR(st.targetPosition->latitudeDeg, 37.05, 1e-4);

        // 2. Slave to SAR Sweep
        ASSERT_TRUE(bridge.slaveToSarSweep(rmb, 150.0, 400.0, 4.0));
        st = bridge.status();
        EXPECT_EQ(st.targetType, PayloadHal::MarineTargetType::SarSweep);

        // Exercise periodic update
        bridge.update();
        st = bridge.status();
        EXPECT_TRUE(st.active);
        EXPECT_EQ(st.targetType, PayloadHal::MarineTargetType::SarSweep);
    }

} // namespace
} // namespace Nmea
