#include "NmeaGateway.h"

#include <chrono>

namespace Nmea::Gateway {

namespace {
    inline std::string extractSentenceMnemonic(std::string_view sentence)
    {
        if (sentence.empty()) {
            return {};
        }
        std::size_t start = (sentence.front() == '$' || sentence.front() == '!') ? 1U : 0U;
        const std::size_t comma = sentence.find(',', start);
        const std::string_view header
            = (comma == std::string_view::npos) ? sentence.substr(start) : sentence.substr(start, comma - start);

        if (header.size() >= 3U) {
            return std::string(header.substr(header.size() - 3U));
        }
        return std::string(header);
    }

    inline void civilFromDays(int z, int& y, unsigned& m, unsigned& d) noexcept
    {
        z += 719468;
        const int era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        y = static_cast<int>(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        d = doy - (153 * mp + 2) / 5 + 1;
        m = mp < 10 ? mp + 3 : mp - 9;
        y += (m <= 2);
    }

    inline std::uint16_t daysFromCivil(int y, unsigned m, unsigned d) noexcept
    {
        y -= (m <= 2);
        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(y - era * 400);
        const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return static_cast<std::uint16_t>(era * 146097 + static_cast<int>(doe) - 719468);
    }
} // namespace

NmeaGateway::NmeaGateway(const GatewayConfig& config)
    : m_config { config }
{
}

void NmeaGateway::setConfig(const GatewayConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

GatewayConfig NmeaGateway::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void NmeaGateway::setPgnDecimation(std::uint32_t pgn, std::chrono::milliseconds minInterval)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pgnIntervals[pgn] = minInterval;
}

void NmeaGateway::setSentenceDecimation(std::string_view sentenceId, std::chrono::milliseconds minInterval)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sentenceIntervals[std::string(sentenceId)] = minInterval;
}

void NmeaGateway::setSentenceOutputCallback(SentenceOutputCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sentenceCallback = std::move(cb);
}

void NmeaGateway::setCanFrameOutputCallback(CanFrameOutputCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_canCallback = std::move(cb);
}

void NmeaGateway::onCanFrame(const N2k::CanFrame& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_stats.n2kFramesReceived;

    if (!m_config.enableN2kTo0183) {
        return;
    }

    const auto header = N2k::N2kHeader::fromCanId(frame.id);

    // Single-frame PGNs
    if (header.pgn == static_cast<std::uint32_t>(N2k::Pgn::PositionRapidUpdate)
        || header.pgn == static_cast<std::uint32_t>(N2k::Pgn::CogSogRapidUpdate)
        || header.pgn == static_cast<std::uint32_t>(N2k::Pgn::VesselHeading)
        || header.pgn == static_cast<std::uint32_t>(N2k::Pgn::Attitude)
        || header.pgn == static_cast<std::uint32_t>(N2k::Pgn::WindData)) {
        N2k::N2kMessage msg {};
        msg.header = header;
        msg.payload.assign(frame.data.begin(), frame.data.begin() + frame.dlc);
        msg.timestamp = std::chrono::steady_clock::now();
        processDecodedN2k(msg);
        return;
    }

    // FastPacket reassembly
    auto assembled = m_assembler.processCanFrame(frame);
    if (assembled.has_value()) {
        processDecodedN2k(*assembled);
    }
}

void NmeaGateway::onN2kMessage(const N2k::N2kMessage& msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_config.enableN2kTo0183) {
        return;
    }
    processDecodedN2k(msg);
}

