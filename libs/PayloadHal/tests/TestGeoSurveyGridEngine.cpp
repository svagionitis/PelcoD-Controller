#include "GeoSurveyGridEngine.h"
#include "GeoSurveyGridTypes.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <memory>
#include <vector>

using namespace PayloadHal;

// Mock PTU
class MockPtu : public IPanTiltUnit {
public:
    MockPtu() = default;

    bool connect() override { return true; }
    void disconnect() override {}
    [[nodiscard]] bool isConnected() const noexcept override { return true; }
    [[nodiscard]] DeviceState state() const noexcept override { return DeviceState::Ready; }
    [[nodiscard]] DeviceInfo info() const noexcept override { return DeviceInfo{"Mock", "PTU", "1.0"}; }
    void registerStateCallback(StateCallback) override {}

    bool setRate(double pan, double tilt) override { m_panRate = pan; m_tiltRate = tilt; return true; }
    bool setNormalizedVelocity(float, float) override { return true; }
    bool setAbsoluteAngles(double pan, double tilt) override { m_pan = pan; m_tilt = tilt; return true; }
    bool setRelativeNudge(double, double) override { return true; }
    bool stopMotion() override { m_panRate = 0.0; m_tiltRate = 0.0; return true; }

    [[nodiscard]] bool supportsStabilization() const noexcept override { return false; }
    bool setStabilizationMode(StabilizationMode) override { return false; }
    [[nodiscard]] StabilizationMode stabilizationMode() const noexcept override { return StabilizationMode::Disabled; }
    bool zeroGyroDrift() override { return false; }

    bool getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const override
    {
        minPan = -180.0; maxPan = 180.0; minTilt = -90.0; maxTilt = 45.0;
        return true;
    }
    bool savePreset(std::uint8_t, const std::string&) override { return false; }
    bool recallPreset(std::uint8_t) override { return false; }

    void registerTelemetryCallback(TelemetryCallback) override {}

    [[nodiscard]] GimbalTelemetry currentTelemetry() const override
    {
        GimbalTelemetry t {};
        t.panAngleDeg = m_pan;
        t.tiltAngleDeg = m_tilt;
        t.panRateDegPerSec = m_panRate;
        t.tiltRateDegPerSec = m_tiltRate;
        return t;
    }

    double m_pan { 0.0 };
    double m_tilt { -30.0 };
    double m_panRate { 0.0 };
    double m_tiltRate { 0.0 };
};

// Mock DEM
class MockDem : public IDemProvider {
public:
    [[nodiscard]] bool hasCoverage(double, double) const noexcept override { return true; }
    [[nodiscard]] std::optional<double> getElevationM(double, double lon) const noexcept override
    {
        // Simple ridge at lon > -122.418
        if (lon > -122.418) {
            return 150.0;
        }
        return 20.0;
    }
    [[nodiscard]] double minElevationM() const noexcept override { return 20.0; }
    [[nodiscard]] double maxElevationM() const noexcept override { return 150.0; }
};

// Helper creating a simple rectangular survey polygon (~1 km x 1 km)
std::vector<Klv::GeoPoint2D> makeSquarePolygon(double centerLat, double centerLon, double deltaLat = 0.01, double deltaLon = 0.01)
{
    return {
        { centerLat - deltaLat, centerLon - deltaLon },
        { centerLat - deltaLat, centerLon + deltaLon },
        { centerLat + deltaLat, centerLon + deltaLon },
        { centerLat + deltaLat, centerLon - deltaLon }
    };
}

// =============================================================================
// Test 1: Point in Polygon & Area Calculations
// =============================================================================

