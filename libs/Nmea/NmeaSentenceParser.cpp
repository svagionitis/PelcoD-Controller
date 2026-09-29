/// @file NmeaSentenceParser.cpp
/// @brief Implementation of NMEA 0183 tokenizer and sentence parser.

#include "NmeaSentenceParser.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

namespace Nmea {

namespace {

    bool parseDouble(std::string_view sv, double& outVal) noexcept
    {
        if (sv.empty()) {
            return false;
        }
        // std::from_chars for double is available in modern GCC/Clang, or fallback to strtod with null-terminated copy
        char buf[64] {};
        if (sv.size() >= sizeof(buf)) {
            return false;
        }
        for (std::size_t i { 0U }; i < sv.size(); ++i) {
            buf[i] = sv[i];
        }
        buf[sv.size()] = '\0';

        char* endPtr { nullptr };
        const double val { std::strtod(buf, &endPtr) };
        if (endPtr == buf) {
            return false;
        }
        outVal = val;
        return true;
    }

    bool parseUInt(std::string_view sv, unsigned int& outVal) noexcept
    {
        if (sv.empty()) {
            return false;
        }
        unsigned int val { 0U };
        const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        if (res.ec != std::errc {}) {
            return false;
        }
        outVal = val;
        return true;
    }

} // namespace

void NmeaSentenceParser::tokenize(std::string_view sentence, std::vector<std::string_view>& outTokens)
{
    outTokens.clear();

    // Strip optional leading tag block
    if (!sentence.empty() && sentence.front() == '\\') {
        const auto secondBackslash = sentence.find('\\', 1U);
        if (secondBackslash != std::string_view::npos) {
            sentence = sentence.substr(secondBackslash + 1U);
            while (!sentence.empty() && (sentence.front() == ' ' || sentence.front() == '\t')) {
                sentence.remove_prefix(1U);
            }
        }
    }

    // Strip leading $/!
    if (!sentence.empty() && (sentence.front() == '$' || sentence.front() == '!')) {
        sentence.remove_prefix(1U);
    }

    // Strip trailing checksum *HH and whitespace
    const auto starPos = sentence.find('*');
    if (starPos != std::string_view::npos) {
        sentence = sentence.substr(0U, starPos);
    }
    while (!sentence.empty() && (sentence.back() == '\r' || sentence.back() == '\n' || sentence.back() == ' ')) {
        sentence.remove_suffix(1U);
    }

    std::size_t start { 0U };
    while (start <= sentence.size()) {
        const auto commaPos = sentence.find(',', start);
        if (commaPos == std::string_view::npos) {
            outTokens.push_back(sentence.substr(start));
            break;
        }
        outTokens.push_back(sentence.substr(start, commaPos - start));
        start = commaPos + 1U;
    }
}

NmeaSentenceId NmeaSentenceParser::identifySentence(std::string_view sentence) noexcept
{
    if (sentence.empty()) {
        return NmeaSentenceId::Unknown;
    }

    // Strip optional leading tag block
    if (sentence.front() == '\\') {
        const auto secondBackslash = sentence.find('\\', 1U);
        if (secondBackslash != std::string_view::npos) {
            sentence = sentence.substr(secondBackslash + 1U);
            while (!sentence.empty() && (sentence.front() == ' ' || sentence.front() == '\t')) {
                sentence.remove_prefix(1U);
            }
        }
    }

    if (sentence.front() == '$' || sentence.front() == '!') {
        sentence.remove_prefix(1U);
    }

    const auto commaPos = sentence.find(',');
    const auto header = (commaPos != std::string_view::npos) ? sentence.substr(0U, commaPos) : sentence;

    // Check proprietary PFEC
    if (header.size() >= 4U && header.substr(0U, 4U) == "PFEC") {
        return NmeaSentenceId::PFEC;
    }

    // Check Ashtech/Applanix PASHR
    if (header == "PASHR" || (header.size() >= 5U && header.substr(header.size() - 5U) == "PASHR")) {
        return NmeaSentenceId::PASHR;
    }

    // Standard talker + 3-char mnemonic (e.g. GPGGA, GPRMC, HEHDT, RATTM)
    if (header.size() >= 5U) {
        const auto mnemonic = header.substr(header.size() - 3U);
        if (mnemonic == "GGA") {
            return NmeaSentenceId::GGA;
        }
        if (mnemonic == "RMC") {
            return NmeaSentenceId::RMC;
        }
        if (mnemonic == "HDT") {
            return NmeaSentenceId::HDT;
        }
        if (mnemonic == "THS") {
            return NmeaSentenceId::THS;
        }
        if (mnemonic == "TTM") {
            return NmeaSentenceId::TTM;
        }
        if (mnemonic == "TLL") {
            return NmeaSentenceId::TLL;
        }
        if (mnemonic == "XDR") {
            return NmeaSentenceId::XDR;
        }
        if (mnemonic == "VDM") {
            return NmeaSentenceId::VDM;
        }
        if (mnemonic == "VDO") {
            return NmeaSentenceId::VDO;
        }
        if (mnemonic == "RSD") {
            return NmeaSentenceId::RSD;
        }
        if (mnemonic == "OSD") {
            return NmeaSentenceId::OSD;
        }
        if (mnemonic == "APB") {
            return NmeaSentenceId::APB;
        }
        if (mnemonic == "BWC") {
            return NmeaSentenceId::BWC;
        }
        if (mnemonic == "BWR") {
            return NmeaSentenceId::BWR;
        }
        if (mnemonic == "MWV") {
            return NmeaSentenceId::MWV;
        }
        if (mnemonic == "HDG") {
            return NmeaSentenceId::HDG;
        }
        if (mnemonic == "RMB") {
            return NmeaSentenceId::RMB;
        }
        if (mnemonic == "RTE") {
            return NmeaSentenceId::RTE;
        }
        if (mnemonic == "WPL") {
            return NmeaSentenceId::WPL;
        }
        if (mnemonic == "MTW") {
            return NmeaSentenceId::MTW;
        }
        if (mnemonic == "MDA") {
            return NmeaSentenceId::MDA;
        }
        if (mnemonic == "MMB") {
            return NmeaSentenceId::MMB;
        }
        if (mnemonic == "GSA") {
            return NmeaSentenceId::GSA;
        }
        if (mnemonic == "GSV") {
            return NmeaSentenceId::GSV;
        }
        if (mnemonic == "ZDA") {
            return NmeaSentenceId::ZDA;
        }
        if (mnemonic == "VBW") {
            return NmeaSentenceId::VBW;
        }
        if (mnemonic == "VHW") {
            return NmeaSentenceId::VHW;
        }
        if (mnemonic == "DPT") {
            return NmeaSentenceId::DPT;
        }
        if (mnemonic == "DBT") {
            return NmeaSentenceId::DBT;
        }
        if (mnemonic == "ALF") {
            return NmeaSentenceId::ALF;
        }
        if (mnemonic == "ALC") {
            return NmeaSentenceId::ALC;
        }
        if (mnemonic == "ARC") {
            return NmeaSentenceId::ARC;
        }
        if (mnemonic == "HBT") {
            return NmeaSentenceId::HBT;
        }
        if (mnemonic == "ALR") {
            return NmeaSentenceId::ALR;
        }
        if (mnemonic == "ACK") {
            return NmeaSentenceId::ACK;
        }
    }

    return NmeaSentenceId::Unknown;
}

std::string_view NmeaSentenceParser::extractTalkerId(std::string_view sentence) noexcept
{
    if (sentence.empty()) {
        return {};
    }

    // Strip optional leading tag block
    if (sentence.front() == '\\') {
        const auto secondBackslash = sentence.find('\\', 1U);
        if (secondBackslash != std::string_view::npos) {
            sentence = sentence.substr(secondBackslash + 1U);
            while (!sentence.empty() && (sentence.front() == ' ' || sentence.front() == '\t')) {
                sentence.remove_prefix(1U);
            }
        }
    }

    if (sentence.front() == '$' || sentence.front() == '!') {
        sentence.remove_prefix(1U);
    }
    const auto commaPos = sentence.find(',');
    const auto header = (commaPos != std::string_view::npos) ? sentence.substr(0U, commaPos) : sentence;
    if (header.size() == 5U) {
        return header.substr(0U, 2U);
    }
    return {};
}

bool NmeaSentenceParser::parseCoordinate(
    std::string_view coordStr, std::string_view hemiStr, double& outDegrees) noexcept
{
    if (coordStr.empty() || hemiStr.empty()) {
        return false;
    }

    const auto dotPos = coordStr.find('.');
    if (dotPos == std::string_view::npos || dotPos < 2U) {
        return false;
    }

    // Minutes are the 2 digits before the dot plus everything after
    const auto minStartPos = dotPos - 2U;
    const auto degStr = coordStr.substr(0U, minStartPos);
    const auto minStr = coordStr.substr(minStartPos);

    unsigned int degreesInt { 0U };
    if (!parseUInt(degStr, degreesInt)) {
        return false;
    }

    double minutes { 0.0 };
    if (!parseDouble(minStr, minutes)) {
        return false;
    }

    double deg = static_cast<double>(degreesInt) + (minutes / 60.0);
    const char hemi = hemiStr.front();
    if (hemi == 'S' || hemi == 's' || hemi == 'W' || hemi == 'w') {
        deg = -deg;
    } else if (hemi != 'N' && hemi != 'n' && hemi != 'E' && hemi != 'e') {
        return false;
    }

    outDegrees = deg;
    return true;
}

bool NmeaSentenceParser::parseUtcTime(std::string_view timeStr, NmeaUtcTime& outTime) noexcept
{
    if (timeStr.size() < 6U) {
        return false;
    }

    unsigned int hh { 0U };
    unsigned int mm { 0U };
    unsigned int ss { 0U };

    if (!parseUInt(timeStr.substr(0U, 2U), hh) || hh > 23U) {
        return false;
    }
    if (!parseUInt(timeStr.substr(2U, 2U), mm) || mm > 59U) {
        return false;
    }
    if (!parseUInt(timeStr.substr(4U, 2U), ss) || ss > 60U) { // 60 for leap second
        return false;
    }

    std::uint16_t ms { 0U };
    if (timeStr.size() > 6U && timeStr[6U] == '.') {
        double frac { 0.0 };
        if (parseDouble(timeStr.substr(6U), frac)) {
            ms = static_cast<std::uint16_t>(std::clamp(frac * 1000.0, 0.0, 999.0));
        }
    }

    outTime.hour = static_cast<std::uint8_t>(hh);
    outTime.minute = static_cast<std::uint8_t>(mm);
    outTime.second = static_cast<std::uint8_t>(ss);
    outTime.millisecond = ms;
    return true;
}

bool NmeaSentenceParser::parseDate(std::string_view dateStr, NmeaDate& outDate) noexcept
{
    if (dateStr.size() < 6U) {
        return false;
    }

    unsigned int dd { 0U };
    unsigned int mm { 0U };
    unsigned int yy { 0U };

    if (!parseUInt(dateStr.substr(0U, 2U), dd) || dd < 1U || dd > 31U) {
        return false;
    }
    if (!parseUInt(dateStr.substr(2U, 2U), mm) || mm < 1U || mm > 12U) {
        return false;
    }
    if (!parseUInt(dateStr.substr(4U, 2U), yy)) {
        return false;
    }

    // 2-digit to 4-digit year mapping per NMEA standard
    const std::uint16_t fullYear = static_cast<std::uint16_t>((yy < 70U) ? (2000U + yy) : (1900U + yy));

    outDate.day = static_cast<std::uint8_t>(dd);
    outDate.month = static_cast<std::uint8_t>(mm);
    outDate.year = fullYear;
    return true;
}

bool NmeaSentenceParser::parseGga(std::string_view sentence, GgaData& outData, bool verifyChecksum) noexcept
{
    outData = GgaData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--GGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx
    if (tokens.size() < 15U) {
        return false;
    }

    static_cast<void>(parseUtcTime(tokens[1], outData.utcTime));

    if (!parseCoordinate(tokens[2], tokens[3], outData.coordinates.latitudeDeg)
        || !parseCoordinate(tokens[4], tokens[5], outData.coordinates.longitudeDeg)) {
        return false;
    }

    unsigned int fixQ { 0U };
    if (parseUInt(tokens[6], fixQ)) {
        outData.fixQuality = static_cast<NmeaFixQuality>(std::clamp(fixQ, 0U, 8U));
    }

    unsigned int sats { 0U };
    if (parseUInt(tokens[7], sats)) {
        outData.numSatellites = static_cast<std::uint8_t>(sats);
    }

    parseDouble(tokens[8], outData.hdop);
    parseDouble(tokens[9], outData.altitudeMeters);
    parseDouble(tokens[11], outData.geoidalSeparationMeters);
    parseDouble(tokens[13], outData.dgpsAgeSeconds);

    unsigned int stnId { 0U };
    if (parseUInt(tokens[14], stnId)) {
        outData.dgpsStationId = static_cast<std::uint16_t>(stnId);
    }

    outData.valid = (outData.fixQuality != NmeaFixQuality::Invalid);
    return true;
}

bool NmeaSentenceParser::parseRmc(std::string_view sentence, RmcData& outData, bool verifyChecksum) noexcept
{
    outData = RmcData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--RMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a,m
    if (tokens.size() < 10U) {
        return false;
    }

    static_cast<void>(parseUtcTime(tokens[1], outData.utcTime));

    outData.statusActive = (!tokens[2].empty() && tokens[2].front() == 'A');

    if (!parseCoordinate(tokens[3], tokens[4], outData.coordinates.latitudeDeg)
        || !parseCoordinate(tokens[5], tokens[6], outData.coordinates.longitudeDeg)) {
        return false;
    }

    parseDouble(tokens[7], outData.speedOverGroundKnots);
    parseDouble(tokens[8], outData.courseOverGroundDegrees);
    static_cast<void>(parseDate(tokens[9], outData.date));

    if (tokens.size() > 11U) {
        double magVar { 0.0 };
        if (parseDouble(tokens[10], magVar)) {
            if (!tokens[11].empty() && (tokens[11].front() == 'W' || tokens[11].front() == 'w')) {
                magVar = -magVar;
            }
            outData.magneticVariationDegrees = magVar;
        }
    }

    if (tokens.size() > 12U && !tokens[12].empty()) {
        outData.faaMode = static_cast<NmeaFaaMode>(tokens[12].front());
    }

    outData.valid = outData.statusActive;
    return true;
}

bool NmeaSentenceParser::parseHdt(std::string_view sentence, HdtData& outData, bool verifyChecksum) noexcept
{
    outData = HdtData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--HDT,x.x,T
    if (tokens.size() < 3U) {
        return false;
    }

    if (!parseDouble(tokens[1], outData.headingDegrees)) {
        return false;
    }

    outData.valid = (tokens[2] == "T" && outData.headingDegrees >= 0.0 && outData.headingDegrees < 360.0);
    return outData.valid;
}

bool NmeaSentenceParser::parseThs(std::string_view sentence, ThsData& outData, bool verifyChecksum) noexcept
{
    outData = ThsData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--THS,x.x,a
    if (tokens.size() < 3U) {
        return false;
    }

    if (!parseDouble(tokens[1], outData.headingDegrees)) {
        return false;
    }

    if (!tokens[2].empty()) {
        outData.mode = static_cast<NmeaFaaMode>(tokens[2].front());
    }

    outData.valid = (outData.mode == NmeaFaaMode::Autonomous || outData.mode == NmeaFaaMode::Differential);
    return true;
}

bool NmeaSentenceParser::parseTtm(std::string_view sentence, TtmData& outData, bool verifyChecksum) noexcept
{
    outData = TtmData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--TTM,xx,x.x,x.x,a,x.x,x.x,a,x.x,x.x,a,c--c,a,a,hhmmss.ss,a
    if (tokens.size() < 15U) {
        return false;
    }

    unsigned int targetNum { 0U };
    if (!parseUInt(tokens[1], targetNum)) {
        return false;
    }
    outData.targetNumber = targetNum;

    parseDouble(tokens[2], outData.targetDistanceNmi);
    parseDouble(tokens[3], outData.bearingDegrees);
    outData.bearingReference
        = (!tokens[4].empty() && tokens[4].front() == 'R') ? TtmReference::Relative : TtmReference::True;

    parseDouble(tokens[5], outData.targetSpeedKnots);
    parseDouble(tokens[6], outData.targetCourseDegrees);
    outData.courseReference
        = (!tokens[7].empty() && tokens[7].front() == 'R') ? TtmReference::Relative : TtmReference::True;

    parseDouble(tokens[8], outData.distanceCpaNmi);
    parseDouble(tokens[9], outData.timeCpaMinutes);

    if (!tokens[10].empty()) {
        outData.speedDistanceUnits = tokens[10].front();
    }

    outData.targetName = std::string(tokens[11]);

    if (!tokens[12].empty()) {
        outData.status = static_cast<TtmTargetStatus>(tokens[12].front());
    }

    outData.referenceTarget = (!tokens[13].empty() && tokens[13].front() == 'R');
    static_cast<void>(parseUtcTime(tokens[14], outData.utcTimeTag));

    if (tokens.size() > 15U && !tokens[15].empty()) {
        outData.acquisitionType = tokens[15].front();
    }

    outData.valid = (outData.status != TtmTargetStatus::Lost);
    return true;
}

bool NmeaSentenceParser::parseTll(std::string_view sentence, TllData& outData, bool verifyChecksum) noexcept
{
    outData = TllData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--TLL,xx,llll.ll,a,yyyyy.yy,a,c--c,hhmmss.ss,a,a
    if (tokens.size() < 9U) {
        return false;
    }

    unsigned int targetNum { 0U };
    if (parseUInt(tokens[1], targetNum)) {
        outData.targetNumber = targetNum;
    }

    if (!parseCoordinate(tokens[2], tokens[3], outData.coordinates.latitudeDeg)
        || !parseCoordinate(tokens[4], tokens[5], outData.coordinates.longitudeDeg)) {
        return false;
    }

    outData.targetName = std::string(tokens[6]);
    static_cast<void>(parseUtcTime(tokens[7], outData.utcTimeTag));

    if (!tokens[8].empty()) {
        outData.status = static_cast<TtmTargetStatus>(tokens[8].front());
    }

    if (tokens.size() > 9U && !tokens[9].empty()) {
        outData.referenceTarget = (tokens[9].front() == 'R');
    }

    outData.valid = (outData.status != TtmTargetStatus::Lost);
    return true;
}

bool NmeaSentenceParser::parseXdr(std::string_view sentence, XdrData& outData, bool verifyChecksum) noexcept
{
    outData = XdrData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--XDR,a,x.x,a,c--c,... (repeating groups of 4 fields)
    if (tokens.size() < 5U) {
        return false;
    }

    std::size_t idx { 1U };
    while (idx + 3U < tokens.size()) {
        XdrTransducer tr {};
        if (!tokens[idx].empty()) {
            tr.type = tokens[idx].front();
        }
        parseDouble(tokens[idx + 1U], tr.measurement);
        if (!tokens[idx + 2U].empty()) {
            tr.units = tokens[idx + 2U].front();
        }
        tr.id = std::string(tokens[idx + 3U]);
        outData.transducers.push_back(std::move(tr));
        idx += 4U;
    }

    outData.valid = !outData.transducers.empty();
    return outData.valid;
}

bool NmeaSentenceParser::parsePfecPos(
    std::string_view sentence, PfecGimbalPosition& outPos, bool verifyChecksum) noexcept
{
    outPos = PfecGimbalPosition {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $PFEC,GPpos,pan,tilt
    if (tokens.size() < 4U) {
        return false;
    }

    if (tokens[0] != "PFEC" || tokens[1] != "GPpos") {
        return false;
    }

    if (!parseDouble(tokens[2], outPos.panDegrees) || !parseDouble(tokens[3], outPos.tiltDegrees)) {
        return false;
    }

    outPos.timestamp = std::chrono::steady_clock::now();
    outPos.valid = true;
    return true;
}

bool NmeaSentenceParser::parsePashr(
    std::string_view sentence, PashrData& outData, bool verifyChecksum) noexcept
{
    outData = PashrData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $PASHR,hhmmss.ss,hhh.hh,T,rrr.rr,ppp.pp,hhh.hh,r.rrr,p.ppp,h.hhh,q,a
    if (tokens.size() < 6U) {
        return false;
    }

    const auto header = tokens[0];
    if (header != "PASHR" && (header.size() < 5U || header.substr(header.size() - 5U) != "PASHR")) {
        return false;
    }

    static_cast<void>(parseUtcTime(tokens[1], outData.utcTime));
    parseDouble(tokens[2], outData.headingDegrees);
    outData.isTrueHeading = (tokens[3] != "M" && tokens[3] != "m");
    parseDouble(tokens[4], outData.rollDegrees);
    parseDouble(tokens[5], outData.pitchDegrees);

    if (tokens.size() > 6U) {
        parseDouble(tokens[6], outData.heaveMeters);
    }
    if (tokens.size() > 7U) {
        parseDouble(tokens[7], outData.rollAccuracyDeg);
    }
    if (tokens.size() > 8U) {
        parseDouble(tokens[8], outData.pitchAccuracyDeg);
    }
    if (tokens.size() > 9U) {
        parseDouble(tokens[9], outData.headingAccuracyDeg);
    }
    if (tokens.size() > 10U) {
        unsigned int q { 0U };
        if (parseUInt(tokens[10], q)) {
            outData.gpsQualityFlag = static_cast<std::uint8_t>(q);
        }
    }
    if (tokens.size() > 11U) {
        unsigned int imu { 0U };
        if (parseUInt(tokens[11], imu)) {
            outData.imuStatusFlag = static_cast<std::uint8_t>(imu);
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parsePfecAtt(
    std::string_view sentence, PfecAttitudeData& outAtt, bool verifyChecksum) noexcept
{
    outAtt = PfecAttitudeData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $PFEC,GPatt,yaw,pitch,roll
    if (tokens.size() < 5U) {
        return false;
    }

    if (tokens[0] != "PFEC" || tokens[1] != "GPatt") {
        return false;
    }

    if (!parseDouble(tokens[2], outAtt.yawDegrees)
        || !parseDouble(tokens[3], outAtt.pitchDegrees)
        || !parseDouble(tokens[4], outAtt.rollDegrees)) {
        return false;
    }

    outAtt.valid = true;
    return true;
}

bool NmeaSentenceParser::parseRsd(std::string_view sentence, RsdData& outData, bool verifyChecksum) noexcept
{
    outData = RsdData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--RSD,d1,b1,d2,b2,d3,b3,d4,b4,cursorRange,cursorBearing,rangeScale,rotation
    if (tokens.size() < 13U) {
        return false;
    }

    parseDouble(tokens[9], outData.cursorRangeNmi);
    parseDouble(tokens[10], outData.cursorBearingDeg);
    parseDouble(tokens[11], outData.rangeScaleNmi);
    if (!tokens[12].empty()) {
        outData.displayRotation = tokens[12].front();
    }
    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseOsd(std::string_view sentence, OsdData& outData, bool verifyChecksum) noexcept
{
    outData = OsdData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--OSD,heading,headingStatus,course,courseRef,speed,speedRef,set,drift,speedUnits
    if (tokens.size() < 10U) {
        return false;
    }

    parseDouble(tokens[1], outData.headingDegrees);
    outData.headingValid = (!tokens[2].empty() && tokens[2].front() == 'A');
    parseDouble(tokens[3], outData.courseDegrees);
    if (!tokens[4].empty()) {
        outData.courseReference = tokens[4].front();
    }
    parseDouble(tokens[5], outData.vesselSpeed);
    if (!tokens[6].empty()) {
        outData.speedReference = tokens[6].front();
    }
    parseDouble(tokens[7], outData.vesselSetDeg);
    parseDouble(tokens[8], outData.vesselDriftSpeed);
    if (!tokens[9].empty()) {
        outData.speedUnits = tokens[9].front();
    }

    outData.valid = outData.headingValid;
    return true;
}

bool NmeaSentenceParser::parseApb(std::string_view sentence, ApbData& outData, bool verifyChecksum) noexcept
{
    outData = ApbData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format:
    // $--APB,status1,status2,xte,steerDir,xteUnits,arrCircle,perpPassed,brgOrigDest,brgOrigRef,destWpt,brgPresDest,brgPresRef,headingSteer,headingSteerRef,faaMode
    if (tokens.size() < 15U) {
        return false;
    }

    outData.generalWarning = (!tokens[1].empty() && tokens[1].front() == 'V');
    outData.cycleLockWarning = (!tokens[2].empty() && tokens[2].front() == 'V');
    parseDouble(tokens[3], outData.crossTrackErrorNmi);
    if (!tokens[4].empty()) {
        outData.directionToSteer = tokens[4].front();
    }
    if (!tokens[5].empty()) {
        outData.xteUnits = tokens[5].front();
    }
    outData.arrivalCircleEntered = (!tokens[6].empty() && tokens[6].front() == 'A');
    outData.perpendicularPassed = (!tokens[7].empty() && tokens[7].front() == 'A');
    parseDouble(tokens[8], outData.bearingOriginToDestDeg);
    if (!tokens[9].empty()) {
        outData.bearingOriginRef = tokens[9].front();
    }
    outData.destWaypointId = std::string(tokens[10]);
    parseDouble(tokens[11], outData.bearingPresentToDestDeg);
    if (!tokens[12].empty()) {
        outData.bearingPresentRef = tokens[12].front();
    }
    parseDouble(tokens[13], outData.headingToSteerDeg);
    if (!tokens[14].empty()) {
        outData.headingToSteerRef = tokens[14].front();
    }
    if (tokens.size() > 15U && !tokens[15].empty()) {
        outData.faaMode = static_cast<NmeaFaaMode>(tokens[15].front());
    }

    outData.valid = !outData.generalWarning;
    return true;
}

bool NmeaSentenceParser::parseBwc(std::string_view sentence, BwcData& outData, bool verifyChecksum) noexcept
{
    outData = BwcData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--BWC,utcTime,lat,N/S,lon,E/W,brgTrue,T,brgMag,M,dist,N,wptId,faaMode
    if (tokens.size() < 13U) {
        return false;
    }

    (void)parseUtcTime(tokens[1], outData.utcTime);
    if (!parseCoordinate(tokens[2], tokens[3], outData.waypointCoordinates.latitudeDeg)
        || !parseCoordinate(tokens[4], tokens[5], outData.waypointCoordinates.longitudeDeg)) {
        return false;
    }
    parseDouble(tokens[6], outData.bearingTrueDeg);
    parseDouble(tokens[8], outData.bearingMagneticDeg);
    parseDouble(tokens[10], outData.distanceNmi);
    outData.waypointId = std::string(tokens[12]);
    if (tokens.size() > 13U && !tokens[13].empty()) {
        outData.faaMode = static_cast<NmeaFaaMode>(tokens[13].front());
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseMwv(std::string_view sentence, MwvData& outData, bool verifyChecksum) noexcept
{
    outData = MwvData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--MWV,windAngle,ref,windSpeed,speedUnits,status
    if (tokens.size() < 6U) {
        return false;
    }

    parseDouble(tokens[1], outData.windAngleDeg);
    if (!tokens[2].empty()) {
        outData.reference = tokens[2].front();
    }
    parseDouble(tokens[3], outData.windSpeed);
    if (!tokens[4].empty()) {
        outData.speedUnits = tokens[4].front();
    }
    const bool statusValid = (!tokens[5].empty() && tokens[5].front() == 'A');

    outData.valid = statusValid;
    return outData.valid;
}

bool NmeaSentenceParser::parseHdg(std::string_view sentence, HdgData& outData, bool verifyChecksum) noexcept
{
    outData = HdgData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--HDG,magHeading,magDev,devDir,magVar,varDir
    if (tokens.size() < 2U) {
        return false;
    }

    if (!parseDouble(tokens[1], outData.magneticHeadingDeg)) {
        return false;
    }

    if (tokens.size() >= 4U && !tokens[2].empty()) {
        double devVal { 0.0 };
        if (parseDouble(tokens[2], devVal)) {
            outData.hasDeviation = true;
            outData.magneticDeviationDeg = (!tokens[3].empty() && tokens[3].front() == 'W') ? -devVal : devVal;
        }
    }

    if (tokens.size() >= 6U && !tokens[4].empty()) {
        double varVal { 0.0 };
        if (parseDouble(tokens[4], varVal)) {
            outData.hasVariation = true;
            outData.magneticVariationDeg = (!tokens[5].empty() && tokens[5].front() == 'W') ? -varVal : varVal;
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseRmb(std::string_view sentence, RmbData& outData, bool verifyChecksum) noexcept
{
    outData = RmbData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--RMB,status,xte,steerDir,destId,origId,lat,latHem,lon,lonHem,range,bearing,vmg,arrival,faa
    if (tokens.size() < 14U) {
        return false;
    }

    outData.statusActive = (!tokens[1].empty() && tokens[1].front() == 'A');
    parseDouble(tokens[2], outData.crossTrackErrorNmi);
    if (!tokens[3].empty()) {
        outData.directionToSteer = tokens[3].front();
    }
    outData.destWaypointId = std::string(tokens[4]);
    outData.originWaypointId = std::string(tokens[5]);

    if (!tokens[6].empty() && !tokens[7].empty() && !tokens[8].empty() && !tokens[9].empty()) {
        double lat { 0.0 };
        double lon { 0.0 };
        if (parseCoordinate(tokens[6], tokens[7], lat) && parseCoordinate(tokens[8], tokens[9], lon)) {
            outData.destCoordinates.latitudeDeg = lat;
            outData.destCoordinates.longitudeDeg = lon;
        }
    }

    parseDouble(tokens[10], outData.rangeToDestNmi);
    parseDouble(tokens[11], outData.bearingToDestTrueDeg);
    parseDouble(tokens[12], outData.closingVelocityKnots);

    outData.arrivalAlarm = (!tokens[13].empty() && tokens[13].front() == 'A');

    if (tokens.size() >= 15U && !tokens[14].empty()) {
        outData.faaMode = static_cast<NmeaFaaMode>(tokens[14].front());
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseRte(std::string_view sentence, RteData& outData, bool verifyChecksum) noexcept
{
    outData = RteData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--RTE,totalSentences,sentenceNum,routeType,routeName,wpt1,wpt2,...
    if (tokens.size() < 5U) {
        return false;
    }

    double totalVal { 1.0 };
    double numVal { 1.0 };
    if (parseDouble(tokens[1], totalVal)) {
        outData.totalSentences = static_cast<std::uint32_t>(totalVal);
    }
    if (parseDouble(tokens[2], numVal)) {
        outData.sentenceNumber = static_cast<std::uint32_t>(numVal);
    }

    if (!tokens[3].empty()) {
        outData.routeType = tokens[3].front();
    }
    outData.routeName = std::string(tokens[4]);

    for (std::size_t i = 5U; i < tokens.size(); ++i) {
        if (!tokens[i].empty()) {
            outData.waypointIds.emplace_back(tokens[i]);
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseWpl(std::string_view sentence, WplData& outData, bool verifyChecksum) noexcept
{
    outData = WplData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--WPL,lat,latHem,lon,lonHem,wptId
    if (tokens.size() < 6U) {
        return false;
    }

    double lat { 0.0 };
    double lon { 0.0 };
    if (!parseCoordinate(tokens[1], tokens[2], lat) || !parseCoordinate(tokens[3], tokens[4], lon)) {
        return false;
    }

    outData.coordinates.latitudeDeg = lat;
    outData.coordinates.longitudeDeg = lon;
    outData.waypointId = std::string(tokens[5]);
    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseMtw(std::string_view sentence, MtwData& outData, bool verifyChecksum) noexcept
{
    outData = MtwData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--MTW,tempC,C
    if (tokens.size() < 3U) {
        return false;
    }

    if (!parseDouble(tokens[1], outData.waterTemperatureCelsius)) {
        return false;
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseMmb(std::string_view sentence, MmbData& outData, bool verifyChecksum) noexcept
{
    outData = MmbData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--MMB,pressInHg,I,pressBar,B
    if (tokens.size() < 5U) {
        return false;
    }

    parseDouble(tokens[1], outData.pressureInHg);
    parseDouble(tokens[3], outData.pressureBars);

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseMda(std::string_view sentence, MdaData& outData, bool verifyChecksum) noexcept
{
    outData = MdaData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format:
    // $--MDA,pressInHg,I,pressBar,B,airTemp,C,waterTemp,C,relHum,absHum,dewPoint,C,windDirT,T,windDirM,M,windSpdKnots,N,windSpdMps,M
    if (tokens.size() < 2U) {
        return false;
    }

    auto parseOptDouble = [](std::string_view sv) -> std::optional<double> {
        if (sv.empty()) {
            return std::nullopt;
        }
        double val { 0.0 };
        if (parseDouble(sv, val)) {
            return val;
        }
        return std::nullopt;
    };

    if (tokens.size() > 1U) {
        outData.barometricPressureInHg = parseOptDouble(tokens[1]);
    }
    if (tokens.size() > 3U) {
        outData.barometricPressureBars = parseOptDouble(tokens[3]);
    }
    if (tokens.size() > 5U) {
        outData.airTemperatureCelsius = parseOptDouble(tokens[5]);
    }
    if (tokens.size() > 7U) {
        outData.waterTemperatureCelsius = parseOptDouble(tokens[7]);
    }
    if (tokens.size() > 9U) {
        outData.relativeHumidityPercent = parseOptDouble(tokens[9]);
    }
    if (tokens.size() > 10U) {
        outData.absoluteHumidityGPerM3 = parseOptDouble(tokens[10]);
    }
    if (tokens.size() > 11U) {
        outData.dewPointCelsius = parseOptDouble(tokens[11]);
    }
    if (tokens.size() > 13U) {
        outData.windDirectionTrueDeg = parseOptDouble(tokens[13]);
    }
    if (tokens.size() > 15U) {
        outData.windDirectionMagneticDeg = parseOptDouble(tokens[15]);
    }
    if (tokens.size() > 17U) {
        outData.windSpeedKnots = parseOptDouble(tokens[17]);
    }
    if (tokens.size() > 19U) {
        outData.windSpeedMps = parseOptDouble(tokens[19]);
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseGsa(std::string_view sentence, GsaData& outData, bool verifyChecksum) noexcept
{
    outData = GsaData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--GSA,mode,fixMode,sat1..sat12,pdop,hdop,vdop[,systemId]
    if (tokens.size() < 18U) {
        return false;
    }

    if (!tokens[1].empty()) {
        outData.selectionMode = tokens[1].front();
    }

    unsigned int fixModeVal { 0U };
    if (parseUInt(tokens[2], fixModeVal)) {
        outData.fixMode = static_cast<std::uint8_t>(fixModeVal);
    }

    for (std::size_t i = 3U; i <= 14U; ++i) {
        if (!tokens[i].empty()) {
            unsigned int prnVal { 0U };
            if (parseUInt(tokens[i], prnVal)) {
                outData.activeSatellitePrns.push_back(static_cast<std::uint8_t>(prnVal));
            }
        }
    }

    parseDouble(tokens[15], outData.pdop);
    parseDouble(tokens[16], outData.hdop);
    parseDouble(tokens[17], outData.vdop);

    if (tokens.size() > 18U && !tokens[18].empty()) {
        unsigned int sysIdVal { 0U };
        if (parseUInt(tokens[18], sysIdVal)) {
            outData.systemId = static_cast<std::uint8_t>(sysIdVal);
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseGsv(std::string_view sentence, GsvData& outData, bool verifyChecksum) noexcept
{
    outData = GsvData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--GSV,totalSentences,sentenceNumber,totalSatsInView,[prn,elev,azim,snr]x1..4[,signalId]
    if (tokens.size() < 4U) {
        return false;
    }

    unsigned int totalSentencesVal { 0U };
    if (parseUInt(tokens[1], totalSentencesVal)) {
        outData.totalSentences = static_cast<std::uint8_t>(totalSentencesVal);
    }

    unsigned int sentenceNumberVal { 0U };
    if (parseUInt(tokens[2], sentenceNumberVal)) {
        outData.sentenceNumber = static_cast<std::uint8_t>(sentenceNumberVal);
    }

    unsigned int totalSatsVal { 0U };
    if (parseUInt(tokens[3], totalSatsVal)) {
        outData.totalSatellitesInView = static_cast<std::uint16_t>(totalSatsVal);
    }

    std::size_t idx { 4U };
    while (idx + 3U < tokens.size()) {
        // Check if remaining token is a trailing signalId (1 token left at end)
        if (idx + 4U == tokens.size() && tokens[idx].size() <= 2U) {
            // Might be trailing signalId if no more 4-tuples can be formed
        }
        GsvSatelliteInfo sat {};
        unsigned int prnVal { 0U };
        if (parseUInt(tokens[idx], prnVal)) {
            sat.prn = static_cast<std::uint16_t>(prnVal);
            parseDouble(tokens[idx + 1U], sat.elevationDeg);
            parseDouble(tokens[idx + 2U], sat.azimuthDeg);
            if (!tokens[idx + 3U].empty()) {
                double snrVal { 0.0 };
                if (parseDouble(tokens[idx + 3U], snrVal)) {
                    sat.snrDb = snrVal;
                }
            }
            outData.satellites.push_back(sat);
        }
        idx += 4U;
    }

    if (idx < tokens.size() && !tokens[idx].empty()) {
        unsigned int sigVal { 0U };
        if (parseUInt(tokens[idx], sigVal)) {
            outData.signalId = static_cast<std::uint8_t>(sigVal);
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseZda(std::string_view sentence, ZdaData& outData, bool verifyChecksum) noexcept
{
    outData = ZdaData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--ZDA,hhmmss.ss,day,month,year,localZoneHours,localZoneMinutes
    if (tokens.size() < 7U) {
        return false;
    }

    if (!parseUtcTime(tokens[1], outData.utcTime)) {
        return false;
    }

    unsigned int dayVal { 0U };
    if (parseUInt(tokens[2], dayVal)) {
        outData.day = static_cast<std::uint8_t>(dayVal);
    }

    unsigned int monthVal { 0U };
    if (parseUInt(tokens[3], monthVal)) {
        outData.month = static_cast<std::uint8_t>(monthVal);
    }

    unsigned int yearVal { 0U };
    if (parseUInt(tokens[4], yearVal)) {
        outData.year = static_cast<std::uint16_t>(yearVal);
    }

    if (!tokens[5].empty()) {
        int zh { 0 };
        const auto res = std::from_chars(tokens[5].data(), tokens[5].data() + tokens[5].size(), zh);
        if (res.ec == std::errc {}) {
            outData.localZoneHours = static_cast<std::int8_t>(zh);
        }
    }

    unsigned int zmVal { 0U };
    if (parseUInt(tokens[6], zmVal)) {
        outData.localZoneMinutes = static_cast<std::uint8_t>(zmVal);
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseVbw(std::string_view sentence, VbwData& outData, bool verifyChecksum) noexcept
{
    outData = VbwData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--VBW,longWater,transWater,waterStat,longGround,transGround,groundStat[,sternWater,sternWStat,sternGround,sternGStat]
    if (tokens.size() < 7U) {
        return false;
    }

    parseDouble(tokens[1], outData.longitudinalWaterSpeedKnots);
    parseDouble(tokens[2], outData.transverseWaterSpeedKnots);
    if (!tokens[3].empty()) {
        outData.waterSpeedStatus = tokens[3].front();
    }

    parseDouble(tokens[4], outData.longitudinalGroundSpeedKnots);
    parseDouble(tokens[5], outData.transverseGroundSpeedKnots);
    if (!tokens[6].empty()) {
        outData.groundSpeedStatus = tokens[6].front();
    }

    if (tokens.size() > 7U && !tokens[7].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[7], val)) {
            outData.sternWaterSpeedKnots = val;
        }
    }
    if (tokens.size() > 8U && !tokens[8].empty()) {
        outData.sternWaterStatus = tokens[8].front();
    }
    if (tokens.size() > 9U && !tokens[9].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[9], val)) {
            outData.sternGroundSpeedKnots = val;
        }
    }
    if (tokens.size() > 10U && !tokens[10].empty()) {
        outData.sternGroundStatus = tokens[10].front();
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseVhw(std::string_view sentence, VhwData& outData, bool verifyChecksum) noexcept
{
    outData = VhwData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--VHW,headingTrue,T,headingMag,M,speedKnots,N,speedKmh,K
    if (tokens.size() < 6U) {
        return false;
    }

    if (!tokens[1].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[1], val)) {
            outData.headingDegreesTrue = val;
        }
    }
    if (tokens.size() > 3U && !tokens[3].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[3], val)) {
            outData.headingDegreesMagnetic = val;
        }
    }
    if (tokens.size() > 5U && !tokens[5].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[5], val)) {
            outData.speedWaterKnots = val;
        }
    }
    if (tokens.size() > 7U && !tokens[7].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[7], val)) {
            outData.speedWaterKmh = val;
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseDpt(std::string_view sentence, DptData& outData, bool verifyChecksum) noexcept
{
    outData = DptData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--DPT,depth,offset[,maxRange]
    if (tokens.size() < 3U) {
        return false;
    }

    parseDouble(tokens[1], outData.waterDepthMeters);
    parseDouble(tokens[2], outData.offsetMeters);

    if (tokens.size() > 3U && !tokens[3].empty()) {
        double val { 0.0 };
        if (parseDouble(tokens[3], val)) {
            outData.maximumRangeScaleMeters = val;
        }
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseDbt(std::string_view sentence, DbtData& outData, bool verifyChecksum) noexcept
{
    outData = DbtData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--DBT,depthFeet,f,depthMeters,M,depthFathoms,F
    if (tokens.size() < 7U) {
        return false;
    }

    parseDouble(tokens[1], outData.depthFeet);
    parseDouble(tokens[3], outData.depthMeters);
    parseDouble(tokens[5], outData.depthFathoms);

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseRsa(std::string_view sentence, RsaData& outData, bool verifyChecksum) noexcept
{
    outData = RsaData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--RSA,starboardAngle,starboardStatus,portAngle,portStatus
    if (tokens.size() < 3U) {
        return false;
    }

    if (parseDouble(tokens[1], outData.starboardRudderAngleDeg)) {
        if (tokens.size() > 2U && !tokens[2].empty() && tokens[2][0] == 'A') {
            outData.starboardValid = true;
        }
    }

    if (tokens.size() >= 5U) {
        if (parseDouble(tokens[3], outData.portRudderAngleDeg)) {
            if (!tokens[4].empty() && tokens[4][0] == 'A') {
                outData.portValid = true;
            }
        }
    }

    outData.valid = outData.starboardValid || outData.portValid;
    return outData.valid;
}

bool NmeaSentenceParser::parseAlf(
    std::string_view sentence, Bam::AlfData& outData, bool verifyChecksum) noexcept
{
    outData = Bam::AlfData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--ALF,totSentences,sentNum,seqMsgId,timeLastChange,priority,category,state,alertId,alertInst,revCnt,escCnt,alertText
    if (tokens.size() < 13U) {
        return false;
    }

    unsigned int totSentencesVal { 0U };
    if (parseUInt(tokens[1], totSentencesVal)) {
        outData.totalSentences = static_cast<std::uint8_t>(totSentencesVal);
    }

    unsigned int sentNumVal { 0U };
    if (parseUInt(tokens[2], sentNumVal)) {
        outData.sentenceNumber = static_cast<std::uint8_t>(sentNumVal);
    }

    unsigned int seqMsgIdVal { 0U };
    if (parseUInt(tokens[3], seqMsgIdVal)) {
        outData.sequentialMessageId = static_cast<std::uint8_t>(seqMsgIdVal);
    }

    if (!tokens[4].empty()) {
        static_cast<void>(parseUtcTime(tokens[4], outData.timeOfLastChange));
    }

    if (!tokens[5].empty()) {
        outData.alertPriority = tokens[5].front();
    }
    if (!tokens[6].empty()) {
        outData.alertCategory = tokens[6].front();
    }
    if (!tokens[7].empty()) {
        outData.alertState = tokens[7].front();
    }

    unsigned int alertIdVal { 0U };
    if (parseUInt(tokens[8], alertIdVal)) {
        outData.alertIdentifier = static_cast<std::uint32_t>(alertIdVal);
    }

    unsigned int alertInstVal { 0U };
    if (parseUInt(tokens[9], alertInstVal)) {
        outData.alertInstance = static_cast<std::uint32_t>(alertInstVal);
    }

    unsigned int revCntVal { 0U };
    if (parseUInt(tokens[10], revCntVal)) {
        outData.revisionCounter = static_cast<std::uint32_t>(revCntVal);
    }

    unsigned int escCntVal { 0U };
    if (parseUInt(tokens[11], escCntVal)) {
        outData.escalationCounter = static_cast<std::uint32_t>(escCntVal);
    }

    outData.alertText = std::string(tokens[12]);
    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseAlc(
    std::string_view sentence, Bam::AlcData& outData, bool verifyChecksum) noexcept
{
    outData = Bam::AlcData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--ALC,totSentences,sentNum,seqMsgId,alertCount[,alertId,alertInst,revCnt]...
    if (tokens.size() < 5U) {
        return false;
    }

    unsigned int totSentencesVal { 0U };
    if (parseUInt(tokens[1], totSentencesVal)) {
        outData.totalSentences = static_cast<std::uint8_t>(totSentencesVal);
    }

    unsigned int sentNumVal { 0U };
    if (parseUInt(tokens[2], sentNumVal)) {
        outData.sentenceNumber = static_cast<std::uint8_t>(sentNumVal);
    }

    unsigned int seqMsgIdVal { 0U };
    if (parseUInt(tokens[3], seqMsgIdVal)) {
        outData.sequentialMessageId = static_cast<std::uint8_t>(seqMsgIdVal);
    }

    unsigned int alertCntVal { 0U };
    if (parseUInt(tokens[4], alertCntVal)) {
        outData.alertCount = static_cast<std::uint8_t>(alertCntVal);
    }

    std::size_t idx { 5U };
    while (idx + 2U < tokens.size()) {
        Bam::AlcEntry entry {};
        unsigned int idVal { 0U };
        unsigned int instVal { 0U };
        unsigned int revVal { 0U };
        if (parseUInt(tokens[idx], idVal) && parseUInt(tokens[idx + 1U], instVal) && parseUInt(tokens[idx + 2U], revVal)) {
            entry.alertIdentifier = static_cast<std::uint32_t>(idVal);
            entry.alertInstance = static_cast<std::uint32_t>(instVal);
            entry.revisionCounter = static_cast<std::uint32_t>(revVal);
            outData.alertEntries.push_back(entry);
        }
        idx += 3U;
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseArc(
    std::string_view sentence, Bam::ArcData& outData, bool verifyChecksum) noexcept
{
    outData = Bam::ArcData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--ARC,releaseTime,alertId,alertInst,command
    if (tokens.size() < 5U) {
        return false;
    }

    if (!tokens[1].empty()) {
        static_cast<void>(parseUtcTime(tokens[1], outData.releaseTime));
    }

    unsigned int idVal { 0U };
    if (parseUInt(tokens[2], idVal)) {
        outData.alertIdentifier = static_cast<std::uint32_t>(idVal);
    }

    unsigned int instVal { 0U };
    if (parseUInt(tokens[3], instVal)) {
        outData.alertInstance = static_cast<std::uint32_t>(instVal);
    }

    if (!tokens[4].empty()) {
        outData.command = tokens[4].front();
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseHbt(
    std::string_view sentence, Bam::HbtData& outData, bool verifyChecksum) noexcept
{
    outData = Bam::HbtData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--HBT,intervalSec,equipmentStatus,seqSentenceId
    if (tokens.size() < 4U) {
        return false;
    }

    parseDouble(tokens[1], outData.configuredIntervalSec);

    if (!tokens[2].empty()) {
        outData.equipmentStatus = tokens[2].front();
    }

    unsigned int seqIdVal { 0U };
    if (parseUInt(tokens[3], seqIdVal)) {
        outData.sequentialSentenceId = static_cast<std::uint8_t>(seqIdVal);
    }

    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseAlr(
    std::string_view sentence, Bam::AlrData& outData, bool verifyChecksum) noexcept
{
    outData = Bam::AlrData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--ALR,timeChange,alertId,condition,ackState,alertText
    if (tokens.size() < 6U) {
        return false;
    }

    if (!tokens[1].empty()) {
        static_cast<void>(parseUtcTime(tokens[1], outData.timeOfLastChange));
    }

    unsigned int idVal { 0U };
    if (parseUInt(tokens[2], idVal)) {
        outData.alertIdentifier = static_cast<std::uint32_t>(idVal);
    }

    if (!tokens[3].empty()) {
        outData.condition = tokens[3].front();
    }

    if (!tokens[4].empty()) {
        outData.acknowledgeState = tokens[4].front();
    }

    outData.alertText = std::string(tokens[5]);
    outData.valid = true;
    return true;
}

bool NmeaSentenceParser::parseAck(
    std::string_view sentence, Bam::AckData& outData, bool verifyChecksum) noexcept
{
    outData = Bam::AckData {};
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    tokenize(sentence, tokens);

    // Format: $--ACK,alertId
    if (tokens.size() < 2U) {
        return false;
    }

    unsigned int idVal { 0U };
    if (parseUInt(tokens[1], idVal)) {
        outData.alertIdentifier = static_cast<std::uint32_t>(idVal);
    }

    outData.valid = true;
    return true;
}

} // namespace Nmea
