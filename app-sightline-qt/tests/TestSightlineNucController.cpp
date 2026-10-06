/// @file TestSightlineNucController.cpp
/// @brief Unit tests for the QML-facing NUC/DPR controller (0x35 / 0x36 / 0xA1 / 0xA8 / 0xAF).
/// @details Packets are captured through the injected sink, so no board or transport is needed.

#include "SightlineNucController.h"

#include <SightlineCore/SightlineFraming.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QVariantMap>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using Packet = std::vector<std::uint8_t>;

/// @class NucControllerTest
/// @brief Fixture owning a controller whose sink records every packet.
class NucControllerTest : public ::testing::Test {
protected:
    /// @brief Ensures a QCoreApplication exists for signal delivery.
    static void SetUpTestSuite()
    {
        if (QCoreApplication::instance() == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestSightlineNucController" };
            static char* argv[] { appName, nullptr };
            static QCoreApplication app { argc, argv };
            static_cast<void>(app);
        }
    }

    /// @brief Creates the controller with a recording sink.
    void SetUp() override
    {
        m_ctl = std::make_unique<SightlineNucController>([this](const Packet& p) {
            m_sent.push_back(p);
            return true;
        });
    }

    /// @brief Feeds a firmware version.
    /// @param[in] major Major version.
    /// @param[in] minor Minor version.
    void setFw(std::uint8_t major, std::uint8_t minor)
    {
        Sightline::MsgVersionNumber ver {};
        ver.softwareMajor = major;
        ver.softwareMinor = minor;
        m_ctl->onVersion(ver);
    }

    /// @brief Returns the message ID of a captured packet.
    /// @param[in] idx Packet index.
    /// @return Message ID.
    [[nodiscard]] Sightline::MessageId idAt(std::size_t idx) const
    {
        return Sightline::SightlineFraming::identifyMessage(m_sent.at(idx));
    }

    /// @brief Returns the payload of a captured packet.
    /// @param[in] idx Packet index.
    /// @return Payload bytes.
    [[nodiscard]] Packet payloadAt(std::size_t idx) const
    {
        return Sightline::SightlineFraming::extractPayload(m_sent.at(idx));
    }

    std::vector<Packet> m_sent {};
    std::unique_ptr<SightlineNucController> m_ctl {};
};

TEST_F(NucControllerTest, TwoPointRecipeSendsSteps)
{
    QSignalSpy spy(m_ctl.get(), &SightlineNucController::stateChanged);
    m_ctl->setCamera(1);
    EXPECT_TRUE(m_ctl->start(0, 30, QString {}).isEmpty());
    EXPECT_TRUE(m_ctl->busy());
    EXPECT_EQ(m_ctl->stepCount(), 4);
    EXPECT_FALSE(m_ctl->prompt().isEmpty());

    const std::uint8_t runs[] { 6U, 1U, 1U, 8U };
    for (const std::uint8_t run : runs) {
        EXPECT_TRUE(m_ctl->next().isEmpty());
        const Packet pl { payloadAt(m_sent.size() - 1U) };
        ASSERT_GE(pl.size(), 4U);
        EXPECT_EQ(pl[0], 1U);
        EXPECT_EQ(pl[2], run);
    }
    EXPECT_EQ(idAt(0), Sightline::MessageId::NucParameters);
    EXPECT_EQ(m_ctl->stage(), static_cast<int>(Sightline::NucStage::Done));
    EXPECT_FALSE(m_ctl->busy());
    EXPECT_GE(spy.count(), 5);
}

TEST_F(NucControllerTest, StabilizationBlocksStart)
{
    Sightline::MsgSetStabilizationParameters stab {};
    stab.cameraIndex = 0U;
    stab.mode = 1U;
    m_ctl->onStabilization(stab);
    EXPECT_TRUE(m_ctl->stabilizationOn());
    EXPECT_FALSE(m_ctl->start(0, 30, QString {}).isEmpty());
    EXPECT_FALSE(m_ctl->busy());
    EXPECT_TRUE(m_sent.empty());

    stab.mode = 0U;
    m_ctl->onStabilization(stab);
    EXPECT_FALSE(m_ctl->stabilizationOn());
    EXPECT_TRUE(m_ctl->start(0, 30, QString {}).isEmpty());
}