TEST(TestGeoSurveyGridEngine, PointInPolygonAndArea)
{
    const auto poly = makeSquarePolygon(37.7749, -122.4194, 0.01, 0.01);

    // Center point must be inside
    EXPECT_TRUE(GeoSurveyGridEngine::isPointInsidePolygon({ 37.7749, -122.4194 }, poly));

    // Distant point must be outside
    EXPECT_FALSE(GeoSurveyGridEngine::isPointInsidePolygon({ 37.8500, -122.4194 }, poly));
    EXPECT_FALSE(GeoSurveyGridEngine::isPointInsidePolygon({ 37.7749, -122.5000 }, poly));

    // Area should be positive and roughly (2 * 0.01 * 111km) * (2 * 0.01 * 88km) ~= 2.22km x 1.76km ~= 3.9e6 m²
    const double area = GeoSurveyGridEngine::calculatePolygonAreaM2(poly);
    EXPECT_GT(area, 1.0e6);
    EXPECT_LT(area, 1.0e7);
}

// =============================================================================
// Test 2: Optimal Sweep Angle Calculation (OMBB)
// =============================================================================

TEST(TestGeoSurveyGridEngine, OmbbPrincipalAxisCalculation)
{
    // Elongated North-South rectangle: height (lat) = 0.04, width (lon) = 0.01
    const auto poly = makeSquarePolygon(37.7749, -122.4194, 0.04, 0.01);

    const double optimalHeading = GeoSurveyGridEngine::calculateOptimalSweepHeading(poly);
    // Should be close to 0 or 180 degrees (North-South direction)
    const double errorFromNorth = std::min(std::abs(optimalHeading - 0.0), std::abs(optimalHeading - 180.0));
    EXPECT_LT(errorFromNorth, 10.0);
}

// =============================================================================
// Test 3: Convex Polygon Slicing and Transect Generation
// =============================================================================

TEST(TestGeoSurveyGridEngine, ConvexPolygonSlicing)
{
    GeoSurveyGridEngine engine;
    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.005, 0.005);
    cfg.surveyAltitudeAglMeters = 200.0;
    cfg.sweepAngleDeg = 0.0; // North-South sweep
    cfg.cameraHfovDeg = 45.0;
    cfg.cameraVfovDeg = 25.0;
    cfg.sideOverlapRatio = 0.50;
    cfg.forwardOverlapRatio = 0.70;
    cfg.gridResolutionMeters = 10.0;
    engine.setPlanConfig(cfg);

    EXPECT_TRUE(engine.generatePlan());

    const auto transects = engine.plannedTransects();
    EXPECT_GE(transects.size(), 2U);
    EXPECT_GT(engine.totalPlanLengthMeters(), 0.0);

    // Each transect must have waypoints
    for (const auto& t : transects) {
        EXPECT_GE(t.waypoints.size(), 2U);
        EXPECT_GT(t.lengthMeters, 0.0);
    }
}

// =============================================================================
// Test 4: Concave Polygon Multi-Intersection Slicing
// =============================================================================

TEST(TestGeoSurveyGridEngine, ConcavePolygonMultiIntersection)
{
    GeoSurveyGridEngine engine;
    // L-shaped concave polygon
    std::vector<Klv::GeoPoint2D> lPoly = {
        { 37.770, -122.420 },
        { 37.770, -122.410 },
        { 37.775, -122.410 },
        { 37.775, -122.415 },
        { 37.780, -122.415 },
        { 37.780, -122.420 }
    };

    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = lPoly;
    cfg.surveyAltitudeAglMeters = 150.0;
    cfg.gridResolutionMeters = 15.0;
    engine.setPlanConfig(cfg);

    EXPECT_TRUE(engine.generatePlan());

    const auto transects = engine.plannedTransects();
    EXPECT_FALSE(transects.empty());

    // Verify waypoints are inside the polygon
    for (const auto& t : transects) {
        for (const auto& wp : t.waypoints) {
            EXPECT_TRUE(GeoSurveyGridEngine::isPointInsidePolygon(
                { wp.targetGroundPos.latitudeDeg, wp.targetGroundPos.longitudeDeg }, lPoly));
        }
    }
}

// =============================================================================
// Test 5: Adaptive GSD and DEM Elevation Ingestion
// =============================================================================

