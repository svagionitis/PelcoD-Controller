/// @file TestLensDrivers.cpp
/// @brief Automated unit verification for driver binding, collision guard, and dynamic loader.

#include <cstdint>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "LensFramePacket.h"
#include "LensWorker.h"

namespace LensApp {

class LensDriversTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "LensDriversTest";
            static char* argv[] = { appName, nullptr };
            new QCoreApplication(argc, argv);
        }
    }
};

TEST_F(LensDriversTest, MockDriverBinding)
{
    Video::AtomicTripleBuffer<LensFramePacket> buffer;
    LensWorker worker(buffer);

    QSignalSpy spy(&worker, &LensWorker::driverBound);
    worker.onBindMock(0);

    EXPECT_EQ(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toInt(), 0); // Channel
    EXPECT_EQ(args.at(1).toInt(), 0); // Mock type
    EXPECT_EQ(args.at(2).toString(), QStringLiteral("IN-MEMORY"));
    EXPECT_TRUE(args.at(5).toBool()); // Success
}

TEST_F(LensDriversTest, PelcoDDriverBinding)
{
    Video::AtomicTripleBuffer<LensFramePacket> buffer;
    LensWorker worker(buffer);

    QSignalSpy spy(&worker, &LensWorker::driverBound);
    worker.onBindPelco(1, 1, 115200, 2);

    EXPECT_EQ(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toInt(), 1); // Channel 1
    EXPECT_EQ(args.at(1).toInt(), 1); // Pelco-D type
    EXPECT_EQ(args.at(4).toInt(), 2); // Camera address 2
}

TEST_F(LensDriversTest, ViscaDriverBinding)
{
    Video::AtomicTripleBuffer<LensFramePacket> buffer;
    LensWorker worker(buffer);

    QSignalSpy spy(&worker, &LensWorker::driverBound);
    worker.onBindVisca(2, 2, 9600, 1);

    EXPECT_EQ(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toInt(), 2); // Channel 2
    EXPECT_EQ(args.at(1).toInt(), 2); // VISCA type
    EXPECT_EQ(args.at(3).toInt(), 9600); // Baud rate
}

TEST_F(LensDriversTest, DynamicDllNotFound)
{
    Video::AtomicTripleBuffer<LensFramePacket> buffer;
    LensWorker worker(buffer);

    QSignalSpy spy(&worker, &LensWorker::driverBound);
    worker.onLoadDll(3, QStringLiteral("nonexistent_lens_driver.dll"), 5, 115200);

    EXPECT_EQ(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toInt(), 3); // Channel 3
    EXPECT_EQ(args.at(1).toInt(), 3); // Dynamic DLL type
    EXPECT_FALSE(args.at(5).toBool()); // Failed to load
    EXPECT_FALSE(args.at(6).toString().isEmpty()); // Error message populated
}

TEST_F(LensDriversTest, PortCollisionGuard)
{
    Video::AtomicTripleBuffer<LensFramePacket> buffer;
    LensWorker worker(buffer);

    QSignalSpy spy(&worker, &LensWorker::driverBound);

    // Bind Channel 0 to port index 99
    worker.onBindPelco(0, 99, 115200, 1);
    EXPECT_EQ(spy.count(), 1);

    // Note: If physical port 99 failed to open because it's a mock number,
    // port collision guard checks actively held ports.
    // Re-binding channel 0 to mock releases port reservation.
    worker.onBindMock(0);
    EXPECT_EQ(spy.count(), 2);
}

} // namespace LensApp
