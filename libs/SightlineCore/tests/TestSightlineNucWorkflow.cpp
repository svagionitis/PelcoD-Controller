/// @file TestSightlineNucWorkflow.cpp
/// @brief Packet-sequence tests for the NucWorkflow sequencer and its one-shot commands.
/// @details Recipes follow docs/protocols/Sightline/EAN-NUC-and-DPR.pdf:
///          2-point (section 3.4), 1-point (section 4.6 / Appendix A4), shutter flatten
///          (sections 4.7 / 4.7.1) and 3D noise statistics (section 5.5).

#include "SightlineFraming.h"
#include "modules/SightlineNucWorkflow.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {
namespace {

    constexpr FwVersion kFw300 { 3U, 0U };
    constexpr FwVersion kFw302 { 3U, 2U };
    constexpr FwVersion kFw303 { 3U, 3U };
    constexpr FwVersion kFw304 { 3U, 4U };
    constexpr FwVersion kFw309 { 3U, 9U };
    constexpr FwVersion kFw311 { 3U, 11U };

    using Bytes = std::vector<std::uint8_t>;

    /// @brief Returns the payload of a framed packet.
    /// @param[in] packet Framed packet.
    /// @return Payload bytes.
    Bytes payloadOf(const Bytes& packet)
    {
        const ByteView view { SightlineFraming::extractPayload(packet) };
        return Bytes { view.begin(), view.end() };
    }

    /// @brief Builds a Version Number reply for the given firmware.
    /// @param[in] fw Firmware version.
    /// @return Parsed 0x40 message.
    MsgVersionNumber versionOf(FwVersion fw)
    {
        MsgVersionNumber ver {};
        ver.softwareMajor = fw.major;
        ver.softwareMinor = fw.minor;
        return ver;
    }

    /// @brief Board state with non-default tail values that must survive every run command.
    /// @return Parsed 0x35 reply.
    MsgNucParameters boardState()
    {
        MsgNucParameters st {};
        st.cameraIndex = 1U;
        st.nucShow = NucShow::NucAndDpr;
        st.nucRunMode = NucRunMode::CalcDead; // echoed last action: must never be replayed
        st.maxDeadGain = 124U;                // echoed limits: must be zeroed for other runs
        st.maxNumDead = 500;
        st.deadReplace = DeadReplace::Median;
        st.numReplace = 3U;
        st.deadFilter = DeadFilter::MinMax;
        st.deadFilterThresh = 40;
        st.destripeAmount = 7U;
        st.destripeSections = 2U;
        return st;
    }

    /// @brief Test fixture recording every packet the workflow emits.
    class NucWorkflowTest : public ::testing::Test {
    protected:
        std::vector<Bytes> sent {};
        bool sinkOk { true };
        NucWorkflow wf { [this](const Bytes& pkt) {
            sent.push_back(pkt);
            return sinkOk;
        } };

        /// @brief Configures firmware, camera 1, and optionally the cached board state.
        /// @param[in] fw Firmware version.
        /// @param[in] withState True to deliver boardState() as a 0x35 reply.
        void setup(FwVersion fw, bool withState)
        {
            wf.onVersion(versionOf(fw));
            wf.setCamera(1U);
            if (withState) {
                wf.onNucReply(boardState());
            }
        }

        /// @brief Runs next() until the workflow leaves the Running stage.
        void runAll()
        {
            while (wf.stage() == NucStage::Running) {
                ASSERT_EQ(wf.next(), NucError::Ok);
            }
        }
    };

    // =========================================================================
    // 2-point recipe
    // =========================================================================

