/// @file TestLensDriverLoader.cpp
/// @brief GoogleTest suite for dynamic lens driver shared library loading and port collision guard.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "ILensDriver.h"
#include "LensDriverLoader.h"
#include "LensTypes.h"

namespace LensControl {

class TestLensDriverLoader : public ::testing::Test {
protected:
    LensDriverLoader m_loader { };
};

TEST_F(TestLensDriverLoader, FileNotFoundOnEmptyOrBadPath)
{
    std::unique_ptr<ILensDriver> driver;
    const LensLoadResult res1 = m_loader.loadDriver(CameraIndex { 0U }, "", 1U, 9600U, driver);
    EXPECT_EQ(res1, LensLoadResult::FileNotFound);
    EXPECT_EQ(driver, nullptr);

    const LensLoadResult res2
        = m_loader.loadDriver(CameraIndex { 0U }, "non_existent_lens_driver_library_xyz123.dll", 1U, 9600U, driver);
    EXPECT_EQ(res2, LensLoadResult::FileNotFound);
    EXPECT_EQ(driver, nullptr);
}

TEST_F(TestLensDriverLoader, RejectInvalidChannelIndex)
{
    std::unique_ptr<ILensDriver> driver;
    const LensLoadResult res = m_loader.loadDriver(CameraIndex { 4U }, "any_driver.dll", 1U, 9600U, driver);
    EXPECT_EQ(res, LensLoadResult::InitFailed);
    EXPECT_EQ(driver, nullptr);
}

TEST_F(TestLensDriverLoader, PortCollisionPreventsDuplicateBinds)
{
    // Initially port 2 is unbound
    EXPECT_FALSE(m_loader.isPortBound(2U));
    EXPECT_EQ(m_loader.boundPort(CameraIndex { 0U }), LensDriverLoader::kInvalidPort);

    // Load non-existent library on port 2: it fails, so port remains unbound
    std::unique_ptr<ILensDriver> driver;
    const LensLoadResult res = m_loader.loadDriver(CameraIndex { 0U }, "dummy_fail.dll", 2U, 9600U, driver);
    EXPECT_EQ(res, LensLoadResult::FileNotFound);
    EXPECT_FALSE(m_loader.isPortBound(2U));
}

TEST_F(TestLensDriverLoader, MissingSymbolRejection)
{
    std::unique_ptr<ILensDriver> driver;
#if defined(_WIN32) || defined(_WIN64)
    // kernel32.dll is always present on Windows but does not export create_lens_object
    const std::string existingLib = "kernel32.dll";
#else
    // libm.so or libc.so is present on Linux
    const std::string existingLib = "libm.so";
#endif
    const LensLoadResult res = m_loader.loadDriver(CameraIndex { 0U }, existingLib, 1U, 9600U, driver);
    EXPECT_EQ(res, LensLoadResult::MissingCreateSymbol);
    EXPECT_EQ(driver, nullptr);
    EXPECT_FALSE(m_loader.isPortBound(1U));
}

TEST_F(TestLensDriverLoader, ResetCleansUpState)
{
    EXPECT_FALSE(m_loader.isPortBound(1U));
    m_loader.unloadDriver(CameraIndex { 0U });
    m_loader.reset();
    EXPECT_FALSE(m_loader.isPortBound(1U));
}

} // namespace LensControl