TEST(TestGeoSurveyGridEngine, AdaptiveGsdTerrainModeling)
{
    auto mockDem = std::make_shared<MockDem>();
    GeoSurveyGridEngine engine(nullptr, nullptr, mockDem);

    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.005, 0.005);
    cfg.surveyAltitudeAglMeters = 300.0;
    cfg.sensorWidthPixels = 3840;
    engine.setPlanConfig(cfg);

    EXPECT_TRUE(engine.generatePlan());

    const auto transects = engine.plannedTransects();
    ASSERT_FALSE(transects.empty());

    // Verify waypoints populated elevation from DEM
    for (const auto& t : transects) {
        for (const auto& wp : t.waypoints) {
            EXPECT_GT(wp.targetGroundPos.altitudeM, 0.0);
            EXPECT_GT(wp.expectedGsdMeters, 0.0);
            EXPECT_LT(wp.expectedGsdMeters, 1.0); // Sub-meter GSD
        }
    }
}

// =============================================================================
// Test 6: Boustrophedon Sweep Alternation
// =============================================================================

TEST(TestGeoSurveyGridEngine, BoustrophedonSweepAlternation)
{
    GeoSurveyGridEngine engine;
    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.005, 0.005);
    cfg.sweepAngleDeg = 0.0; // North
    cfg.patternType = SweepPatternType::Boustrophedon;
    engine.setPlanConfig(cfg);

    EXPECT_TRUE(engine.generatePlan());

    const auto transects = engine.plannedTransects();
    ASSERT_GE(transects.size(), 2U);

    // Transect 0 should head North (~0°), Transect 1 should head South (~180°)
    EXPECT_NEAR(transects[0].headingDeg, 0.0, 1.0);
    EXPECT_NEAR(transects[1].headingDeg, 180.0, 1.0);
}

// =============================================================================
// Test 7: Discrete Occupancy Grid Discretization
// =============================================================================

TEST(TestGeoSurveyGridEngine, OccupancyGridDiscretization)
{
    GeoSurveyGridEngine engine;
    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.004, 0.004);
    cfg.gridResolutionMeters = 10.0;
    engine.setPlanConfig(cfg);

    EXPECT_TRUE(engine.generatePlan());

    const auto grid = engine.occupancyGrid();
    EXPECT_TRUE(grid.isValid());
    EXPECT_GE(grid.rows, 10U);
    EXPECT_GE(grid.cols, 10U);

    // Verify coordinate mapping round-trip
    std::size_t r = 0U;
    std::size_t c = 0U;
    EXPECT_TRUE(grid.geoToGrid(37.7749, -122.4194, r, c));

    double lat = 0.0;
    double lon = 0.0;
    EXPECT_TRUE(grid.gridToGeo(r, c, lat, lon));
    EXPECT_NEAR(lat, 37.7749, 0.001);
    EXPECT_NEAR(lon, -122.4194, 0.001);

    // Center cell should be marked Unsurveyed (inside polygon)
    EXPECT_EQ(grid.cell(r, c), GridCellState::Unsurveyed);

    // Corner (0, 0) in the padded margin should be OutsidePolygon
    EXPECT_EQ(grid.cell(0, 0), GridCellState::OutsidePolygon);
}

// =============================================================================
// Test 8: Real-Time Frustum Footprint Stamping
// =============================================================================

TEST(TestGeoSurveyGridEngine, FootprintStampingAndShadowOcclusion)
{
    auto mockDem = std::make_shared<MockDem>();
    GeoSurveyGridEngine engine(nullptr, nullptr, mockDem);

    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.004, 0.004);
    cfg.gridResolutionMeters = 10.0;
    engine.setPlanConfig(cfg);

    EXPECT_TRUE(engine.generatePlan());

    auto initialMetrics = engine.coverageMetrics();
    EXPECT_DOUBLE_EQ(initialMetrics.coveragePercentage, 0.0);
    EXPECT_DOUBLE_EQ(initialMetrics.surveyedAreaM2, 0.0);

    // Create a frustum stamp inside the polygon
    Klv::FrustumCorners frustum {};
    frustum.topLeft = { 37.776, -122.421 };
    frustum.topRight = { 37.776, -122.418 };
    frustum.bottomRight = { 37.774, -122.418 };
    frustum.bottomLeft = { 37.774, -122.421 };

    Klv::GeoPoint3D platformPos { 37.775, -122.4194, 500.0 };
    engine.stampFootprint(frustum, platformPos);

    auto updatedMetrics = engine.coverageMetrics();
    EXPECT_GT(updatedMetrics.coveragePercentage, 0.0);
    EXPECT_GT(updatedMetrics.surveyedAreaM2, 0.0);
}

