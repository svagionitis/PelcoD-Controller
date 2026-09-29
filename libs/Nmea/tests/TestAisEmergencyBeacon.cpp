#include "AisBitReader.h"
#include "AisDecoder.h"
#include "AisTypes.h"
#include "NmeaChecksum.h"
#include "NmeaDevice.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Nmea {
namespace {

    class MockNmeaTransport : public Transport::BaseTransport {
    public:
        MockNmeaTransport() = default;
        ~MockNmeaTransport() override = default;

        bool open() override
        {
            m_isOpen = true;
            return true;
        }

        void close() override
        {
            m_isOpen = false;
        }

        bool isOpen() const noexcept override
        {
            return m_isOpen;
        }

        bool sendData(const std::vector<std::uint8_t>& data) override
        {
            std::lock_guard<std::mutex> lock(m_txMutex);
            m_sentPackets.push_back(std::string(data.begin(), data.end()));
            return true;
        }

        void injectString(std::string_view text)
        {
            const std::vector<std::uint8_t> data(text.begin(), text.end());
            invokeDataCallback(data);
        }

    private:
        std::atomic<bool> m_isOpen { false };
        mutable std::mutex m_txMutex {};
        std::vector<std::string> m_sentPackets {};
    };

    class AisBitWriter {
    public:
        void writeBits(std::uint32_t val, std::size_t numBits)
        {
            for (int i = static_cast<int>(numBits) - 1; i >= 0; --i) {
                m_bits.push_back(static_cast<std::uint8_t>((val >> static_cast<std::uint32_t>(i)) & 1U));
            }
        }

        void writeString(const std::string& str)
        {
            for (const char c : str) {
                std::uint8_t val { 0U };
                if (c >= '@' && c <= '_') {
                    val = static_cast<std::uint8_t>(c - 64);
                } else if (c >= ' ' && c <= '?') {
                    val = static_cast<std::uint8_t>(c);
                }
                writeBits(val, 6U);
            }
        }

        [[nodiscard]] std::pair<std::string, std::size_t> toArmoredPayload() const
        {
            std::vector<std::uint8_t> bytes {};
            std::size_t bitIdx { 0U };
            while (bitIdx < m_bits.size()) {
                std::uint8_t b { 0U };
                for (std::size_t i { 0U }; i < 6U; ++i) {
                    b = static_cast<std::uint8_t>(b << 1U);
                    if (bitIdx < m_bits.size()) {
                        b = static_cast<std::uint8_t>(b | m_bits[bitIdx++]);
                    }
                }
                bytes.push_back(b);
            }
            const std::size_t fillBits = (bytes.size() * 6U - m_bits.size()) % 6U;
            std::string payload {};
            payload.reserve(bytes.size());
            for (std::uint8_t v : bytes) {
                v &= 0x3FU;
                const char c = (v <= 40U) ? static_cast<char>(v + 48U) : static_cast<char>(v + 56U);
                payload.push_back(c);
            }
            return { payload, fillBits };
        }

    private:
        std::vector<std::uint8_t> m_bits {};
    };

    [[nodiscard]] std::string createSafetyBroadcastSentence(std::uint32_t mmsi, const std::string& text)
    {
        AisBitWriter writer {};
        writer.writeBits(14U, 6U); // Type 14
        writer.writeBits(0U, 2U); // Repeat indicator
        writer.writeBits(mmsi, 30U); // Source MMSI
        writer.writeBits(0U, 2U); // Spare
        writer.writeString(text);

        const auto [payload, fillBits] = writer.toArmoredPayload();
        const std::string body = "AIVDM,1,1,,A," + payload + "," + std::to_string(fillBits);
        return NmeaChecksum::frameSentence(body, '!');
    }

