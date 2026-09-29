#include "NmeaDevice.h"
#include "replay/NmeaReplayTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Nmea {
namespace {

    TEST(TestNmeaReplayTransport, TimestampExtractionAndParsing)
    {
        NmeaReplayTransport transport {};

        // 1. Tag block timestamps
        const std::string tagBlockLog
            = "\\s:GP001,c:1609459200*71\\$GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42\r\n"
              "\\s:GP001,c:1609459202*73\\$HEHDT,090.0,T*1B\r\n";

        ASSERT_TRUE(transport.loadFromMemory(tagBlockLog));
        EXPECT_EQ(transport.totalLines(), 2U);
        EXPECT_EQ(transport.totalDuration(), std::chrono::seconds(2));

        // 2. Syslog / ISO log prefix timestamps
        const std::string isoLog
            = "[2026-09-29T12:00:00.000Z] $GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42\r\n"
              "[2026-09-29T12:00:00.250Z] $HEHDT,090.0,T*1B\r\n";

        ASSERT_TRUE(transport.loadFromMemory(isoLog));
        EXPECT_EQ(transport.totalLines(), 2U);
        EXPECT_EQ(transport.totalDuration(), std::chrono::milliseconds(250));

        // 3. Fallback synthetic pacing
        const std::string bareLog = "$GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42\r\n"
                                    "$HEHDT,090.0,T*1B\r\n"
                                    "$GPRMC,120000.00,A,3700.000,N,12200.000,W,12.0,090.0,290926,,,A*7C\r\n";

        ASSERT_TRUE(transport.loadFromMemory(bareLog, std::chrono::milliseconds(50)));
        EXPECT_EQ(transport.totalLines(), 3U);
        EXPECT_EQ(transport.totalDuration(), std::chrono::milliseconds(100)); // 0, 50ms, 100ms
    }

    TEST(TestNmeaReplayTransport, NavigationSeekingAndStep)
    {
        NmeaReplayTransport transport {};
        const std::string log = "$GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42\r\n"
                                "$HEHDT,090.0,T*1B\r\n"
                                "$GPRMC,120000.00,A,3700.000,N,12200.000,W,12.0,090.0,290926,,,A*7C\r\n"
                                "$GPVTG,090.0,T,,M,12.0,N,22.2,K,A*28\r\n";

        ASSERT_TRUE(transport.loadFromMemory(log, std::chrono::milliseconds(100)));
        EXPECT_EQ(transport.totalLines(), 4U);
        EXPECT_EQ(transport.state(), ReplayState::Stopped);

        std::vector<std::string> receivedLines {};
        std::mutex rxMutex {};
        transport.setDataCallback([&](const std::vector<std::uint8_t>& data) {
            std::lock_guard<std::mutex> lock(rxMutex);
            receivedLines.emplace_back(data.begin(), data.end());
        });

        // 1. Single stepping
        transport.stepForward();
        EXPECT_EQ(transport.state(), ReplayState::Paused);
        EXPECT_EQ(transport.currentLine(), 1U);
        {
            std::lock_guard<std::mutex> lock(rxMutex);
            ASSERT_EQ(receivedLines.size(), 1U);
            EXPECT_NE(receivedLines[0].find("GPGGA"), std::string::npos);
        }

        transport.stepForward();
        EXPECT_EQ(transport.currentLine(), 2U);
        {
            std::lock_guard<std::mutex> lock(rxMutex);
            ASSERT_EQ(receivedLines.size(), 2U);
            EXPECT_NE(receivedLines[1].find("HEHDT"), std::string::npos);
        }

        // 2. Seeking by line index
        EXPECT_TRUE(transport.seekLine(0U));
        EXPECT_EQ(transport.currentLine(), 0U);

        // 3. Seeking by ratio
        EXPECT_TRUE(transport.seekRatio(1.0));
        EXPECT_EQ(transport.currentLine(), 3U);
        EXPECT_EQ(transport.progress().currentOffset, transport.totalDuration());

        // Stop resets position
        transport.stop();
        EXPECT_EQ(transport.state(), ReplayState::Stopped);
        EXPECT_EQ(transport.currentLine(), 0U);
    }

