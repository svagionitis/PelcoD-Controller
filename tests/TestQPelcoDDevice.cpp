/// @file TestQPelcoDDevice.cpp
/// @brief Unit test verifying QPelcoDDevice callback registration and reconnection lifecycle.

#include "MockPelcoDDevice.h"
#include "QPelcoDDevice.h"

#include <QCoreApplication>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

static void testNoDuplicateSignalsOnReconnection()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoDQt::QPelcoDDevice device(mock, 1U);

    int txCount { 0 };
    QObject::connect(&device, &PelcoDQt::QPelcoDDevice::trafficLogged,
        [&](bool isTx, const QByteArray&, const QString&) {
            if (isTx) {
                ++txCount;
            }
        });

    // 1st Connection cycle
    assert(device.connectDevice());
    assert(device.isConnected());

    device.panRight(20);
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(txCount == 1);

    // Disconnect
    device.disconnectDevice();
    assert(!device.isConnected());

    // 2nd Connection cycle (reconnect on same instance)
    assert(device.connectDevice());
    assert(device.isConnected());

    // Reset counter and send a second command
    txCount = 0;
    device.panLeft(20);

    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Must be emitted EXACTLY ONCE, not duplicated
    assert(txCount == 1);

    // 3rd Connection cycle
    device.disconnectDevice();
    assert(device.connectDevice());

    txCount = 0;
    device.stopMotion();

    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(txCount == 1);

    device.disconnectDevice();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "[TestQPelcoDDevice] Running tests..." << std::endl;
    testNoDuplicateSignalsOnReconnection();
    std::cout << "[TestQPelcoDDevice] All tests passed successfully." << std::endl;

    return 0;
}