// =============================================================================
// Test 9: Execution State Machine & Control Interlocks
// =============================================================================

TEST(TestGeoSurveyGridEngine, StateMachineLifecycleAndControls)
{
    auto ptu = std::make_shared<MockPtu>();
    GeoSurveyGridEngine engine(ptu, nullptr, nullptr);

    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.002, 0.002);
    cfg.surveyAltitudeAglMeters = 100.0;
    cfg.forwardOverlapRatio = 0.20;
    cfg.sideOverlapRatio = 0.20;
    cfg.waypointDwellSec = 0.05;
    engine.setPlanConfig(cfg);

    EXPECT_EQ(engine.surveyState(), SurveyState::Idle);

    EXPECT_TRUE(engine.generatePlan());
    EXPECT_EQ(engine.surveyState(), SurveyState::Idle);

    // Start
    EXPECT_TRUE(engine.startSurvey());
    EXPECT_EQ(engine.surveyState(), SurveyState::Executing);

    // Pause
    EXPECT_TRUE(engine.pauseSurvey());
    EXPECT_EQ(engine.surveyState(), SurveyState::Paused);

    // Resume
    EXPECT_TRUE(engine.resumeSurvey());
    EXPECT_EQ(engine.surveyState(), SurveyState::Executing);

    // Skip transect
    EXPECT_TRUE(engine.skipToNextTransect());

    // Advance simulation to completion
    Klv::GeoPoint3D platformPos { 37.7749, -122.4194, 200.0 };
    for (int i = 0; i < 300; ++i) {
        engine.update(0.1, platformPos, 0.0);
        if (engine.surveyState() == SurveyState::Completed) {
            break;
        }
    }

    EXPECT_EQ(engine.surveyState(), SurveyState::Completed);
    EXPECT_DOUBLE_EQ(engine.coverageMetrics().coveragePercentage, 100.0);

    // Abort from completed is no-op
    engine.abortSurvey();
}

// =============================================================================
// Test 10: Integration with SimulatedPayload
// =============================================================================

TEST(TestGeoSurveyGridEngine, SimulatedPayloadIntegration)
{
    auto simPayload = std::make_shared<SimulatedPayload>();
    ASSERT_TRUE(simPayload->connect());

    auto surveyEngine = simPayload->geoSurveyGridEngine();
    ASSERT_NE(surveyEngine, nullptr);

    EXPECT_EQ(surveyEngine->surveyState(), SurveyState::Idle);

    SurveyPlanConfig cfg {};
    cfg.boundaryPolygon = makeSquarePolygon(37.7749, -122.4194, 0.003, 0.003);
    cfg.surveyAltitudeAglMeters = 200.0;
    surveyEngine->setPlanConfig(cfg);

    EXPECT_TRUE(surveyEngine->generatePlan());
    EXPECT_GT(surveyEngine->plannedTransects().size(), 0U);

    EXPECT_TRUE(surveyEngine->startSurvey());
    EXPECT_EQ(surveyEngine->surveyState(), SurveyState::Executing);

    // Update with platform motion
    Klv::GeoPoint3D platformPos { 37.7749, -122.4194, 300.0 };
    surveyEngine->update(0.1, platformPos, 45.0);

    surveyEngine->abortSurvey();
    EXPECT_EQ(surveyEngine->surveyState(), SurveyState::Aborted);

    simPayload->disconnect();
}
