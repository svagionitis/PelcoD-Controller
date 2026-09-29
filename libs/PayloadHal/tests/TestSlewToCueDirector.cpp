#include "AutoFramingController.h"
#include "GeoLockController.h"
#include "Nmea/NmeaChecksum.h"
#include "Nmea/NmeaDevice.h"
#include "NmeaSlavingBridge.h"
#include "PayloadAutoTrackerBridge.h"
#include "SlewToCueDirector.h"
#include "TargetThreatEvaluator.h"
#include "Transport/BaseTransport.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PayloadHal {
namespace {

    class MockTransport : public Transport::BaseTransport {
    public:
        bool open() override
        {
            m_isOpen = true;
            return true;
        }

        void close() override
        {
            m_isOpen = false;
        }

        bool isOpen() const noexcept override
        {
            return m_isOpen;
        }

        bool sendData(const std::vector<std::uint8_t>& data) override
        {
            (void)data;
            return true;
        }

        void injectString(std::string_view text)
        {
            const std::vector<std::uint8_t> data(text.begin(), text.end());
            invokeDataCallback(data);
        }

    private:
        std::atomic<bool> m_isOpen { false };
    };

    TEST(TestSlewToCueDirector, AutonomousCueingLifecycle)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        auto slavingBridge = std::make_shared<NmeaSlavingBridge>(nmeaDevice, geoLock);
        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();
        auto framing = std::make_shared<AutoFramingController>();
        auto autoTracker = std::make_shared<PayloadAutoTrackerBridge>(payload);
        auto camera = payload->primaryCamera();

        SlewToCueConfig cfg;
        cfg.autonomousEngagement = true;
        cfg.minThreatScoreToCue = 30.0;
        cfg.opticalAcquisitionTimeout = std::chrono::milliseconds(20);
        cfg.inspectionDwellDuration = std::chrono::milliseconds(50); // fast for testing

        auto director = std::make_shared<SlewToCueDirector>(
            threatEvaluator, slavingBridge, framing, autoTracker, camera, geoLock, cfg);

        EXPECT_FALSE(director->isCueingActive());
        EXPECT_EQ(director->status().state, CueingState::Idle);

        // Inject own-ship position and heading
        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,120000,3700.000,N,12200.000,W,1,08,1.0,0.0,M,0.0,M,,");
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        transport->injectString(gga);
        transport->injectString(hdt);

        // Inject high threat radar contact closing at 25 knots from 1.0 NM
        const std::string ttm
            = Nmea::NmeaChecksum::frameSentence("RATTM,15,1.0,000.0,T,25.0,180.0,T,0.0,2.4,K,THREAT_CRAFT,T,,120000,A");
        transport->injectString(ttm);

        // Evaluate threats
        const auto snap = nmeaDevice->navSnapshot();
        const auto radarList = nmeaDevice->activeRadarTargets();
        const auto aisList = nmeaDevice->activeAisTargets();
        threatEvaluator->evaluate(snap, radarList, aisList);

        // First update: autonomously detects high threat and transitions to SlewingToTarget
        director->update();
        EXPECT_TRUE(director->isCueingActive());
        EXPECT_EQ(director->status().state, CueingState::SlewingToTarget);
        EXPECT_EQ(director->status().activeTargetId, 15U);

        // Second update: gimbal converges, transitions to FramingTarget
        director->update();
        EXPECT_EQ(director->status().state, CueingState::FramingTarget);

        // Third update: frames target and transitions to AcquiringOpticalLock
        director->update();
        EXPECT_EQ(director->status().state, CueingState::AcquiringOpticalLock);

        // Wait for optical acquisition timeout (20ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        director->update();
        EXPECT_EQ(director->status().state, CueingState::DwellInspection);

        // Wait for inspection dwell duration (50ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        director->update(); // transitions to TargetCompleted
        director->update(); // transitions to Idle