void NmeaGateway::processDecodedN2k(const N2k::N2kMessage& msg)
{
    ++m_stats.n2kMessagesDecoded;
    const auto now = std::chrono::steady_clock::now();
    const auto pgn = static_cast<N2k::Pgn>(msg.header.pgn);

    switch (pgn) {
    case N2k::Pgn::PositionRapidUpdate: {
        N2k::PositionRapid pos {};
        if (N2k::N2kDecoder::parsePgn129025(msg.payload.data(), msg.payload.size(), pos)) {
            m_lastPos = pos;
            if (pos.isValid && shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                GgaData gga {};
                gga.coordinates.latitudeDeg = pos.latitudeDeg;
                gga.coordinates.longitudeDeg = pos.longitudeDeg;
                gga.fixQuality = NmeaFixQuality::GpsFix;
                gga.numSatellites = 8U;
                gga.hdop = 1.0;
                gga.valid = true;

                const auto sentence = NmeaSentenceBuilder::buildGga(gga, m_config.defaultTalkerId);
                m_sentenceCallback(sentence);
                ++m_stats.sentencesEmitted;

                // Also synthesize RMC if COG/SOG is cached
                if (m_lastCogSog.has_value()) {
                    RmcData rmc {};
                    rmc.coordinates.latitudeDeg = pos.latitudeDeg;
                    rmc.coordinates.longitudeDeg = pos.longitudeDeg;
                    rmc.speedOverGroundKnots = m_lastCogSog->sogKnots;
                    rmc.courseOverGroundDegrees = m_lastCogSog->cogDegrees;
                    rmc.statusActive = true;
                    rmc.valid = true;

                    const auto rmcSentence = NmeaSentenceBuilder::buildRmc(rmc, m_config.defaultTalkerId);
                    m_sentenceCallback(rmcSentence);
                    ++m_stats.sentencesEmitted;
                }
            }
        }
        break;
    }

    case N2k::Pgn::CogSogRapidUpdate: {
        N2k::CogSogRapid cogSog {};
        if (N2k::N2kDecoder::parsePgn129026(msg.payload.data(), msg.payload.size(), cogSog)) {
            m_lastCogSog = cogSog;
            if (m_lastPos.has_value() && m_lastPos->isValid && shouldEmitPgn(msg.header.pgn, now)
                && m_sentenceCallback) {
                RmcData rmc {};
                rmc.coordinates.latitudeDeg = m_lastPos->latitudeDeg;
                rmc.coordinates.longitudeDeg = m_lastPos->longitudeDeg;
                rmc.speedOverGroundKnots = cogSog.sogKnots;
                rmc.courseOverGroundDegrees = cogSog.cogDegrees;
                rmc.statusActive = true;
                rmc.valid = true;

                const auto sentence = NmeaSentenceBuilder::buildRmc(rmc, m_config.defaultTalkerId);
                m_sentenceCallback(sentence);
                ++m_stats.sentencesEmitted;
            }
        }
        break;
    }

    case N2k::Pgn::VesselHeading: {
        N2k::VesselHeading hdg {};
        if (N2k::N2kDecoder::parsePgn127250(msg.payload.data(), msg.payload.size(), hdg)) {
            m_lastHeading = hdg;
            if (hdg.hasHeading && shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                const auto hdtSentence = NmeaSentenceBuilder::buildHdt(hdg.headingDegrees, "HE");
                m_sentenceCallback(hdtSentence);
                ++m_stats.sentencesEmitted;

                if (hdg.hasDeviation || hdg.hasVariation) {
                    HdgData hdgData {};
                    hdgData.magneticHeadingDeg = hdg.headingDegrees;
                    if (hdg.hasDeviation) {
                        hdgData.magneticDeviationDeg = hdg.deviationDegrees;
                        hdgData.hasDeviation = true;
                    }
                    if (hdg.hasVariation) {
                        hdgData.magneticVariationDeg = hdg.variationDegrees;
                        hdgData.hasVariation = true;
                    }
                    hdgData.valid = true;
                    const auto hdgSentence = NmeaSentenceBuilder::buildHdg(hdgData, "HC");
                    m_sentenceCallback(hdgSentence);
                    ++m_stats.sentencesEmitted;
                }
            }
        }
        break;
    }

    case N2k::Pgn::Attitude: {
        N2k::Attitude att {};
        if (N2k::N2kDecoder::parsePgn127257(msg.payload.data(), msg.payload.size(), att)) {
            m_lastAttitude = att;
            if (shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                const double pitch = att.hasPitch ? att.pitchDegrees : 0.0;
                const double roll = att.hasRoll ? att.rollDegrees : 0.0;
                const auto xdrSentence = NmeaSentenceBuilder::buildXdrPitchRoll(pitch, roll, "II");
                m_sentenceCallback(xdrSentence);
                ++m_stats.sentencesEmitted;
            }
        }
        break;
    }

    case N2k::Pgn::WindData: {
        N2k::WindData wind {};
        if (N2k::N2kDecoder::parsePgn130306(msg.payload.data(), msg.payload.size(), wind)) {
            m_lastWind = wind;
            if (shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                MwvData mwv {};
                mwv.windAngleDeg = wind.windAngleDegrees;
                mwv.windSpeed = wind.windSpeedKnots;
                mwv.speedUnits = 'N';
                mwv.reference = (wind.reference == N2k::WindReference::Apparent) ? 'R' : 'T';
                mwv.valid = true;

                const auto mwvSentence = NmeaSentenceBuilder::buildMwv(mwv, "WI");
                m_sentenceCallback(mwvSentence);
                ++m_stats.sentencesEmitted;
            }
        }
        break;
    }

    case N2k::Pgn::Rudder: {
        N2k::RudderData rudder {};
        if (N2k::N2kDecoder::parsePgn127245(msg.payload.data(), msg.payload.size(), rudder)) {
            m_lastRudder = rudder;
            if (shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                RsaData rsa {};
                rsa.starboardRudderAngleDeg = rudder.positionDegrees;
                rsa.starboardValid = rudder.hasPosition;
                rsa.valid = rudder.hasPosition;
                const auto sentence = NmeaSentenceBuilder::buildRsa(rsa, "RA");
                m_sentenceCallback(sentence);
                ++m_stats.sentencesEmitted;
            }
        }
        break;
    }

    case N2k::Pgn::MagneticVariation: {
        N2k::MagneticVariation magVar {};
        if (N2k::N2kDecoder::parsePgn127258(msg.payload.data(), msg.payload.size(), magVar)) {
            m_lastMagVariation = magVar;
            if (magVar.hasVariation && shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                HdgData hdgData {};
                if (m_lastHeading.has_value() && m_lastHeading->hasHeading) {
                    hdgData.magneticHeadingDeg = m_lastHeading->headingDegrees;
                }
                hdgData.magneticVariationDeg = magVar.variationDegrees;
                hdgData.hasVariation = true;
                hdgData.valid = true;
                const auto sentence = NmeaSentenceBuilder::buildHdg(hdgData, "HC");
                m_sentenceCallback(sentence);
                ++m_stats.sentencesEmitted;
            }
        }
        break;
    }

    case N2k::Pgn::SystemTime: {
        N2k::SystemTimeData sysTime {};
        if (N2k::N2kDecoder::parsePgn126992(msg.payload.data(), msg.payload.size(), sysTime)) {
            m_lastSystemTime = sysTime;
            if (shouldEmitPgn(msg.header.pgn, now) && m_sentenceCallback) {
                ZdaData zda {};
                int year = 1970;
                unsigned month = 1;
                unsigned day = 1;
                civilFromDays(static_cast<int>(sysTime.systemDateDays), year, month, day);
                zda.year = static_cast<std::uint16_t>(year);
                zda.month = static_cast<std::uint8_t>(month);
                zda.day = static_cast<std::uint8_t>(day);

                const auto totalSec = static_cast<std::uint32_t>(sysTime.secondsSinceMidnight);
                zda.utcTime.hour = static_cast<std::uint8_t>(totalSec / 3600U);
                zda.utcTime.minute = static_cast<std::uint8_t>((totalSec % 3600U) / 60U);
                zda.utcTime.second = static_cast<std::uint8_t>(totalSec % 60U);
                zda.utcTime.millisecond = static_cast<std::uint16_t>(
                    (sysTime.secondsSinceMidnight - totalSec) * 1000.0);
                zda.valid = true;

                const auto sentence = NmeaSentenceBuilder::buildZda(zda, m_config.defaultTalkerId);
                m_sentenceCallback(sentence);
                ++m_stats.sentencesEmitted;
            }
        }
        break;
    }

    default:
        break;
    }
}