    TEST(TestNmeaReplayTransport, SpeedMultiplierBurstMode)
    {
        NmeaReplayTransport transport {};
        const std::string log = "$GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42\r\n"
                                "$HEHDT,090.0,T*1B\r\n"
                                "$GPRMC,120000.00,A,3700.000,N,12200.000,W,12.0,090.0,290926,,,A*7C\r\n"
                                "$GPVTG,090.0,T,,M,12.0,N,22.2,K,A*28\r\n";

        ASSERT_TRUE(transport.loadFromMemory(log, std::chrono::milliseconds(500)));
        ASSERT_TRUE(transport.open());

        // Burst mode (speed = 0.0) -> unthrottled maximum throughput
        transport.setSpeedMultiplier(0.0);
        EXPECT_DOUBLE_EQ(transport.speedMultiplier(), 0.0);

        std::atomic<std::size_t> lineCount { 0U };
        transport.setDataCallback([&lineCount](const std::vector<std::uint8_t>&) { lineCount.fetch_add(1U); });

        const auto start = std::chrono::steady_clock::now();
        transport.play();

        // Wait up to 100 ms for completion (at 1.0x it would take 1500 ms)
        for (int i = 0; i < 50; ++i) {
            if (lineCount.load() >= 4U && transport.state() == ReplayState::Finished) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_EQ(lineCount.load(), 4U);
        EXPECT_EQ(transport.state(), ReplayState::Finished);
        EXPECT_LT(elapsed, std::chrono::milliseconds(150));
    }

    TEST(TestNmeaReplayTransport, SentenceFiltering)
    {
        NmeaReplayTransport transport {};
        const std::string log = "$GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*42\r\n"
                                "$HEHDT,090.0,T*1B\r\n"
                                "$GPRMC,120000.00,A,3700.000,N,12200.000,W,12.0,090.0,290926,,,A*7C\r\n";

        ASSERT_TRUE(transport.loadFromMemory(log, std::chrono::milliseconds(10)));
        ASSERT_TRUE(transport.open());
        transport.setSpeedMultiplier(0.0); // burst

        // Only allow GGA sentences
        transport.setSentenceFilter({ "GGA" });

        std::vector<std::string> received {};
        std::mutex mtx {};
        transport.setDataCallback([&](const std::vector<std::uint8_t>& data) {
            std::lock_guard<std::mutex> lock(mtx);
            received.emplace_back(data.begin(), data.end());
        });

        transport.play();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        std::lock_guard<std::mutex> lock(mtx);
        ASSERT_EQ(received.size(), 1U);
        EXPECT_NE(received[0].find("GPGGA"), std::string::npos);
    }

    TEST(TestNmeaReplayTransport, NmeaDeviceIntegration)
    {
        auto replayTransport = std::make_shared<NmeaReplayTransport>();

        // Log containing GPS position, heading, and AIS commercial target
        const std::string log = "$GPGGA,123519,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,*58\r\n"
                                "$HEHDT,123.4,T*2B\r\n"
                                "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n";

        ASSERT_TRUE(replayTransport->loadFromMemory(log, std::chrono::milliseconds(5)));
        replayTransport->setSpeedMultiplier(0.0); // Fast burst mode

        auto nmeaDevice = std::make_shared<NmeaDevice>(replayTransport);
        ASSERT_TRUE(nmeaDevice->start());

        std::atomic<bool> aisReceived { false };
        nmeaDevice->addAisCallback([&aisReceived](const AisVesselTarget& target) {
            if (target.mmsi == 477553000U) {
                aisReceived.store(true);
            }
        });

        replayTransport->play();

        for (int i = 0; i < 50; ++i) {
            if (aisReceived.load() && nmeaDevice->navSnapshot().hasPosition) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        EXPECT_TRUE(aisReceived.load());
        const auto nav = nmeaDevice->navSnapshot();
        EXPECT_TRUE(nav.hasPosition);
        EXPECT_NEAR(nav.position.latitudeDeg, 37.0, 1e-4);
        EXPECT_NEAR(nav.position.longitudeDeg, -122.0, 1e-4);
        EXPECT_TRUE(nav.hasHeading);
        EXPECT_NEAR(nav.trueHeadingDegrees, 123.4, 1e-1);

        const auto aisOpt = nmeaDevice->aisTarget(477553000U);
        ASSERT_TRUE(aisOpt.has_value());
        EXPECT_EQ(aisOpt->mmsi, 477553000U);
    }

} // namespace
} // namespace Nmea