TEST_F(NucControllerTest, StabilizationAllCameras)
{
    m_ctl->setCamera(2);
    Sightline::MsgSetStabilizationParameters stab {};
    stab.cameraIndex = 255U;
    stab.mode = 1U;
    m_ctl->onStabilization(stab);
    EXPECT_TRUE(m_ctl->stabilizationOn());

    stab.cameraIndex = 3U;
    stab.mode = 0U;
    m_ctl->onStabilization(stab);
    EXPECT_TRUE(m_ctl->stabilizationOn());
}

TEST_F(NucControllerTest, InvalidRecipeRejected)
{
    EXPECT_FALSE(m_ctl->start(-1, 30, QString {}).isEmpty());
    EXPECT_FALSE(m_ctl->start(4, 30, QString {}).isEmpty());
    EXPECT_FALSE(m_ctl->start(0, 256, QString {}).isEmpty());
    EXPECT_FALSE(m_ctl->busy());
}

TEST_F(NucControllerTest, UnknownFirmwareIsConservative)
{
    EXPECT_FALSE(m_ctl->start(2, 30, QString {}).isEmpty());
    EXPECT_FALSE(m_ctl->addDeadPixel(10, 20).isEmpty());
    EXPECT_TRUE(m_sent.empty());
    EXPECT_FALSE(m_ctl->caps().value(QStringLiteral("dpr")).toBool());
}

TEST_F(NucControllerTest, CapsFollowFirmware)
{
    QSignalSpy spy(m_ctl.get(), &SightlineNucController::capsChanged);
    setFw(3U, 9U);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(m_ctl->firmware(), QStringLiteral("3.9"));
    const QVariantMap caps { m_ctl->caps() };
    EXPECT_TRUE(caps.value(QStringLiteral("dpr")).toBool());
    EXPECT_TRUE(caps.value(QStringLiteral("noise")).toBool());
    EXPECT_TRUE(caps.value(QStringLiteral("destripe")).toBool());
    EXPECT_FALSE(caps.value(QStringLiteral("shutterSave")).toBool());
}

TEST_F(NucControllerTest, DeadStatsMappedForCamera)
{
    QSignalSpy spy(m_ctl.get(), &SightlineNucController::deadStatsChanged);
    Sightline::MsgDeadPixelStats st {};
    st.cameraIndex = 1U;
    st.nDead = 99;
    m_ctl->onDeadStats(st);
    EXPECT_EQ(spy.count(), 0);

    st.cameraIndex = 0U;
    st.nDead = 12;
    st.nGainLo = 3U;
    st.nDevHi = 7U;
    m_ctl->onDeadStats(st);
    EXPECT_EQ(spy.count(), 1);
    const QVariantMap m { m_ctl->deadStats() };
    EXPECT_EQ(m.value(QStringLiteral("nDead")).toInt(), 12);
    EXPECT_EQ(m.value(QStringLiteral("nGainLo")).toInt(), 3);
    EXPECT_EQ(m.value(QStringLiteral("nDevHi")).toInt(), 7);
}

TEST_F(NucControllerTest, NoiseStatsScaled)
{
    Sightline::MsgNoise3D n {};
    n.sigT8 = 512U;
    n.noiseTemporal8 = 128U;
    m_ctl->onNoiseStats(n);
    const QVariantMap m { m_ctl->noiseStats() };
    EXPECT_DOUBLE_EQ(m.value(QStringLiteral("sigT")).toDouble(), 2.0);
    EXPECT_DOUBLE_EQ(m.value(QStringLiteral("noiseTemporal")).toDouble(), 0.5);
}

TEST_F(NucControllerTest, WarningAdvancesNoiseRecipe)
{
    setFw(3U, 4U);
    ASSERT_TRUE(m_ctl->start(3, 30, QString {}).isEmpty());
    ASSERT_TRUE(m_ctl->next().isEmpty());
    const std::size_t before { m_sent.size() };

    Sightline::MsgUserWarningMessage warn {};
    warn.message = "Noise stats calculation complete";
    m_ctl->onWarning(warn);
    ASSERT_EQ(m_sent.size(), before + 1U);
    EXPECT_EQ(idAt(before), Sightline::MessageId::GetParameters);
    EXPECT_EQ(payloadAt(before).at(0), 0xAFU);
    EXPECT_EQ(m_ctl->stage(), static_cast<int>(Sightline::NucStage::Done));
    ASSERT_FALSE(m_ctl->warnings().isEmpty());
    EXPECT_TRUE(m_ctl->warnings().constFirst().contains(QStringLiteral("Noise stats")));
}

