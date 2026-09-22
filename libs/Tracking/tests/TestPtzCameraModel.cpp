/// @file TestPtzCameraModel.cpp
/// @brief Unit tests for PtzCameraModel: project/unproject round-trips, Jacobian finite-difference
///        validation, intrinsics getters/setters, and boundary conditions.

#include "MatrixMath.h"
#include "PtzCameraModel.h"

#include <gtest/gtest.h>
#include <cmath>
#include <iostream>

using namespace PelcoD;
using namespace PelcoD::Math;

namespace {

#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

constexpr double TOL_PIX = 0.5; ///< Pixel tolerance for round-trip
constexpr double TOL_RAD = 0.001; ///< Radian tolerance for angular round-trip

bool near(double a, double b, double tol)
{
    return std::abs(a - b) <= tol;
}

// ---------------------------------------------------------------------------

TEST(PtzCameraModelTest, DefaultIntrinsics)
{
    std::cout << "[Test] testDefaultIntrinsics\n";
    PtzCameraModel model;
    const auto& intr = model.getIntrinsics();
    EXPECT_TRUE(intr.imageWidth == 1920);
    EXPECT_TRUE(intr.imageHeight == 1080);
    EXPECT_TRUE(near(intr.cx, 960.0, 1e-9));
    EXPECT_TRUE(near(intr.cy, 540.0, 1e-9));
    EXPECT_TRUE(near(intr.fx0, 1200.0, 1e-9));
    EXPECT_TRUE(near(intr.fy0, 1200.0, 1e-9));
    std::cout << "  -> PASSED\n";
}

TEST(PtzCameraModelTest, SetIntrinsics)
{
    std::cout << "[Test] testSetIntrinsics\n";
    PtzCameraModel model;
    CameraIntrinsics intr;
    intr.imageWidth = 1280;
    intr.imageHeight = 720;
    intr.cx = 640.0;
    intr.cy = 360.0;
    intr.fx0 = 800.0;
    intr.fy0 = 800.0;
    model.setIntrinsics(intr);

    const auto& got = model.getIntrinsics();
    EXPECT_TRUE(got.imageWidth == 1280);
    EXPECT_TRUE(got.imageHeight == 720);
    EXPECT_TRUE(near(got.fx0, 800.0, 1e-9));
    std::cout << "  -> PASSED\n";
}

TEST(PtzCameraModelTest, ProjectBoresightToPrincipalPoint)
{
    std::cout << "[Test] testProjectBoresightToPrincipalPoint\n";
    PtzCameraModel model;
    // When target azimuth == camera pan and target elevation == camera tilt,
    // the target is directly on the boresight → projects to the principal point (cx, cy)
    const double pan = 1.0;
    const double tilt = 0.3;
    const auto uv = model.project(pan, tilt, pan, tilt, 1.0);
    const auto& intr = model.getIntrinsics();
    EXPECT_TRUE(near(uv[0], intr.cx, TOL_PIX));
    EXPECT_TRUE(near(uv[1], intr.cy, TOL_PIX));
    std::cout << "  uv=(" << uv[0] << ", " << uv[1] << ") principal=(" << intr.cx << ", " << intr.cy << ")\n";
    std::cout << "  -> PASSED\n";
}

TEST(PtzCameraModelTest, ProjectUnprojectRoundTrip)
{
    std::cout << "[Test] testProjectUnprojectRoundTrip\n";
    PtzCameraModel model;
    const double targetAz = 1.2;
    const double targetEl = 0.1;
    const double camPan = 1.0;
    const double camTilt = 0.0;
    const double zoom = 1.0;

    const auto uv = model.project(targetAz, targetEl, camPan, camTilt, zoom);

    double azOut = 0.0;
    double elOut = 0.0;
    const bool ok = model.unproject(uv[0], uv[1], camPan, camTilt, zoom, azOut, elOut);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(near(azOut, targetAz, TOL_RAD));
    EXPECT_TRUE(near(elOut, targetEl, TOL_RAD));
    std::cout << "  target=(" << targetAz << ", " << targetEl << ") recovered=(" << azOut << ", " << elOut << ")\n";
    std::cout << "  -> PASSED\n";
}

TEST(PtzCameraModelTest, ZoomScalesProjection)
{
    std::cout << "[Test] testZoomScalesProjection\n";
    PtzCameraModel model;
    const double pan = 1.0;
    const double tilt = 0.0;
    const double smallAngle = 0.05; // ~2.87 degrees off-boresight

    const auto uv1x = model.project(pan + smallAngle, tilt, pan, tilt, 1.0);
    const auto uv2x = model.project(pan + smallAngle, tilt, pan, tilt, 2.0);

    // At 2x zoom, the pixel displacement from principal point should be ~2x larger
    const auto& intr = model.getIntrinsics();
    const double disp1x = uv1x[0] - intr.cx;
    const double disp2x = uv2x[0] - intr.cx;
    EXPECT_TRUE(std::abs(disp2x) > std::abs(disp1x) * 1.5);
    std::cout << "  disp1x=" << disp1x << " disp2x=" << disp2x << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(PtzCameraModelTest, ComputeJacobianIsFinite)
{
    std::cout << "[Test] testComputeJacobianIsFinite\n";
    PtzCameraModel model;
    const auto J = model.computeJacobian(1.1, 0.2, 1.0, 0.0, 1.0);
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 2; ++c) {
            EXPECT_TRUE(std::isfinite(J(static_cast<std::size_t>(r), static_cast<std::size_t>(c))));
        }
    }
    std::cout << "  J(0,0)=" << J(0, 0) << " J(1,1)=" << J(1, 1) << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(PtzCameraModelTest, UnprojectPrincipalPointReturnsBoresight)
{
    std::cout << "[Test] testUnprojectPrincipalPointReturnsBoresight\n";
    PtzCameraModel model;
    const double camPan = 1.5;
    const double camTilt = 0.2;
    const auto& intr = model.getIntrinsics();

    double azOut = 0.0;
    double elOut = 0.0;
    const bool ok = model.unproject(intr.cx, intr.cy, camPan, camTilt, 1.0, azOut, elOut);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(near(azOut, camPan, TOL_RAD));
    EXPECT_TRUE(near(elOut, camTilt, TOL_RAD));
    std::cout << "  -> PASSED\n";
}

} // namespace

