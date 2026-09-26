#include "PayloadHal.h"
#include "AtmosphericRefractionCompensator.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <cmath>

namespace PayloadHal {
namespace {

TEST(TestAtmosphericRefractionCompensator, ZeroDistanceIdentity)
{
    AtmosphericRefractionCompensator comp;
    EXPECT_DOUBLE_EQ(comp.refractionAngle(0.0), 0.0);
    EXPECT_DOUBLE_EQ(comp.apparentToTrueElevation(5.0, 0.0), 5.0);
    EXPECT_DOUBLE_EQ(comp.trueToApparentElevation(5.0, 0.0), 5.0);
    EXPECT_DOUBLE_EQ(AtmosphericRefractionCompensator::geometricEarthDrop(0.0), 0.0);
    EXPECT_DOUBLE_EQ(comp.effectiveEarthDrop(0.0), 0.0);
}

TEST(TestAtmosphericRefractionCompensator, EarthCurvatureDropCalculation)
{
    AtmosphericRefractionCompensator comp;
    const double d = 20000.0; // 20 km

    // Geometric drop: d^2 / (2 * R_E) ~ 4e8 / (2 * 6371008.8) ~ 31.39 meters
    const double geomDrop = AtmosphericRefractionCompensator::geometricEarthDrop(d);
    EXPECT_NEAR(geomDrop, 31.39, 0.1);

    // Effective drop with k ~ 1.17: ~ 31.39 / 1.17 ~ 26.83 meters
    const double effDrop = comp.effectiveEarthDrop(d);
    EXPECT_NEAR(effDrop, 26.83, 0.5);
    EXPECT_LT(effDrop, geomDrop); // Ray curvature reduces effective drop
}

TEST(TestAtmosphericRefractionCompensator, ApparentToTrueElevationOffset)
{
    AtmosphericRefractionCompensator comp;
    const double range = 20000.0; // 20 km
    const double apparentEl = 0.0; // Pointing at horizon

    const double refrAngle = comp.refractionAngle(range, apparentEl);
    EXPECT_GT(refrAngle, 0.010);
    EXPECT_LT(refrAngle, 0.030); // ~ 40 to 80 arcseconds

    const double trueEl = comp.apparentToTrueElevation(apparentEl, range);
    EXPECT_LT(trueEl, apparentEl);
    EXPECT_NEAR(trueEl, -refrAngle, 1e-6);
}

TEST(TestAtmosphericRefractionCompensator, RoundTripInvertibility)
{
    AtmosphericRefractionCompensator comp;
    const std::vector<double> ranges = {1000.0, 5000.0, 15000.0, 30000.0, 40000.0};
    const std::vector<double> elevations = {-15.0, -5.0, 0.0, 5.0, 20.0};

    for (double r : ranges) {
        for (double el : elevations) {
            const double trueEl = comp.apparentToTrueElevation(el, r);
            const double recoveredAppEl = comp.trueToApparentElevation(trueEl, r);
            EXPECT_NEAR(recoveredAppEl, el, 1e-6);
        }
    }
}

TEST(TestAtmosphericRefractionCompensator, WavelengthDispersionScaling)
{
    AtmosphericRefractionCompensator comp;

    const double nVis = comp.refractivity(OpticalBand::Visible);
    const double nSwir = comp.refractivity(OpticalBand::Swir);
    const double nMwir = comp.refractivity(OpticalBand::Mwir);
    const double nLwir = comp.refractivity(OpticalBand::Lwir);

    // Visible light experiences higher atmospheric refractivity than thermal infrared
    EXPECT_GT(nVis, nSwir);
    EXPECT_GT(nSwir, nMwir);
    EXPECT_GT(nMwir, nLwir);

    const double range = 25000.0;
    const double refrVis = comp.refractionAngle(range, 0.0, OpticalBand::Visible);
    const double refrLwir = comp.refractionAngle(range, 0.0, OpticalBand::Lwir);

    EXPECT_GT(refrVis, refrLwir);
}

TEST(TestAtmosphericRefractionCompensator, OpticalHorizonDistance)
{
    AtmosphericRefractionCompensator comp;

    // Observer at 100m MSL, sea level target (0m MSL)
    // d = sqrt(2 * k * R_E * 100m) ~ sqrt(2 * 1.17 * 6371008 * 100) ~ 38,600 meters
    const double d100 = comp.opticalHorizonDistance(100.0, 0.0);
    EXPECT_NEAR(d100, 38600.0, 600.0);

    // Target elevated to 25m MSL extends horizon by sqrt(2 * k * R_E * 25m) ~ 19,300m
    const double d100_25 = comp.opticalHorizonDistance(100.0, 25.0);
    EXPECT_NEAR(d100_25, 38600.0 + 19300.0, 900.0);
}

TEST(TestAtmosphericRefractionCompensator, OverTheHorizonOcclusion)
{
    AtmosphericRefractionCompensator comp;

    // Observer at 25m MSL, sea level target (0m MSL): horizon ~ 19.3 km
    const double obsAlt = 25.0;
    const double tgtAlt = 0.0;

    // Target at 10 km is clearly visible
    EXPECT_TRUE(comp.isTargetVisibleOverHorizon(obsAlt, tgtAlt, 10000.0));

    // Target at 25 km is below the horizon
    EXPECT_FALSE(comp.isTargetVisibleOverHorizon(obsAlt, tgtAlt, 25000.0));

    // Evaluate full refraction
    RefractionCorrectionResult res = comp.evaluateRefraction(obsAlt, 0.0, 25000.0);
    EXPECT_TRUE(res.targetBelowHorizon);
    EXPECT_GT(res.earthCurvatureDropMeters, 40.0);
}

TEST(TestAtmosphericRefractionCompensator, SimulatedPayloadIntegration)
{
    SimulatedPayload payload;
    ASSERT_TRUE(payload.connect());

    auto comp = payload.atmosphericCompensator();
    ASSERT_NE(comp, nullptr);

    AtmosphericEnvironment env = comp->environment();
    EXPECT_TRUE(env.isValid());
    EXPECT_DOUBLE_EQ(env.pressureHpa, 1013.25);

    // Modify environment
    env.temperatureCelsius = 25.0;
    env.pressureHpa = 1005.0;
    comp->setEnvironment(env);
    EXPECT_DOUBLE_EQ(comp->environment().temperatureCelsius, 25.0);

    const double trueEl = comp->apparentToTrueElevation(0.0, 10000.0);
    EXPECT_LT(trueEl, 0.0);

    payload.disconnect();
}

} // namespace
} // namespace PayloadHal
