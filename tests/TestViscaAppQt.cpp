/// @file TestViscaAppQt.cpp
/// @brief Unit tests for ViscaAppQt GUI components, QViscaSonyDevice adapter, and widgets.

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "app-visca-qt/QViscaBusScanner.h"
#include "app-visca-qt/QViscaSonyDevice.h"
#include "app-visca-qt/widgets/ViscaConnectionWidget.h"
#include "app-visca-qt/widgets/ViscaTrafficInspectorWidget.h"
#include <MockSonyCamera.h>

class ViscaAppQtTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestViscaAppQt";
            static char* argv[] = { appName, nullptr };
            new QApplication(argc, argv);
        }
    }
};

TEST_F(ViscaAppQtTest, DeviceConnectionAndModelIdentification)
{
    auto mock = std::make_shared<Visca::Sony::MockSonyCamera>(Visca::Sony::SonyCameraModelType::FCB_EV9520L, 1);
    ViscaApp::QViscaSonyDevice device(mock, 1);

    QSignalSpy connSpy(&device, &ViscaApp::QViscaSonyDevice::connectionStateChanged);
    QSignalSpy modelSpy(&device, &ViscaApp::QViscaSonyDevice::modelDiscovered);
    QSignalSpy telemSpy(&device, &ViscaApp::QViscaSonyDevice::statusUpdated);

    const bool connected = device.connectDevice();
    EXPECT_TRUE(connected);
    EXPECT_TRUE(device.isConnected());

    EXPECT_GE(connSpy.count(), 1);
    EXPECT_TRUE(connSpy.takeFirst().at(0).toBool());

    EXPECT_GE(modelSpy.count(), 1);
    EXPECT_EQ(device.modelType(), Visca::Sony::SonyCameraModelType::FCB_EV9520L);

    const auto caps = device.capabilities();
    EXPECT_TRUE(caps.supportsDistortionCompensation);
    EXPECT_FALSE(caps.supports4K);
    EXPECT_DOUBLE_EQ(caps.minFocalLengthMm, 4.3);
    EXPECT_DOUBLE_EQ(caps.maxFocalLengthMm, 129.0);

    device.pollStatus();
    EXPECT_GE(telemSpy.count(), 1);

    device.disconnectDevice();
    EXPECT_FALSE(device.isConnected());
}

TEST_F(ViscaAppQtTest, CapabilityGatingEv9520LvsEw9500H)
{
    // EV9520L: supports distortion comp, rejects 4K
    {
        auto mockEV = std::make_shared<Visca::Sony::MockSonyCamera>(Visca::Sony::SonyCameraModelType::FCB_EV9520L, 1);
        ViscaApp::QViscaSonyDevice devEV(mockEV, 1);
        ASSERT_TRUE(devEV.connectDevice());

        QSignalSpy failSpy(&devEV, &ViscaApp::QViscaSonyDevice::commandFailed);
        devEV.setDistortionCompensation(true);
        EXPECT_EQ(failSpy.count(), 0);

        // Mode 0x25 is 4K UHD -> must fail on EV9520L
        devEV.setOperatingMode(0x25);
        EXPECT_GE(failSpy.count(), 1);

        devEV.disconnectDevice();
    }

    // EW9500H: rejects distortion comp, accepts 4K
    {
        auto mockEW = std::make_shared<Visca::Sony::MockSonyCamera>(Visca::Sony::SonyCameraModelType::FCB_EW9500H, 1);
        ViscaApp::QViscaSonyDevice devEW(mockEW, 1);
        ASSERT_TRUE(devEW.connectDevice());

        QSignalSpy failSpy(&devEW, &ViscaApp::QViscaSonyDevice::commandFailed);
        devEW.setDistortionCompensation(true);
        EXPECT_GE(failSpy.count(), 1);

        failSpy.clear();
        devEW.setOperatingMode(0x25);
        EXPECT_EQ(failSpy.count(), 0);

        devEW.disconnectDevice();
    }
}

TEST_F(ViscaAppQtTest, TrafficInspectorLoggingAndFiltering)
{
    ViscaApp::ViscaTrafficInspectorWidget inspector;

    const QByteArray txFrame = QByteArray::fromHex("8101040702FF");
    const QByteArray rxFrame = QByteArray::fromHex("9041FF");

    inspector.logFrame(true, txFrame, "Zoom Tele");
    inspector.logFrame(false, rxFrame, "ACK Socket 1");

    // Clear and verify
    inspector.clearLog();
}

TEST_F(ViscaAppQtTest, ConnectionWidgetCreation)
{
    ViscaApp::ViscaConnectionWidget widget;
    widget.setSelectedAddress(3);
    EXPECT_EQ(widget.selectedAddress(), 3);

    widget.setConnectionState(true);
    widget.setModelBadge("FCB-EV9520L");
    widget.setConnectionState(false);
}

TEST_F(ViscaAppQtTest, BusScannerAsynchronousDiscovery)
{
    auto mock = std::make_shared<Visca::Sony::MockSonyCamera>(Visca::Sony::SonyCameraModelType::FCB_EV9520L, 1);
    ViscaApp::QViscaBusScanner scanner(mock);

    QSignalSpy startedSpy(&scanner, &ViscaApp::QViscaBusScanner::scanStarted);
    QSignalSpy discSpy(&scanner, &ViscaApp::QViscaBusScanner::deviceDiscovered);
    QSignalSpy finishedSpy(&scanner, &ViscaApp::QViscaBusScanner::scanFinished);

    const bool started = scanner.startScan();
    EXPECT_TRUE(started);
    EXPECT_GE(startedSpy.count(), 1);

    // Wait up to 3 seconds for asynchronous finish
    EXPECT_TRUE(finishedSpy.wait(3000));
    EXPECT_GE(discSpy.count(), 1);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