        EXPECT_FALSE(director->isCueingActive());
        EXPECT_EQ(director->status().state, CueingState::Idle);
    }

    TEST(TestSlewToCueDirector, EmergencyBeaconImmediatePreemption)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        auto slavingBridge = std::make_shared<NmeaSlavingBridge>(nmeaDevice, geoLock);
        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();
        auto framing = std::make_shared<AutoFramingController>();
        auto autoTracker = std::make_shared<PayloadAutoTrackerBridge>(payload);
        auto camera = payload->primaryCamera();

        SlewToCueConfig cfg;
        cfg.autonomousEngagement = true;
        cfg.allowPreemptionByHigherThreat = true;

        auto director = std::make_shared<SlewToCueDirector>(
            threatEvaluator, slavingBridge, framing, autoTracker, camera, geoLock, cfg);

        // Inject own-ship position
        transport->injectString(
            Nmea::NmeaChecksum::frameSentence("GPGGA,120000,3700.000,N,12200.000,W,1,08,1.0,0.0,M,0.0,M,,"));
        transport->injectString(Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T"));

        // Currently tracking standard radar target #5
        director->cueRadarTarget(5U);
        EXPECT_TRUE(director->isCueingActive());
        EXPECT_EQ(director->status().activeTargetId, 5U);
        EXPECT_FALSE(director->status().isEmergencyTarget);

        // Emergency AIS-SART burst arrives
        // !AIVDM Message 1 with MMSI 970012345
        Nmea::AisVesselTarget sart;
        sart.mmsi = 970012345;
        sart.coordinates.latitudeDeg = 37.01;
        sart.coordinates.longitudeDeg = -122.01;
        sart.beaconType = Nmea::AisBeaconType::AisSart;
        sart.isEmergencyBeacon = true;

        const auto snap = nmeaDevice->navSnapshot();
        threatEvaluator->evaluate(snap, nmeaDevice->activeRadarTargets(), { sart });

        // Update director: should pre-empt radar target #5 with AIS-SART
        director->update();

        const auto st = director->status();
        EXPECT_EQ(st.activeTargetId, 970012345U);
        EXPECT_TRUE(st.isEmergencyTarget);
        EXPECT_TRUE(director->isCueingActive());
    }

    TEST(TestSlewToCueDirector, ManualCueAndDismiss)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        auto slavingBridge = std::make_shared<NmeaSlavingBridge>(nmeaDevice, geoLock);
        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();

        SlewToCueDirector director(threatEvaluator, slavingBridge);

        EXPECT_FALSE(director.isCueingActive());

        // Manually cue AIS target
        const bool cued = director.cueAisTarget(244012345U);
        EXPECT_TRUE(cued);
        EXPECT_TRUE(director.isCueingActive());
        EXPECT_EQ(director.status().activeTargetId, 244012345U);

        // Operator dismisses target
        director.dismissActiveTarget();
        EXPECT_EQ(director.status().state, CueingState::TargetCompleted);

        director.update();
        EXPECT_EQ(director.status().state, CueingState::Idle);
        EXPECT_FALSE(director.isCueingActive());
    }

    TEST(TestSlewToCueDirector, ZoomConvergenceTrackingHandover)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        auto slavingBridge = std::make_shared<NmeaSlavingBridge>(nmeaDevice, geoLock);
        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();

        AutoFramingConfig frameCfg {};
        frameCfg.maxZoomVelocityPerSec = 0.20; // 20% per second slew
        auto framing = std::make_shared<AutoFramingController>(frameCfg);

        auto autoTracker = std::make_shared<PayloadAutoTrackerBridge>(payload);
        auto camera = payload->primaryCamera();

        SlewToCueConfig cfg {};
        cfg.autonomousEngagement = true;
        cfg.waitForZoomConvergence = true;
        cfg.maxFramingDuration = std::chrono::milliseconds(3000);
        cfg.opticalAcquisitionTimeout = std::chrono::milliseconds(50);
        cfg.inspectionDwellDuration = std::chrono::milliseconds(50);

        auto director = std::make_shared<SlewToCueDirector>(
            threatEvaluator, slavingBridge, framing, autoTracker, camera, geoLock, cfg);

        // Inject own-ship position and heading
        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,120000,3700.000,N,12200.000,W,1,08,1.0,0.0,M,0.0,M,,");
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        transport->injectString(gga);
        transport->injectString(hdt);

        // Inject radar target at 1.5 NM
        const std::string ttm
            = Nmea::NmeaChecksum::frameSentence("RATTM,22,1.5,045.0,T,20.0,225.0,T,0.0,3.0,K,FAST_PATROL,T,,120000,A");
        transport->injectString(ttm);

        const auto snap = nmeaDevice->navSnapshot();
        threatEvaluator->evaluate(snap, nmeaDevice->activeRadarTargets(), nmeaDevice->activeAisTargets());

        // Update 1: SlewingToTarget
        director->update();
        EXPECT_EQ(director->status().state, CueingState::SlewingToTarget);

        // Update 2: Gimbal converges, transitions to FramingTarget
        director->update();
        EXPECT_EQ(director->status().state, CueingState::FramingTarget);
        EXPECT_FALSE(framing->isZoomConverged());

        // Small time step: zoom is still traveling -> state MUST remain FramingTarget
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        director->update();
        EXPECT_EQ(director->status().state, CueingState::FramingTarget);
        EXPECT_FALSE(autoTracker->isEngaged());

        // Settle zoom by stepping framing controller forward (1.0 travel at 0.20/s needs >= 5000ms)
        framing->update(*camera, std::chrono::milliseconds(6000));
        EXPECT_TRUE(framing->isZoomConverged());

        // Now director update detects zoom convergence -> transitions to AcquiringOpticalLock and engages tracker
        director->update();
        EXPECT_EQ(director->status().state, CueingState::AcquiringOpticalLock);
        EXPECT_TRUE(autoTracker->isEngaged());
    }

} // namespace
} // namespace PayloadHal