    [[nodiscard]] std::string createClassAPositionSentence(std::uint32_t mmsi, AisNavStatus navStatus, double latDeg,
        double lonDeg, double sogKnots = 0.0, double cogDeg = 0.0)
    {
        AisBitWriter writer {};
        writer.writeBits(1U, 6U); // Type 1
        writer.writeBits(0U, 2U); // Repeat indicator
        writer.writeBits(mmsi, 30U); // Source MMSI
        writer.writeBits(static_cast<std::uint32_t>(navStatus), 4U);
        writer.writeBits(0U, 8U); // ROT
        const auto sogRaw = static_cast<std::uint32_t>(sogKnots * 10.0);
        writer.writeBits(sogRaw, 10U);
        writer.writeBits(1U, 1U); // Accuracy high

        const auto lonRaw = static_cast<std::int32_t>(lonDeg * 600000.0);
        const auto latRaw = static_cast<std::int32_t>(latDeg * 600000.0);
        writer.writeBits(static_cast<std::uint32_t>(lonRaw), 28U);
        writer.writeBits(static_cast<std::uint32_t>(latRaw), 27U);

        const auto cogRaw = static_cast<std::uint32_t>(cogDeg * 10.0);
        writer.writeBits(cogRaw, 12U);
        writer.writeBits(511U, 9U); // Heading unavailable
        writer.writeBits(60U, 6U); // Time stamp unavailable
        writer.writeBits(0U, 2U); // Maneuver
        writer.writeBits(0U, 4U); // Spare (3) + RAIM (1)
        writer.writeBits(0U, 19U); // Radio status

        const auto [payload, fillBits] = writer.toArmoredPayload();
        const std::string body = "AIVDM,1,1,,A," + payload + "," + std::to_string(fillBits);
        return NmeaChecksum::frameSentence(body, '!');
    }

    TEST(TestAisEmergencyBeacon, MmsiClassification)
    {
        EXPECT_EQ(classifyAisMmsi(970010123U), AisBeaconType::AisSart);
        EXPECT_TRUE(isAisEmergencyBeacon(970010123U));

        EXPECT_EQ(classifyAisMmsi(972034567U), AisBeaconType::AisMob);
        EXPECT_TRUE(isAisEmergencyBeacon(972034567U));

        EXPECT_EQ(classifyAisMmsi(974129876U), AisBeaconType::EpirbAis);
        EXPECT_TRUE(isAisEmergencyBeacon(974129876U));

        EXPECT_EQ(classifyAisMmsi(111234567U), AisBeaconType::SarAircraft);
        EXPECT_TRUE(isAisEmergencyBeacon(111234567U));

        EXPECT_EQ(classifyAisMmsi(235001234U), AisBeaconType::StandardVessel);
        EXPECT_FALSE(isAisEmergencyBeacon(235001234U));
    }

    TEST(TestAisEmergencyBeacon, DecodeSafetyBroadcastActive)
    {
        AisDecoder decoder {};
        const std::string sentence = createSafetyBroadcastSentence(970010123U, "SART ACTIVE");

        AisVesselTarget target {};
        ASSERT_TRUE(decoder.decodeSentence(sentence, target, true));

        EXPECT_EQ(target.messageType, AisMessageType::SafetyBroadcast14);
        EXPECT_EQ(target.mmsi, 970010123U);
        EXPECT_EQ(target.beaconType, AisBeaconType::AisSart);
        EXPECT_TRUE(target.isEmergencyBeacon);
        EXPECT_FALSE(target.isEmergencyTestMode);
        EXPECT_EQ(target.safetyText, "SART ACTIVE");
    }

    TEST(TestAisEmergencyBeacon, DecodeSafetyBroadcastTestMode)
    {
        AisDecoder decoder {};
        const std::string sentence = createSafetyBroadcastSentence(972034567U, "MOB TEST");

        AisVesselTarget target {};
        ASSERT_TRUE(decoder.decodeSentence(sentence, target, true));

        EXPECT_EQ(target.messageType, AisMessageType::SafetyBroadcast14);
        EXPECT_EQ(target.mmsi, 972034567U);
        EXPECT_EQ(target.beaconType, AisBeaconType::AisMob);
        EXPECT_TRUE(target.isEmergencyBeacon);
        EXPECT_TRUE(target.isEmergencyTestMode);
        EXPECT_EQ(target.safetyText, "MOB TEST");
    }

