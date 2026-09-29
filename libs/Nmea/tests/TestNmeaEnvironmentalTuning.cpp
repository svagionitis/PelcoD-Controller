#include "Nmea/NmeaChecksum.h"
#include "Nmea/NmeaDevice.h"
#include "Nmea/NmeaSentenceBuilder.h"
#include "Nmea/NmeaSentenceParser.h"
#include "Nmea/environment/ThermalTuningAdvisor.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <vector>

namespace Nmea {
namespace {

    class MockMeteoTransport : public Transport::BaseTransport {
    public:
        MockMeteoTransport() = default;
        bool open() override
        {
            m_open.store(true);
            notifyState(Transport::TransportState::Connected, "Connected");
            return true;
        }
        void close() override
        {
            m_open.store(false);
            notifyState(Transport::TransportState::Disconnected, "Disconnected");
        }
        bool isOpen() const noexcept override
        {
            return m_open.load();
        }
        bool sendData(const std::vector<std::uint8_t>& data) override
        {
            m_sentData.push_back(data);
            return true;
        }
        void injectSentence(std::string_view sentence)
        {
            std::string toSend(sentence);
            if (toSend.empty() || toSend.back() != '\n') {
                toSend += "\r\n";
            }
            invokeDataCallback(std::vector<std::uint8_t>(toSend.begin(), toSend.end()));
        }

        std::vector<std::vector<std::uint8_t>> m_sentData {};

    private:
        std::atomic<bool> m_open { false };
    };

    TEST(TestNmeaEnvironmentalTuning, MtwParsingAndBuilding)
    {
        const std::string mtwStr = NmeaChecksum::frameSentence("WIMTW,18.5,C");
        MtwData mtw {};
        ASSERT_TRUE(NmeaSentenceParser::parseMtw(mtwStr, mtw, true));
        EXPECT_TRUE(mtw.valid);
        EXPECT_DOUBLE_EQ(mtw.waterTemperatureCelsius, 18.5);

        // Roundtrip builder
        const std::string built = NmeaSentenceBuilder::buildMtw(mtw, "WI");
        EXPECT_TRUE(NmeaChecksum::validate(built));

        MtwData roundtrip {};
        ASSERT_TRUE(NmeaSentenceParser::parseMtw(built, roundtrip, true));
        EXPECT_TRUE(roundtrip.valid);
        EXPECT_DOUBLE_EQ(roundtrip.waterTemperatureCelsius, 18.5);
    }

    TEST(TestNmeaEnvironmentalTuning, MmbParsingAndBuilding)
    {
        const std::string mmbStr = NmeaChecksum::frameSentence("WIMMB,29.9200,I,1.0132,B");
        MmbData mmb {};
        ASSERT_TRUE(NmeaSentenceParser::parseMmb(mmbStr, mmb, true));
        EXPECT_TRUE(mmb.valid);
        EXPECT_NEAR(mmb.pressureInHg, 29.92, 1e-3);
        EXPECT_NEAR(mmb.pressureBars, 1.0132, 1e-3);

        // Roundtrip builder
        const std::string built = NmeaSentenceBuilder::buildMmb(mmb, "WI");
        EXPECT_TRUE(NmeaChecksum::validate(built));

        MmbData roundtrip {};
        ASSERT_TRUE(NmeaSentenceParser::parseMmb(built, roundtrip, true));
        EXPECT_TRUE(roundtrip.valid);
        EXPECT_NEAR(roundtrip.pressureBars, 1.0132, 1e-3);
    }

    TEST(TestNmeaEnvironmentalTuning, MdaParsingAndBuilding)
    {
        // $--MDA,pressInHg,I,pressBar,B,airTemp,C,waterTemp,C,relHum,absHum,dewPoint,C,windDirT,T,windDirM,M,windSpdKnots,N,windSpdMps,M
        const std::string mdaStr = NmeaChecksum::frameSentence(
            "WIMDA,29.9200,I,1.0132,B,20.5,C,16.2,C,82.0,,17.3,C,240.0,T,235.0,M,14.5,N,7.5,M");

        MdaData mda {};
        ASSERT_TRUE(NmeaSentenceParser::parseMda(mdaStr, mda, true));
        EXPECT_TRUE(mda.valid);
        ASSERT_TRUE(mda.barometricPressureInHg.has_value());
        EXPECT_NEAR(*mda.barometricPressureInHg, 29.92, 1e-3);
        ASSERT_TRUE(mda.airTemperatureCelsius.has_value());
        EXPECT_DOUBLE_EQ(*mda.airTemperatureCelsius, 20.5);
        ASSERT_TRUE(mda.waterTemperatureCelsius.has_value());
        EXPECT_DOUBLE_EQ(*mda.waterTemperatureCelsius, 16.2);
        ASSERT_TRUE(mda.relativeHumidityPercent.has_value());
        EXPECT_DOUBLE_EQ(*mda.relativeHumidityPercent, 82.0);
        ASSERT_TRUE(mda.dewPointCelsius.has_value());
        EXPECT_DOUBLE_EQ(*mda.dewPointCelsius, 17.3);
        ASSERT_TRUE(mda.windSpeedKnots.has_value());
        EXPECT_DOUBLE_EQ(*mda.windSpeedKnots, 14.5);

        // Roundtrip builder
        const std::string built = NmeaSentenceBuilder::buildMda(mda, "WI");
        EXPECT_TRUE(NmeaChecksum::validate(built));

        MdaData roundtrip {};
        ASSERT_TRUE(NmeaSentenceParser::parseMda(built, roundtrip, true));
        EXPECT_TRUE(roundtrip.valid);
        ASSERT_TRUE(roundtrip.airTemperatureCelsius.has_value());
        EXPECT_DOUBLE_EQ(*roundtrip.airTemperatureCelsius, 20.5);
    }

