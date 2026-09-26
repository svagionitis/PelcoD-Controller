#include "PayloadHal.h"
#include "TourEngine.h"
#include "LocalPresetManager.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>

namespace PayloadHal {
namespace {

TEST(TestTourEngine, TourRegistrationAndManagement)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    auto mgr = std::make_shared<LocalPresetManager>(ptu, cam);
    TourEngine engine(ptu, cam, mgr);

    EXPECT_TRUE(engine.listTours().empty());
    EXPECT_FALSE(engine.getTour("tour1").has_value());

    // Reject empty tourId or empty waypoints
    TourDefinition invalid1 {};
    invalid1.tourId = "";
    invalid1.waypoints.push_back({ 1, std::chrono::milliseconds(1000), 1.0f });
    EXPECT_FALSE(engine.registerTour(invalid1));

    TourDefinition invalid2 {};
    invalid2.tourId = "valid_id";
    invalid2.waypoints.clear();
    EXPECT_FALSE(engine.registerTour(invalid2));

    // Register valid tour
    TourDefinition tour {};
    tour.tourId = "perimeter_patrol";
    tour.name = "Perimeter Patrol";
    tour.loop = true;
    tour.waypoints = {
        { 1, std::chrono::milliseconds(200), 1.0f },
        { 2, std::chrono::milliseconds(200), 1.0f }
    };

    EXPECT_TRUE(engine.registerTour(tour));
    EXPECT_EQ(engine.listTours().size(), 1U);

    auto fetched = engine.getTour("perimeter_patrol");
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->name, "Perimeter Patrol");
    EXPECT_EQ(fetched->waypoints.size(), 2U);

    // Remove tour
    EXPECT_TRUE(engine.removeTour("perimeter_patrol"));
    EXPECT_TRUE(engine.listTours().empty());
    EXPECT_FALSE(engine.removeTour("perimeter_patrol"));
}

TEST(TestTourEngine, StartAndStopTourLifecycle)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    auto mgr = std::make_shared<LocalPresetManager>(ptu, cam);

    PtzPreset p1 {};
    p1.id = 1;
    p1.panAngleDeg = 10.0;
    p1.tiltAngleDeg = -5.0;
    mgr->savePreset(p1);

    PtzPreset p2 {};
    p2.id = 2;
    p2.panAngleDeg = 20.0;
    p2.tiltAngleDeg = -10.0;
    mgr->savePreset(p2);

    TourEngine engine(ptu, cam, mgr);

    TourDefinition tour {};
    tour.tourId = "patrol1";
    tour.name = "Patrol 1";
    tour.loop = false;
    tour.waypoints = {
        { 1, std::chrono::milliseconds(100), 1.0f },
        { 2, std::chrono::milliseconds(100), 1.0f }
    };
    ASSERT_TRUE(engine.registerTour(tour));

    // Start unknown tour fails
    EXPECT_FALSE(engine.startTour("unknown"));

    // Start patrol1
    EXPECT_TRUE(engine.startTour("patrol1"));
    auto st = engine.status();
    EXPECT_EQ(st.activeTourId, "patrol1");
    EXPECT_TRUE(st.state == TourState::SlewingToWaypoint || st.state == TourState::DwellingAtWaypoint);

    // Stop tour
    engine.stopTour();
    st = engine.status();
    EXPECT_EQ(st.state, TourState::Idle);
    EXPECT_TRUE(st.activeTourId.empty());
}

TEST(TestTourEngine, ManualInterventionAndPauseResume)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    auto mgr = std::make_shared<LocalPresetManager>(ptu, cam);

    PtzPreset p1 {};
    p1.id = 1;
    p1.panAngleDeg = 45.0;
    p1.tiltAngleDeg = 0.0;
    mgr->savePreset(p1);

    TourEngine engine(ptu, cam, mgr);

    TourDefinition tour {};
    tour.tourId = "guard";
    tour.name = "Guard";
    tour.waypoints = {
        { 1, std::chrono::milliseconds(500), 1.0f }
    };
    ASSERT_TRUE(engine.registerTour(tour));
    ASSERT_TRUE(engine.startTour("guard"));

    // Manual intervention trigger
    engine.notifyManualIntervention();
    auto st = engine.status();
    EXPECT_EQ(st.state, TourState::Paused);

    // Resume tour
    EXPECT_TRUE(engine.resumeTour());
    st = engine.status();
    EXPECT_EQ(st.state, TourState::SlewingToWaypoint);

    // Pause tour manually
    EXPECT_TRUE(engine.pauseTour());
    st = engine.status();
    EXPECT_EQ(st.state, TourState::Paused);

    engine.stopTour();
}

TEST(TestTourEngine, FullTourProgressionAndCompletion)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    auto mgr = std::make_shared<LocalPresetManager>(ptu, cam);

    // Two presets
    PtzPreset p1 {};
    p1.id = 1;
    p1.panAngleDeg = 15.0;
    p1.tiltAngleDeg = 0.0;
    mgr->savePreset(p1);

    PtzPreset p2 {};
    p2.id = 2;
    p2.panAngleDeg = 30.0;
    p2.tiltAngleDeg = 0.0;
    mgr->savePreset(p2);

    TourEngine engine(ptu, cam, mgr);
    engine.setArrivalThresholdDeg(1.0);
    EXPECT_NEAR(engine.arrivalThresholdDeg(), 1.0, 0.01);

    std::vector<TourState> observedStates;
    engine.registerStatusCallback([&](const TourStatus& status) {
        observedStates.push_back(status.state);
    });

    TourDefinition tour {};
    tour.tourId = "quick_tour";
    tour.name = "Quick Tour";
    tour.loop = false; // single pass
    tour.waypoints = {
        { 1, std::chrono::milliseconds(100), 1.0f },
        { 2, std::chrono::milliseconds(100), 1.0f }
    };
    ASSERT_TRUE(engine.registerTour(tour));

    ASSERT_TRUE(engine.startTour("quick_tour"));

    // Poll until completed or timeout
    const auto startWait = std::chrono::steady_clock::now();
    while (engine.status().state != TourState::Completed &&
           std::chrono::steady_clock::now() - startWait < std::chrono::seconds(3)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const auto finalStatus = engine.status();
    EXPECT_EQ(finalStatus.state, TourState::Completed);
    EXPECT_GE(finalStatus.completedLoops, 1U);
    EXPECT_FALSE(observedStates.empty());
}

} // namespace
} // namespace PayloadHal
