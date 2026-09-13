/// @file TestQPelcoDDevice.cpp
/// @brief Unit test verifying QPelcoDDevice callback registration and reconnection lifecycle.

#include "MockPelcoDDevice.h"
#include "QPelcoDDevice.h"

#include <QCoreApplication>
#include <QThread>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "TestHelpers.h"

using namespace PelcoDTest;

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

static void testAsyncConnectSignalsEmittedOnMainThread()
{
    const auto* mainThread = QThread::currentThread();

    // Scenario 1: Successful connection
    {
        auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        PelcoDQt::QPelcoDDevice device(mock, 1U);

        std::vector<std::pair<bool, const QThread*>> connectingSignals;
        std::vector<std::pair<bool, const QThread*>> connectionSignals;

        // Use Qt::DirectConnection to catch any signal emitted directly on a background thread
        QObject::connect(
            &device, &PelcoDQt::QPelcoDDevice::connectingStateChanged, &device,
            [&](bool connecting) {
                connectingSignals.emplace_back(connecting, QThread::currentThread());
            },
            Qt::DirectConnection);

        QObject::connect(
            &device, &PelcoDQt::QPelcoDDevice::connectionStateChanged, &device,
            [&](bool connected) {
                connectionSignals.emplace_back(connected, QThread::currentThread());
            },
            Qt::DirectConnection);

        device.connectDeviceAsync();

        for (int i = 0; i < 50 && (connectingSignals.size() < 2U || connectionSignals.empty()); ++i) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        assert(device.isConnected());
        assert(connectingSignals.size() == 2U);
        assert(connectingSignals[0].first == true);
        assert(connectingSignals[0].second == mainThread);
        assert(connectingSignals[1].first == false);
        assert(connectingSignals[1].second == mainThread);

        // Exactly one connectionStateChanged emission, on the main thread
        assert(connectionSignals.size() == 1U);
        assert(connectionSignals[0].first == true);
        assert(connectionSignals[0].second == mainThread);

        device.disconnectDevice();
        assert(!device.isConnected());
    }

    // Scenario 2: Failed connection via FailingOpenTransport
    {
        auto failing = std::make_shared<FailingOpenTransport>();
        PelcoDQt::QPelcoDDevice device(failing, 1U);

        std::vector<std::pair<bool, const QThread*>> connectingSignals;
        std::vector<std::pair<bool, const QThread*>> connectionSignals;

        QObject::connect(
            &device, &PelcoDQt::QPelcoDDevice::connectingStateChanged, &device,
            [&](bool connecting) {
                connectingSignals.emplace_back(connecting, QThread::currentThread());
            },
            Qt::DirectConnection);

        QObject::connect(
            &device, &PelcoDQt::QPelcoDDevice::connectionStateChanged, &device,
            [&](bool connected) {
                connectionSignals.emplace_back(connected, QThread::currentThread());
            },
            Qt::DirectConnection);

        device.connectDeviceAsync();

        for (int i = 0; i < 50 && (connectingSignals.size() < 2U || connectionSignals.empty()); ++i) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        assert(!device.isConnected());
        assert(connectingSignals.size() == 2U);
        assert(connectingSignals[0].first == true);
        assert(connectingSignals[0].second == mainThread);
        assert(connectingSignals[1].first == false);
        assert(connectingSignals[1].second == mainThread);

        // Crucial: exactly ONE connectionStateChanged(false) emission, never duplicated, on main thread
        assert(connectionSignals.size() == 1U);
        assert(connectionSignals[0].first == false);
        assert(connectionSignals[0].second == mainThread);
    }
}

static void testInvokeCore()
{
    // Scenario 1: Configured device executes member function and lambda
    {
        auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        PelcoDQt::QPelcoDDevice device(mock, 1U);
        assert(device.connectDevice());

        int txCount { 0 };
        QObject::connect(&device, &PelcoDQt::QPelcoDDevice::trafficLogged,
            [&](bool isTx, const QByteArray&, const QString&) {
                if (isTx) {
                    ++txCount;
                }
            });

        // Test member function invocation via std::invoke
        device.invokeCore(&PelcoD::PelcoDDevice::panRight, static_cast<std::uint8_t>(25));
        for (int i = 0; i < 20 && txCount == 0; ++i) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        assert(txCount == 1);

        // Test lambda execution via std::invoke
        bool lambdaExecuted { false };
        device.invokeCore([&](PelcoD::PelcoDDevice* dev) {
            assert(dev != nullptr);
            lambdaExecuted = true;
        });
        assert(lambdaExecuted);

        device.disconnectDevice();
    }

    // Scenario 2: Unconfigured device safely ignores invocation without null pointer dereference
    {
        PelcoDQt::QPelcoDDevice unconfiguredDevice(nullptr);
        bool calledOnNull { false };
        unconfiguredDevice.invokeCore([&](PelcoD::PelcoDDevice*) {
            calledOnNull = true;
        });
        assert(!calledOnNull);

        // Member function pointer invocation on null device is also safe
        unconfiguredDevice.invokeCore(&PelcoD::PelcoDDevice::stopMotion);
    }
}

int main(int argc, char* argv[])
{
    PelcoDTest::initTestHarness();

    QCoreApplication app(argc, argv);

    std::cout << "[TestQPelcoDDevice] Running tests..." << std::endl;
    testNoDuplicateSignalsOnReconnection();
    testAsyncConnectSignalsEmittedOnMainThread();
    testInvokeCore();
    std::cout << "[TestQPelcoDDevice] All tests passed successfully." << std::endl;

    return 0;
}