TEST_F(NucControllerTest, RefreshLabelsTableReplies)
{
    EXPECT_TRUE(m_ctl->refresh());
    EXPECT_EQ(m_sent.size(), 7U);

    const char* names[] { "fieldA", "deadA", "bootNuc", "bootDead" };
    for (const char* name : names) {
        Sightline::MsgReadWriteNuc rep {};
        rep.fileName = name;
        m_ctl->onNucTable(rep);
    }
    const QVariantMap t { m_ctl->tables() };
    EXPECT_EQ(t.value(QStringLiteral("nuc")).toString(), QStringLiteral("fieldA"));
    EXPECT_EQ(t.value(QStringLiteral("dead")).toString(), QStringLiteral("deadA"));
    EXPECT_EQ(t.value(QStringLiteral("defaultNuc")).toString(), QStringLiteral("bootNuc"));
    EXPECT_EQ(t.value(QStringLiteral("defaultDead")).toString(), QStringLiteral("bootDead"));
}

TEST_F(NucControllerTest, BoardStateFromReply)
{
    EXPECT_FALSE(m_ctl->hasBoardState());
    Sightline::MsgNucParameters rep {};
    rep.nucShow = Sightline::NucShow::NucOnly;
    rep.destripeAmount = 40U;
    m_ctl->onNucParams(rep);
    EXPECT_TRUE(m_ctl->hasBoardState());
    const QVariantMap b { m_ctl->board() };
    EXPECT_EQ(b.value(QStringLiteral("nucShow")).toInt(), 2);
    EXPECT_EQ(b.value(QStringLiteral("destripeAmount")).toInt(), 40);
}

TEST_F(NucControllerTest, CameraChangeClearsState)
{
    Sightline::MsgDeadPixelStats st {};
    st.nDead = 5;
    m_ctl->onDeadStats(st);
    m_ctl->onNucParams(Sightline::MsgNucParameters {});
    ASSERT_TRUE(m_ctl->hasBoardState());

    m_ctl->setCamera(1);
    EXPECT_EQ(m_ctl->camera(), 1);
    EXPECT_FALSE(m_ctl->hasBoardState());
    EXPECT_TRUE(m_ctl->deadStats().isEmpty());
}

TEST_F(NucControllerTest, TableOpValidatesName)
{
    EXPECT_FALSE(m_ctl->tableOp(1, 0, QStringLiteral("bad name!"), QString {}, 0).isEmpty());
    EXPECT_TRUE(m_sent.empty());
    EXPECT_FALSE(m_ctl->tableOp(9, 0, QStringLiteral("ok"), QString {}, 0).isEmpty());

    EXPECT_TRUE(m_ctl->tableOp(1, 0, QStringLiteral("field_01"), QString {}, 0).isEmpty());
    ASSERT_EQ(m_sent.size(), 1U);
    EXPECT_EQ(idAt(0), Sightline::MessageId::ReadWriteNuc);
}

TEST_F(NucControllerTest, CalcDeadUsesLimitsMap)
{
    QVariantMap lim { m_ctl->dprDefaults() };
    EXPECT_EQ(lim.value(QStringLiteral("minGain")).toInt(), 76);
    lim.insert(QStringLiteral("maxNumDead"), 1000);
    EXPECT_TRUE(m_ctl->calcDead(lim).isEmpty());
    ASSERT_GE(m_sent.size(), 2U);
    const Packet pl { payloadAt(0) };
    ASSERT_GE(pl.size(), 28U);
    EXPECT_EQ(pl[2], 9U);
    EXPECT_EQ(pl[4], 76U);

    lim.insert(QStringLiteral("maxGain"), 5000);
    EXPECT_FALSE(m_ctl->calcDead(lim).isEmpty());
}

TEST_F(NucControllerTest, ResetForgetsFirmware)
{
    setFw(3U, 11U);
    m_ctl->setCamera(2);
    m_ctl->reset();
    EXPECT_EQ(m_ctl->camera(), 2);
    EXPECT_TRUE(m_ctl->firmware().isEmpty());
    EXPECT_FALSE(m_ctl->caps().value(QStringLiteral("dpr")).toBool());
}

} // namespace