    TEST(TestNmeaEnvironmentalTuning, ThermalCrossoverDetection)
    {
        ThermalTuningAdvisor advisor {};

        // Air and water have almost identical temperature (Delta T = 0.2 deg C)
        // This causes thermal contrast crossover / washout in IR
        advisor.updateEnvironment(18.0, 18.2, 50.0, 8.0, 5.0);

        const auto snap = advisor.snapshot();
        ASSERT_TRUE(snap.seaAirDeltaTCelsius.has_value());
        EXPECT_NEAR(*snap.seaAirDeltaTCelsius, 0.2, 1e-4);

        const auto adv = advisor.advice();
        EXPECT_GT(adv.washoutRiskScore, 0.85); // High crossover risk
        EXPECT_EQ(adv.agcPreset, ThermalAgcPreset::CrossoverEnhanced);
        EXPECT_GT(adv.ddeStrengthPercent, 70.0);
    }

    TEST(TestNmeaEnvironmentalTuning, MarineFogRiskEstimation)
    {
        ThermalTuningAdvisor advisor {};

        // Dew point spread is tiny (0.5 deg C) with 95% humidity -> High Fog Risk
        advisor.updateEnvironment(14.0, 12.0, 95.0, 11.5, 4.0);

        const auto adv = advisor.advice();
        EXPECT_GT(adv.fogRiskScore, 0.75);
        EXPECT_EQ(adv.agcPreset, ThermalAgcPreset::MarineFogPenetration);
        EXPECT_GT(adv.ddeStrengthPercent, 80.0);
    }

    TEST(TestNmeaEnvironmentalTuning, SeaClutterSuppression)
    {
        ThermalTuningAdvisor advisor {};

        // High winds (22 knots) causing whitecaps and breaking waves
        advisor.updateEnvironment(12.0, 18.0, 55.0, 5.0, 22.0);

        const auto adv = advisor.advice();
        EXPECT_EQ(adv.agcPreset, ThermalAgcPreset::SeaClutterSuppression);
        EXPECT_DOUBLE_EQ(adv.ddeStrengthPercent, 40.0);
    }

    TEST(TestNmeaEnvironmentalTuning, HotWaterPolarityInversion)
    {
        ThermalTuningAdvisor advisor {};

        // Water significantly warmer than air (e.g. geothermal vent or cold winter night)
        // Water = 22 C, Air = 14 C (Delta T = -8 C)
        advisor.updateEnvironment(22.0, 14.0, 60.0, 5.0, 8.0);

        const auto adv = advisor.advice();
        EXPECT_EQ(adv.polarity, ThermalPolarityAdvice::BlackHot);
    }

    TEST(TestNmeaEnvironmentalTuning, NmeaDeviceEnvironmentalDispatch)
    {
        auto transport = std::make_shared<MockMeteoTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> envReceived { false };
        device.addEnvironmentCallback([&envReceived](const NmeaEnvironmentSnapshot& snap) {
            if (snap.hasWaterTemp && snap.hasAirTemp) {
                envReceived.store(true);
            }
        });

        std::atomic<bool> adviceReceived { false };
        device.addThermalAdviceCallback([&adviceReceived](const ThermalTuningAdvice& adv) {
            if (adv.agcPreset != ThermalAgcPreset::DefaultNormal) {
                adviceReceived.store(true);
            }
        });

        // Inject MDA with near-zero Delta-T
        transport->injectSentence(NmeaChecksum::frameSentence(
            "WIMDA,29.9200,I,1.0132,B,18.1,C,18.0,C,60.0,,10.0,C,180.0,T,175.0,M,6.0,N,3.0,M"));

        EXPECT_TRUE(envReceived.load());
        EXPECT_TRUE(adviceReceived.load());

        const auto snap = device.environmentSnapshot();
        EXPECT_TRUE(snap.hasAirTemp);
        EXPECT_TRUE(snap.hasWaterTemp);
        ASSERT_TRUE(snap.seaAirDeltaTCelsius.has_value());
        EXPECT_NEAR(*snap.seaAirDeltaTCelsius, 0.1, 1e-4);

        const auto adv = device.thermalAdvice();
        EXPECT_EQ(adv.agcPreset, ThermalAgcPreset::CrossoverEnhanced);
    }

} // namespace
} // namespace Nmea