    /// @brief EAN 3.4: Reset all -> add hot -> add cold -> 2-pt (numFrames 0).
    TEST_F(NucWorkflowTest, TwoPointSequence)
    {
        setup(kFw311, true);
        NucOptions opt {};
        opt.numFrames = 30U;
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, opt), NucError::Ok);
        EXPECT_TRUE(sent.empty()); // nothing is sent until the operator confirms
        EXPECT_EQ(wf.stepCount(), 4U);
        EXPECT_GT(std::string { wf.prompt() }.size(), 0U);

        runAll();
        EXPECT_EQ(wf.stage(), NucStage::Done);
        ASSERT_EQ(sent.size(), 4U);

        const std::uint8_t runs[] { 6U, 1U, 1U, 8U };
        const std::uint8_t frames[] { 0U, 30U, 30U, 0U };
        for (std::size_t i { 0U }; i < 4U; ++i) {
            EXPECT_EQ(SightlineFraming::identifyMessage(sent[i]), MessageId::NucParameters);
            const Bytes p { payloadOf(sent[i]) };
            ASSERT_EQ(p.size(), 36U) << i; // full layout, empty nucName (FW 3.11 + known state)
            EXPECT_EQ(p[0U], 1U) << i;
            EXPECT_EQ(p[2U], runs[i]) << i;
            EXPECT_EQ(p[3U], frames[i]) << i;
        }
        EXPECT_EQ(wf.next(), NucError::NotRunning);
    }

    /// @brief Cached tail values are re-sent unchanged; echoed DPR limits are zeroed.
    TEST_F(NucWorkflowTest, TwoPointPreservesBoardTail)
    {
        setup(kFw311, true);
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        runAll();
        for (const Bytes& pkt : sent) {
            const Bytes p { payloadOf(pkt) };
            ASSERT_EQ(p.size(), 36U);
            for (std::size_t i { 4U }; i < 28U; ++i) {
                EXPECT_EQ(p[i], 0U) << "DPR byte " << i;
            }
            EXPECT_EQ(p[28U], 2U); // deadReplace Median
            EXPECT_EQ(p[29U], 3U); // numReplace
            EXPECT_EQ(p[30U], 1U); // deadFilter MinMax
            EXPECT_EQ(p[31U], 40U);
            EXPECT_EQ(p[33U], 7U); // destripeAmount
            EXPECT_EQ(p[34U], 2U); // destripeSections
        }
    }

    /// @brief Without a 0x35 reply the tail is unknown, so only the 28-byte base is sent.
    TEST_F(NucWorkflowTest, NoStateSendsBaseTail)
    {
        setup(kFw311, false);
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        runAll();
        ASSERT_EQ(sent.size(), 4U);
        for (const Bytes& pkt : sent) {
            EXPECT_EQ(payloadOf(pkt).size(), 28U);
        }
    }

    /// @brief Unknown firmware is treated as 3.0: base tail even with a cached state.
    TEST_F(NucWorkflowTest, UnknownFirmwareSendsBaseTail)
    {
        wf.setCamera(1U);
        wf.onNucReply(boardState());
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        ASSERT_EQ(wf.next(), NucError::Ok);
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(payloadOf(sent[0U]).size(), 28U);
    }

    /// @brief nucShow is kept during the recipe; after 2-pt the board shows NUC+DPR (EAN 3.4 step 7).
    TEST_F(NucWorkflowTest, ShowTracksTwoPointAutoEnable)
    {
        setup(kFw311, true);
        ASSERT_EQ(wf.setShow(NucShow::Uncorrected), NucError::Ok);
        sent.clear();
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        runAll();
        for (const Bytes& pkt : sent) {
            EXPECT_EQ(payloadOf(pkt)[1U], 0U);
        }
        sent.clear();
        ASSERT_EQ(wf.calcDead(DprLimits {}), NucError::Ok);
        ASSERT_FALSE(sent.empty());
        EXPECT_EQ(payloadOf(sent[0U])[1U], 1U); // NucAndDpr after the 2-pt auto-enable
    }

    /// @brief Starting while a recipe is running is rejected.
    TEST_F(NucWorkflowTest, StartWhileRunningIsBusy)
    {
        setup(kFw311, true);
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        EXPECT_EQ(wf.start(NucRecipe::OnePoint, NucOptions {}), NucError::Busy);
    }

    /// @brief Abort stops the recipe without emitting any packet.
    TEST_F(NucWorkflowTest, AbortSendsNothing)
    {
        setup(kFw311, true);
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        ASSERT_EQ(wf.next(), NucError::Ok);
        wf.abort();
        EXPECT_EQ(wf.stage(), NucStage::Aborted);
        EXPECT_EQ(wf.next(), NucError::NotRunning);
        EXPECT_EQ(sent.size(), 1U);
        EXPECT_EQ(wf.start(NucRecipe::OnePoint, NucOptions {}), NucError::Ok);
    }

    /// @brief A transport failure stops the recipe in the Failed stage.
    TEST_F(NucWorkflowTest, SinkFailureFails)
    {
        setup(kFw311, true);
        sinkOk = false;
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, NucOptions {}), NucError::Ok);
        EXPECT_EQ(wf.next(), NucError::NotSent);
        EXPECT_EQ(wf.stage(), NucStage::Failed);
        EXPECT_EQ(wf.lastError(), NucError::NotSent);
    }

    // =========================================================================
    // 1-point, shutter flatten, noise statistics
    // =========================================================================

    /// @brief EAN 4.6 / A4: clear frames -> add N frames -> calculate 1-pt.
    TEST_F(NucWorkflowTest, OnePointSequence)
    {
        setup(kFw300, false);
        NucOptions opt {};
        opt.numFrames = 5U;
        ASSERT_EQ(wf.start(NucRecipe::OnePoint, opt), NucError::Ok);
        runAll();
        ASSERT_EQ(sent.size(), 3U);
        EXPECT_EQ(payloadOf(sent[0U])[2U], 2U);
        EXPECT_EQ(payloadOf(sent[1U])[2U], 1U);
        EXPECT_EQ(payloadOf(sent[1U])[3U], 5U);

        // EAN A4 step 6 golden vector: <cam>,01,07,00 ... (28 bytes)
        Bytes expected { 0x01U, 0x01U, 0x07U, 0x00U };
        expected.resize(28U, 0x00U);
        EXPECT_EQ(payloadOf(sent[2U]), expected);
    }

    /// @brief EAN 4.7.1: shutter flatten then "Save Shtr Fltn" (0x36 op 7) when a name is given.
    TEST_F(NucWorkflowTest, ShutterFlattenWithSave)
    {
        setup(kFw311, true);
        NucOptions opt {};
        opt.numFrames = 10U;
        opt.saveName = "zoom1_shutter_only";
        ASSERT_EQ(wf.start(NucRecipe::ShutterFlatten, opt), NucError::Ok);
        EXPECT_EQ(wf.stepCount(), 2U);
        runAll();
        ASSERT_EQ(sent.size(), 2U);
        EXPECT_EQ(payloadOf(sent[0U])[2U], 12U);
        EXPECT_EQ(SightlineFraming::identifyMessage(sent[1U]), MessageId::ReadWriteNuc);
        const Bytes p { payloadOf(sent[1U]) };
        EXPECT_EQ(p[0U], 1U);
        EXPECT_EQ(p[2U], 0x07U);
        EXPECT_EQ(p[3U], 18U);
    }

    /// @brief Shutter flatten needs 3.3; saving it needs 3.11; bad names are rejected up front.
    TEST_F(NucWorkflowTest, ShutterFlattenGates)
    {
        setup(kFw302, true);
        EXPECT_EQ(wf.start(NucRecipe::ShutterFlatten, NucOptions {}), NucError::Unsupported);

        setup(kFw309, true);
        NucOptions opt {};
        opt.saveName = "zoom1_shutter_only";
        EXPECT_EQ(wf.start(NucRecipe::ShutterFlatten, opt), NucError::Unsupported);
        opt.saveName = "bad name";
        setup(kFw311, true);
        EXPECT_EQ(wf.start(NucRecipe::ShutterFlatten, opt), NucError::NameInvalid);
        EXPECT_EQ(wf.stage(), NucStage::Idle);
    }

    /// @brief EAN 5.5: run 13, then the completion warning triggers the 0xAF query.
    TEST_F(NucWorkflowTest, NoiseStatsAutoQuery)
    {
        setup(kFw304, true);
        ASSERT_EQ(wf.start(NucRecipe::NoiseStats, NucOptions {}), NucError::Ok);
        ASSERT_EQ(wf.next(), NucError::Ok);
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(payloadOf(sent[0U])[2U], 13U);

        EXPECT_FALSE(wf.onWarning("Warning: NUC: Frames added"));
        EXPECT_EQ(sent.size(), 1U);

        EXPECT_TRUE(wf.onWarning("Warning: NUC: Noise stats calculation complete."));
        ASSERT_EQ(sent.size(), 2U);
        EXPECT_EQ(SightlineFraming::identifyMessage(sent[1U]), MessageId::GetParameters);
        EXPECT_EQ(payloadOf(sent[1U]), (Bytes { 0xAFU, 0x01U }));
        EXPECT_EQ(wf.stage(), NucStage::Done);
    }

    /// @brief The completion warning is ignored when no noise recipe is waiting.
    TEST_F(NucWorkflowTest, NoiseWarningIgnoredWhenIdle)
    {
        setup(kFw304, true);
        EXPECT_FALSE(wf.onWarning("Warning: NUC: Noise stats calculation complete."));
        EXPECT_TRUE(sent.empty());
    }

    // =========================================================================
    // One-shot commands
    // =========================================================================

    /// @brief calcDead validates limits, then sends run 9 and queries 0xA1.
    TEST_F(NucWorkflowTest, CalcDeadValidatesAndQueriesStats)
    {
        setup(kFw311, true);
        DprLimits bad {};
        bad.maxGain = 1000U;
        EXPECT_EQ(wf.calcDead(bad), NucError::GainRange);
        EXPECT_TRUE(sent.empty());

        DprLimits lim {};
        lim.maxNumDead = 5000;
        ASSERT_EQ(wf.calcDead(lim), NucError::Ok);
        ASSERT_EQ(sent.size(), 2U);
        const Bytes p { payloadOf(sent[0U]) };
        EXPECT_EQ(p[2U], 9U);
        EXPECT_EQ(p[4U], 76U);  // EAN 3.5 example min gain
        EXPECT_EQ(p[6U], 124U); // EAN 3.5 example max gain
        EXPECT_EQ(payloadOf(sent[1U]), (Bytes { 0xA1U, 0x01U }));
    }

    /// @brief setReplace carries every Dpr-tail field, so it works without a cached state.
    TEST_F(NucWorkflowTest, SetReplaceTail)
    {
        setup(kFw303, false);
        DprReplace rep {};
        rep.method = DeadReplace::Average;
        rep.numReplace = 4U;
        rep.filter = DeadFilter::NearFar;
        rep.threshold = 50;
        ASSERT_EQ(wf.setReplace(rep), NucError::Ok);
        ASSERT_EQ(sent.size(), 1U);
        const Bytes p { payloadOf(sent[0U]) };
        ASSERT_EQ(p.size(), 33U);
        EXPECT_EQ(p[28U], 1U);
        EXPECT_EQ(p[29U], 4U);
        EXPECT_EQ(p[30U], 2U);
        EXPECT_EQ(p[31U], 50U);

        setup(kFw300, false);
        EXPECT_EQ(wf.setReplace(rep), NucError::Unsupported);
    }

    /// @brief setDestripe would also re-send the Dpr fields, so it needs the cached state.
    TEST_F(NucWorkflowTest, SetDestripeNeedsState)
    {
        setup(kFw309, false);
        EXPECT_EQ(wf.setDestripe(9U, 3U), NucError::StateUnknown);
        EXPECT_TRUE(sent.empty());

        wf.onNucReply(boardState());
        ASSERT_EQ(wf.setDestripe(9U, 3U), NucError::Ok);
        const Bytes p { payloadOf(sent.at(0U)) };
        ASSERT_EQ(p.size(), 35U);
        EXPECT_EQ(p[28U], 2U); // cached deadReplace preserved
        EXPECT_EQ(p[33U], 9U);
        EXPECT_EQ(p[34U], 3U);
    }

    /// @brief tableOp validates and stamps the camera index.
    TEST_F(NucWorkflowTest, TableOp)
    {
        setup(kFw311, true);
        MsgReadWriteNuc msg {};
        msg.fileOp = NucFileOp::SaveNuc;
        EXPECT_EQ(wf.tableOp(msg), NucError::FileNameBlank);
        msg.fileName = "zoom1";
        ASSERT_EQ(wf.tableOp(msg), NucError::Ok);
        ASSERT_EQ(sent.size(), 1U);
        const Bytes p { payloadOf(sent[0U]) };
        EXPECT_EQ(p[0U], 1U);
        EXPECT_EQ(p[2U], 0x01U);
    }

    /// @brief Manual DPR is gated on 3.3 and matches the EAN A1 layout.
    TEST_F(NucWorkflowTest, DeadPixelOps)
    {
        setup(kFw300, false);
        EXPECT_EQ(wf.addDeadPixel(1U, 256U, false), NucError::Unsupported);

        setup(kFw303, false);
        ASSERT_EQ(wf.addDeadPixel(1U, 256U, false), NucError::Ok);
        ASSERT_EQ(wf.removeDeadPixel(1U, 256U, true), NucError::Ok);
        ASSERT_EQ(wf.dynamicDead(15U, 0U), NucError::Ok);
        ASSERT_EQ(sent.size(), 3U);
        EXPECT_EQ(payloadOf(sent[0U]), (Bytes { 0x01U, 0x00U, 0x01U, 0x00U, 0x00U, 0x01U, 0x00U, 0x00U }));
        EXPECT_EQ(payloadOf(sent[1U]), (Bytes { 0x01U, 0x01U, 0x01U, 0x00U, 0x00U, 0x01U, 0x01U, 0x00U }));
        EXPECT_EQ(payloadOf(sent[2U])[1U], 2U);
    }

    /// @brief queryState asks for 0x35, 0xA1 and the four 0x36 table names.
    TEST_F(NucWorkflowTest, QueryState)
    {
        setup(kFw311, false);
        ASSERT_TRUE(wf.queryState());
        ASSERT_EQ(sent.size(), 6U);
        EXPECT_EQ(payloadOf(sent[0U]), (Bytes { 0x35U, 0x01U }));
        EXPECT_EQ(payloadOf(sent[1U]), (Bytes { 0xA1U, 0x01U }));
        EXPECT_EQ(payloadOf(sent[2U]), (Bytes { 0x36U, 0x01U, 0x01U }));
        EXPECT_EQ(payloadOf(sent[5U]), (Bytes { 0x36U, 0x04U, 0x01U }));
    }

    // =========================================================================
    // Multi-NUC recipe (IDD 0x35 nucName, FW 3.11) - unverified on hardware
    // =========================================================================

    /// @brief Returns the nucName carried by a full-layout 0x35 payload.
    /// @param[in] p Payload (35 fixed bytes + u8 length + characters).
    /// @return Name, or empty if the payload is too short.
    std::string nameOf(const Bytes& p)
    {
        if (p.size() < 36U) {
            return {};
        }
        const std::size_t len { p[35U] };
        if (p.size() != (36U + len)) {
            return {};
        }
        std::string name {};
        for (std::size_t i { 36U }; i < p.size(); ++i) {
            name.push_back(static_cast<char>(p[i]));
        }
        return name;
    }

    /// @brief Cold frames per name, hot frames per name, then a 2-point calculation per name.
    TEST_F(NucWorkflowTest, MultiNucSequence)
    {
        setup(kFw311, true);
        NucOptions opt {};
        opt.numFrames = 20U;
        opt.names = { "lensA", "lensB" };
        ASSERT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::Ok);
        EXPECT_TRUE(sent.empty());
        ASSERT_EQ(wf.stepCount(), 6U);

        const std::string first { wf.prompt() };
        EXPECT_NE(first.find("COLD"), std::string::npos) << first;
        EXPECT_NE(first.find("lensA"), std::string::npos) << first;

        runAll();
        EXPECT_EQ(wf.stage(), NucStage::Done);
        ASSERT_EQ(sent.size(), 6U);

        const std::uint8_t runs[] { 1U, 1U, 1U, 1U, 8U, 8U };
        const std::uint8_t frames[] { 20U, 20U, 20U, 20U, 0U, 0U };
        const char* const names[] { "lensA", "lensB", "lensA", "lensB", "lensA", "lensB" };
        for (std::size_t i { 0U }; i < 6U; ++i) {
            const Bytes p { payloadOf(sent[i]) };
            EXPECT_EQ(p[0U], 1U) << i;
            EXPECT_EQ(p[2U], runs[i]) << i;
            EXPECT_EQ(p[3U], frames[i]) << i;
            EXPECT_EQ(nameOf(p), names[i]) << i;
        }
    }

    /// @brief The hot-frame prompts name the target and the hot source.
    TEST_F(NucWorkflowTest, MultiNucHotPrompt)
    {
        setup(kFw311, true);
        NucOptions opt {};
        opt.names = { "wide", "narrow" };
        ASSERT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::Ok);
        ASSERT_EQ(wf.next(), NucError::Ok);
        ASSERT_EQ(wf.next(), NucError::Ok);
        const std::string hot { wf.prompt() };
        EXPECT_NE(hot.find("HOT"), std::string::npos) << hot;
        EXPECT_NE(hot.find("wide"), std::string::npos) << hot;
    }

    /// @brief Gated to FW 3.11, where the IDD documents nucName.
    TEST_F(NucWorkflowTest, MultiNucNeedsFw311)
    {
        constexpr FwVersion kFw310 { 3U, 10U };
        setup(kFw310, true);
        NucOptions opt {};
        opt.names = { "a" };
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::Unsupported);
        EXPECT_EQ(wf.stage(), NucStage::Idle);
    }

    /// @brief nucName needs the full 0x35 layout, so the board state must be known.
    TEST_F(NucWorkflowTest, MultiNucNeedsBoardState)
    {
        setup(kFw311, false);
        NucOptions opt {};
        opt.names = { "a" };
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::StateUnknown);
    }

    /// @brief Names must be present, unique, short, and use [A-Za-z0-9_-].
    TEST_F(NucWorkflowTest, MultiNucValidatesNames)
    {
        setup(kFw311, true);
        NucOptions opt {};
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::FileNameBlank);

        opt.names = { "a", "" };
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::FileNameBlank);

        opt.names = { "a", "a" };
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::NameInvalid);

        opt.names = { "lens.nuc" };
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::NameInvalid);

        opt.names = { std::string(64U, 'x') };
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::NameTooLong);

        opt.names.clear();
        for (std::size_t i { 0U }; i <= kMaxMultiNuc; ++i) {
            opt.names.push_back("n" + std::to_string(i));
        }
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::NameCount);

        opt.names.pop_back();
        EXPECT_EQ(wf.start(NucRecipe::MultiNuc, opt), NucError::Ok);
        EXPECT_EQ(wf.stepCount(), 3U * kMaxMultiNuc);
    }

    /// @brief Names are ignored by the single-table recipes.
    TEST_F(NucWorkflowTest, NamesIgnoredByTwoPoint)
    {
        setup(kFw311, true);
        NucOptions opt {};
        opt.names = { "lensA" };
        ASSERT_EQ(wf.start(NucRecipe::TwoPoint, opt), NucError::Ok);
        runAll();
        for (const Bytes& pkt : sent) {
            EXPECT_EQ(payloadOf(pkt).size(), 36U);
        }
    }

} // namespace
} // namespace Sightline