    TEST(TestAisEmergencyBeacon, DecodeClassAPositionEmergencyMob)
    {
        AisDecoder decoder {};
        const std::string sentence
            = createClassAPositionSentence(972054321U, AisNavStatus::AisSartActive, 37.8044, -122.4678, 1.5, 90.0);

        AisVesselTarget target {};
        ASSERT_TRUE(decoder.decodeSentence(sentence, target, true));

        EXPECT_EQ(target.mmsi, 972054321U);
        EXPECT_EQ(target.beaconType, AisBeaconType::AisMob);
        EXPECT_TRUE(target.isEmergencyBeacon);
        EXPECT_FALSE(target.isEmergencyTestMode);
        EXPECT_TRUE(target.positionValid);
        EXPECT_NEAR(target.coordinates.latitudeDeg, 37.8044, 1e-4);
        EXPECT_NEAR(target.coordinates.longitudeDeg, -122.4678, 1e-4);
        EXPECT_NEAR(target.speedOverGroundKnots, 1.5, 1e-1);
    }

    TEST(TestAisEmergencyBeacon, NmeaDeviceEmergencyCallbackDispatch)
    {
        auto transport = std::make_shared<MockNmeaTransport>();
        auto device = std::make_shared<NmeaDevice>(transport);
        ASSERT_TRUE(device->start());

        std::atomic<bool> alertReceived { false };
        AisEmergencyAlert receivedAlert {};

        const std::size_t subId
            = device->addEmergencyBeaconCallback([&alertReceived, &receivedAlert](const AisEmergencyAlert& alert) {
                  receivedAlert = alert;
                  alertReceived.store(true);
              });

        // 1. Send normal commercial ship sentence - should NOT trigger emergency alert callback
        const std::string normalShipSentence
            = createClassAPositionSentence(235001234U, AisNavStatus::UnderWayUsingEngine, 37.5, -122.2, 10.0, 180.0);
        transport->injectString(normalShipSentence);

        EXPECT_FALSE(alertReceived.load());
        EXPECT_TRUE(device->activeEmergencyBeacons().empty());

        // 2. Send AIS-SART emergency position sentence
        const std::string sartSentence
            = createClassAPositionSentence(970010999U, AisNavStatus::AisSartActive, 36.95, -122.05, 0.5, 45.0);
        transport->injectString(sartSentence);

        EXPECT_TRUE(alertReceived.load());
        EXPECT_EQ(receivedAlert.mmsi, 970010999U);
        EXPECT_EQ(receivedAlert.beaconType, AisBeaconType::AisSart);
        EXPECT_FALSE(receivedAlert.isTestMode);
        EXPECT_TRUE(receivedAlert.positionValid);
        EXPECT_NEAR(receivedAlert.coordinates.latitudeDeg, 36.95, 1e-4);
        EXPECT_NEAR(receivedAlert.coordinates.longitudeDeg, -122.05, 1e-4);

        // Check active emergency beacon registry
        const auto active = device->activeEmergencyBeacons();
        ASSERT_EQ(active.size(), 1U);
        EXPECT_EQ(active[0].mmsi, 970010999U);

        const auto alertLookup = device->emergencyBeacon(970010999U);
        ASSERT_TRUE(alertLookup.has_value());
        EXPECT_EQ(alertLookup->mmsi, 970010999U);

        // 3. Remove callback and verify no further callbacks
        device->removeEmergencyBeaconCallback(subId);
        alertReceived.store(false);

        const std::string epirbSentence
            = createClassAPositionSentence(974001111U, AisNavStatus::AisSartActive, 37.1, -122.1);
        transport->injectString(epirbSentence);

        EXPECT_FALSE(alertReceived.load());
        // But registry should still be updated
        EXPECT_EQ(device->activeEmergencyBeacons().size(), 2U);
    }

} // namespace
} // namespace Nmea