void NmeaGateway::onSentence(std::string_view sentence)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_stats.sentencesReceived;

    if (!m_config.enable0183ToN2k || !m_canCallback) {
        return;
    }

    const std::string mnemonic = extractSentenceMnemonic(sentence);
    if (mnemonic.empty()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    if (mnemonic == "GGA") {
        GgaData gga {};
        if (NmeaSentenceParser::parseGga(sentence, gga, true) && gga.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::PositionRapid pos {};
                pos.latitudeDeg = gga.coordinates.latitudeDeg;
                pos.longitudeDeg = gga.coordinates.longitudeDeg;
                pos.isValid = (gga.fixQuality != NmeaFixQuality::Invalid);

                const auto frame = N2k::N2kEncoder::encodePositionRapid(pos, m_config.defaultCanSource);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "RMC") {
        RmcData rmc {};
        if (NmeaSentenceParser::parseRmc(sentence, rmc, true) && rmc.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::PositionRapid pos {};
                pos.latitudeDeg = rmc.coordinates.latitudeDeg;
                pos.longitudeDeg = rmc.coordinates.longitudeDeg;
                pos.isValid = rmc.statusActive;

                const auto posFrame = N2k::N2kEncoder::encodePositionRapid(pos, m_config.defaultCanSource);
                m_canCallback(posFrame);
                ++m_stats.n2kFramesEmitted;

                N2k::CogSogRapid cogSog {};
                cogSog.cogDegrees = rmc.courseOverGroundDegrees;
                cogSog.hasCog = true;
                cogSog.sogKnots = rmc.speedOverGroundKnots;
                cogSog.hasSog = true;

                const auto cogFrame = N2k::N2kEncoder::encodeCogSogRapid(cogSog, m_config.defaultCanSource);
                m_canCallback(cogFrame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "HDT") {
        HdtData hdt {};
        if (NmeaSentenceParser::parseHdt(sentence, hdt, true) && hdt.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::VesselHeading hdg {};
                hdg.headingDegrees = hdt.headingDegrees;
                hdg.hasHeading = true;
                hdg.reference = N2k::HeadingReference::True;

                const auto frame = N2k::N2kEncoder::encodeVesselHeading(hdg, m_config.defaultCanSource, 2U);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "THS") {
        ThsData ths {};
        if (NmeaSentenceParser::parseThs(sentence, ths, true) && ths.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::VesselHeading hdg {};
                hdg.headingDegrees = ths.headingDegrees;
                hdg.hasHeading = true;
                hdg.reference = N2k::HeadingReference::True;

                const auto frame = N2k::N2kEncoder::encodeVesselHeading(hdg, m_config.defaultCanSource, 2U);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "HDG") {
        HdgData hdgData {};
        if (NmeaSentenceParser::parseHdg(sentence, hdgData, true) && hdgData.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::VesselHeading hdg {};
                hdg.headingDegrees = hdgData.magneticHeadingDeg;
                hdg.hasHeading = true;
                if (hdgData.hasDeviation) {
                    hdg.deviationDegrees = hdgData.magneticDeviationDeg;
                    hdg.hasDeviation = true;
                }
                if (hdgData.hasVariation) {
                    hdg.variationDegrees = hdgData.magneticVariationDeg;
                    hdg.hasVariation = true;
                }
                hdg.reference = N2k::HeadingReference::Magnetic;

                const auto frame = N2k::N2kEncoder::encodeVesselHeading(hdg, m_config.defaultCanSource, 2U);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "XDR") {
        XdrData xdr {};
        if (NmeaSentenceParser::parseXdr(sentence, xdr, true) && xdr.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::Attitude att {};
                for (const auto& tr : xdr.transducers) {
                    if (tr.id == "PITCH" || tr.id == "PTCH") {
                        att.pitchDegrees = tr.measurement;
                        att.hasPitch = true;
                    } else if (tr.id == "ROLL") {
                        att.rollDegrees = tr.measurement;
                        att.hasRoll = true;
                    }
                }
                if (att.hasPitch || att.hasRoll) {
                    const auto frame = N2k::N2kEncoder::encodeAttitude(att, m_config.defaultCanSource);
                    m_canCallback(frame);
                    ++m_stats.n2kFramesEmitted;
                }
            }
        }
    } else if (mnemonic == "MWV") {
        MwvData mwv {};
        if (NmeaSentenceParser::parseMwv(sentence, mwv, true) && mwv.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::WindData wind {};
                wind.windAngleDegrees = mwv.windAngleDeg;
                wind.hasWindAngle = true;

                // Speed conversion to knots and m/s
                if (mwv.speedUnits == 'K') {
                    wind.windSpeedKnots = mwv.windSpeed * 0.539957;
                } else if (mwv.speedUnits == 'M') {
                    wind.windSpeedKnots = mwv.windSpeed * 1.94384;
                } else {
                    wind.windSpeedKnots = mwv.windSpeed;
                }
                wind.windSpeedMps = wind.windSpeedKnots * 0.514444;
                wind.hasWindSpeed = true;

                wind.reference
                    = (mwv.reference == 'R') ? N2k::WindReference::Apparent : N2k::WindReference::TheoreticalBoat;

                const auto frame = N2k::N2kEncoder::encodeWindData(wind, m_config.defaultCanSource);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "RSA") {
        RsaData rsa {};
        if (NmeaSentenceParser::parseRsa(sentence, rsa, true) && rsa.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::RudderData rudder {};
                if (rsa.starboardValid) {
                    rudder.positionDegrees = rsa.starboardRudderAngleDeg;
                    rudder.hasPosition = true;
                } else if (rsa.portValid) {
                    rudder.positionDegrees = rsa.portRudderAngleDeg;
                    rudder.hasPosition = true;
                }
                const auto frame = N2k::N2kEncoder::encodeRudder(rudder, m_config.defaultCanSource);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    } else if (mnemonic == "ZDA") {
        ZdaData zda {};
        if (NmeaSentenceParser::parseZda(sentence, zda, true) && zda.valid) {
            ++m_stats.sentencesParsed;
            if (shouldEmitSentence(mnemonic, now)) {
                N2k::SystemTimeData st {};
                st.systemDateDays = daysFromCivil(
                    static_cast<int>(zda.year), zda.month, zda.day);
                st.secondsSinceMidnight = static_cast<double>(zda.utcTime.hour) * 3600.0
                    + static_cast<double>(zda.utcTime.minute) * 60.0
                    + static_cast<double>(zda.utcTime.second)
                    + static_cast<double>(zda.utcTime.millisecond) / 1000.0;
                st.hasDate = true;
                st.hasTime = true;
                const auto frame = N2k::N2kEncoder::encodeSystemTime(st, m_config.defaultCanSource);
                m_canCallback(frame);
                ++m_stats.n2kFramesEmitted;
            }
        }
    }
}

bool NmeaGateway::shouldEmitPgn(std::uint32_t pgn, std::chrono::steady_clock::time_point now)
{
    const auto itLimit = m_pgnIntervals.find(pgn);
    const auto interval = (itLimit != m_pgnIntervals.end()) ? itLimit->second : m_config.defaultDecimationInterval;

    const auto itLast = m_pgnLastEmitted.find(pgn);
    if (itLast != m_pgnLastEmitted.end() && interval > std::chrono::milliseconds::zero()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - itLast->second) < interval) {
            ++m_stats.throttledDrops;
            return false;
        }
    }
    m_pgnLastEmitted[pgn] = now;
    return true;
}

bool NmeaGateway::shouldEmitSentence(std::string_view id, std::chrono::steady_clock::time_point now)
{
    const std::string key(id);
    const auto itLimit = m_sentenceIntervals.find(key);
    const auto interval = (itLimit != m_sentenceIntervals.end()) ? itLimit->second : m_config.defaultDecimationInterval;

    const auto itLast = m_sentenceLastEmitted.find(key);
    if (itLast != m_sentenceLastEmitted.end() && interval > std::chrono::milliseconds::zero()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - itLast->second) < interval) {
            ++m_stats.throttledDrops;
            return false;
        }
    }
    m_sentenceLastEmitted[key] = now;
    return true;
}

std::optional<N2k::PositionRapid> NmeaGateway::lastPosition() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastPos;
}

std::optional<N2k::CogSogRapid> NmeaGateway::lastCogSog() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastCogSog;
}

std::optional<N2k::VesselHeading> NmeaGateway::lastHeading() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastHeading;
}

std::optional<N2k::Attitude> NmeaGateway::lastAttitude() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastAttitude;
}

std::optional<N2k::WindData> NmeaGateway::lastWind() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastWind;
}

std::optional<N2k::RudderData> NmeaGateway::lastRudder() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastRudder;
}

std::optional<N2k::MagneticVariation> NmeaGateway::lastMagVariation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastMagVariation;
}

std::optional<N2k::SystemTimeData> NmeaGateway::lastSystemTime() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastSystemTime;
}

GatewayStats NmeaGateway::stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void NmeaGateway::resetStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = GatewayStats {};
}

} // namespace Nmea::Gateway
