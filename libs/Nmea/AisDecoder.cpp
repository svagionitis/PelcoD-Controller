/// @file AisDecoder.cpp
/// @brief Implementation of ITU-R M.1371 AIS message decoder and multi-sentence assembler.

#include "AisDecoder.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace Nmea {

namespace {

    bool parseUIntToken(std::string_view sv, unsigned int& outVal) noexcept
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

AisDecoder::AisDecoder() = default;

void AisDecoder::setFragmentTtl(std::chrono::milliseconds ttl) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_fragmentTtl = ttl;
}

void AisDecoder::reset()
{
    std::scoped_lock lock(m_mutex);
    m_fragments.clear();
}

void AisDecoder::pruneStaleFragmentsLocked(std::chrono::steady_clock::time_point now)
{
    for (auto it = m_fragments.begin(); it != m_fragments.end();) {
        if (now - it->second.lastSeen > m_fragmentTtl) {
            it = m_fragments.erase(it);
        } else {
            ++it;
        }
    }
}

bool AisDecoder::decodeSentence(std::string_view sentence, AisVesselTarget& outTarget, bool verifyChecksum)
{
    if (verifyChecksum && !NmeaChecksum::validate(sentence)) {
        return false;
    }

    std::vector<std::string_view> tokens {};
    NmeaSentenceParser::tokenize(sentence, tokens);

    // Format: !AIVDM,totalSentences,sentenceNum,seqId,channel,payload,fillBits
    if (tokens.size() < 7U) {
        return false;
    }

    const auto header = tokens[0];
    if (header != "AIVDM" && header != "AIVDO") {
        return false;
    }

    unsigned int totalSentences { 0U };
    unsigned int sentenceNum { 0U };
    unsigned int fillBits { 0U };

    if (!parseUIntToken(tokens[1], totalSentences) || !parseUIntToken(tokens[2], sentenceNum)
        || !parseUIntToken(tokens[6], fillBits)) {
        return false;
    }

    if (totalSentences == 0U || sentenceNum == 0U || sentenceNum > totalSentences) {
        return false;
    }

    const char channel = (!tokens[4].empty()) ? tokens[4].front() : 'A';
    unsigned int seqId { 0U };
    if (!tokens[3].empty()) {
        parseUIntToken(tokens[3], seqId);
    }

    const std::string_view payload = tokens[5];

    // Single-sentence fast path
    if (totalSentences == 1U) {
        return decodePayload(payload, fillBits, outTarget);
    }

    // Multi-sentence reassembly path
    std::string assembledPayload {};
    std::size_t finalFillBits { 0U };

    {
        std::scoped_lock lock(m_mutex);
        const auto now = std::chrono::steady_clock::now();
        pruneStaleFragmentsLocked(now);

        const auto key = std::make_pair(channel, static_cast<std::uint8_t>(seqId));
        auto it = m_fragments.find(key);

        if (sentenceNum == 1U) {
            MultiPartFragment frag {};
            frag.totalParts = static_cast<std::uint8_t>(totalSentences);
            frag.lastPart = 1U;
            frag.combinedPayload = std::string(payload);
            frag.fillBits = fillBits;
            frag.lastSeen = now;
            m_fragments[key] = std::move(frag);
            return false;
        }

        if (it == m_fragments.end() || it->second.lastPart + 1U != sentenceNum) {
            // Missing preceding part, discard sequence
            if (it != m_fragments.end()) {
                m_fragments.erase(it);
            }
            return false;
        }

        it->second.combinedPayload.append(payload);
        it->second.lastPart = static_cast<std::uint8_t>(sentenceNum);
        it->second.fillBits = fillBits;
        it->second.lastSeen = now;

        if (sentenceNum == totalSentences) {
            assembledPayload = std::move(it->second.combinedPayload);
            finalFillBits = it->second.fillBits;
            m_fragments.erase(it);
        } else {
            return false;
        }
    }

    return decodePayload(assembledPayload, finalFillBits, outTarget);
}

bool AisDecoder::decodePayload(
    std::string_view armoredPayload, std::size_t fillBits, AisVesselTarget& outTarget) noexcept
{
    outTarget = AisVesselTarget {};
    if (armoredPayload.empty()) {
        return false;
    }

    AisBitReader reader(armoredPayload, fillBits);
    if (reader.remainingBits() < 38U) {
        return false;
    }

    const auto msgType = static_cast<AisMessageType>(reader.readBits(6U));
    outTarget.messageType = msgType;
    outTarget.repeatIndicator = static_cast<std::uint8_t>(reader.readBits(2U));
    outTarget.mmsi = reader.readBits(30U);

    switch (msgType) {
    case AisMessageType::ClassAPosition1:
    case AisMessageType::ClassAPosition2:
    case AisMessageType::ClassAPosition3:
        return decodeClassAPosition(reader, outTarget);

    case AisMessageType::ClassAStaticVoyage5:
        return decodeClassAStatic(reader, outTarget);

    case AisMessageType::ClassBPosition18:
        return decodeClassBPosition(reader, outTarget);

    case AisMessageType::ClassBExtendedPosition19:
        return decodeClassBExtendedPosition(reader, outTarget);

    case AisMessageType::ClassBStatic24:
        return decodeClassBStatic(reader, outTarget);

    default:
        break;
    }

    return false;
}

bool AisDecoder::decodeClassAPosition(AisBitReader& reader, AisVesselTarget& target) noexcept
{
    // Total required bits for Types 1, 2, 3: 168 bits (38 bits already read)
    if (reader.remainingBits() < 130U) {
        return false;
    }

    target.navStatus = static_cast<AisNavStatus>(reader.readBits(4U));

    const auto rawRot = reader.readSignedBits(8U);
    if (rawRot == -128) {
        target.rateOfTurnDegPerMin = 0.0;
    } else {
        // ITU-R M.1371: ROT_ais = 4.733 * sqrt(ROT_sensor)
        const double rotAis = static_cast<double>(rawRot);
        const double rotSens = std::pow(rotAis / 4.733, 2.0);
        target.rateOfTurnDegPerMin = (rawRot >= 0) ? rotSens : -rotSens;
    }

    const auto rawSog = reader.readBits(10U);
    target.speedOverGroundKnots = (rawSog < 1023U) ? (static_cast<double>(rawSog) * 0.1) : 0.0;

    target.positionAccuracyHigh = (reader.readBits(1U) == 1U);

    const auto rawLon = reader.readSignedBits(28U);
    const auto rawLat = reader.readSignedBits(27U);

    if (rawLon != 0x6791AC0 && rawLat != 0x3412140) {
        target.coordinates.longitudeDeg = static_cast<double>(rawLon) / 600000.0;
        target.coordinates.latitudeDeg = static_cast<double>(rawLat) / 600000.0;
        target.positionValid
            = (std::abs(target.coordinates.latitudeDeg) <= 90.0 && std::abs(target.coordinates.longitudeDeg) <= 180.0);
    } else {
        target.positionValid = false;
    }

    const auto rawCog = reader.readBits(12U);
    target.courseOverGroundDegrees = (rawCog < 3600U) ? (static_cast<double>(rawCog) * 0.1) : 0.0;

    const auto rawHdg = reader.readBits(9U);
    target.trueHeadingDegrees = (rawHdg < 511U) ? static_cast<double>(rawHdg) : 0.0;

    target.utcSecond = static_cast<std::uint8_t>(reader.readBits(6U));
    target.lastUpdate = std::chrono::steady_clock::now();

    return true;
}

bool AisDecoder::decodeClassAStatic(AisBitReader& reader, AisVesselTarget& target) noexcept
{
    // Total required bits for Type 5: 424 bits (38 already read -> 386 remaining)
    if (reader.remainingBits() < 386U) {
        return false;
    }

    static_cast<void>(reader.readBits(2U)); // AIS Version
    target.imoNumber = reader.readBits(30U);
    target.callSign = reader.readString(7U);
    target.vesselName = reader.readString(20U);
    target.shipType = static_cast<std::uint8_t>(reader.readBits(8U));

    target.dimensions.toBow = static_cast<std::uint16_t>(reader.readBits(9U));
    target.dimensions.toStern = static_cast<std::uint16_t>(reader.readBits(9U));
    target.dimensions.toPort = static_cast<std::uint8_t>(reader.readBits(6U));
    target.dimensions.toStarboard = static_cast<std::uint8_t>(reader.readBits(6U));

    static_cast<void>(reader.readBits(4U)); // EPFD fixture type
    static_cast<void>(reader.readBits(20U)); // ETA (month:4, day:5, hour:5, minute:6)

    const auto rawDraught = reader.readBits(8U);
    target.draughtMeters = static_cast<double>(rawDraught) * 0.1;

    target.destination = reader.readString(20U);
    target.staticDataValid = true;
    target.lastUpdate = std::chrono::steady_clock::now();

    return true;
}

bool AisDecoder::decodeClassBPosition(AisBitReader& reader, AisVesselTarget& target) noexcept
{
    // Total required bits for Type 18: 168 bits (38 already read -> 130 remaining)
    if (reader.remainingBits() < 130U) {
        return false;
    }

    static_cast<void>(reader.readBits(8U)); // Regional reserved

    const auto rawSog = reader.readBits(10U);
    target.speedOverGroundKnots = (rawSog < 1023U) ? (static_cast<double>(rawSog) * 0.1) : 0.0;

    target.positionAccuracyHigh = (reader.readBits(1U) == 1U);

    const auto rawLon = reader.readSignedBits(28U);
    const auto rawLat = reader.readSignedBits(27U);

    if (rawLon != 0x6791AC0 && rawLat != 0x3412140) {
        target.coordinates.longitudeDeg = static_cast<double>(rawLon) / 600000.0;
        target.coordinates.latitudeDeg = static_cast<double>(rawLat) / 600000.0;
        target.positionValid
            = (std::abs(target.coordinates.latitudeDeg) <= 90.0 && std::abs(target.coordinates.longitudeDeg) <= 180.0);
    }

    const auto rawCog = reader.readBits(12U);
    target.courseOverGroundDegrees = (rawCog < 3600U) ? (static_cast<double>(rawCog) * 0.1) : 0.0;

    const auto rawHdg = reader.readBits(9U);
    target.trueHeadingDegrees = (rawHdg < 511U) ? static_cast<double>(rawHdg) : 0.0;

    target.utcSecond = static_cast<std::uint8_t>(reader.readBits(6U));
    target.lastUpdate = std::chrono::steady_clock::now();

    return true;
}

bool AisDecoder::decodeClassBExtendedPosition(AisBitReader& reader, AisVesselTarget& target) noexcept
{
    // Total required bits for Type 19: 312 bits (38 already read -> 274 remaining)
    if (reader.remainingBits() < 274U) {
        return false;
    }

    static_cast<void>(reader.readBits(8U)); // Regional reserved

    const auto rawSog = reader.readBits(10U);
    target.speedOverGroundKnots = (rawSog < 1023U) ? (static_cast<double>(rawSog) * 0.1) : 0.0;

    target.positionAccuracyHigh = (reader.readBits(1U) == 1U);

    const auto rawLon = reader.readSignedBits(28U);
    const auto rawLat = reader.readSignedBits(27U);

    if (rawLon != 0x6791AC0 && rawLat != 0x3412140) {
        target.coordinates.longitudeDeg = static_cast<double>(rawLon) / 600000.0;
        target.coordinates.latitudeDeg = static_cast<double>(rawLat) / 600000.0;
        target.positionValid
            = (std::abs(target.coordinates.latitudeDeg) <= 90.0 && std::abs(target.coordinates.longitudeDeg) <= 180.0);
    }

    const auto rawCog = reader.readBits(12U);
    target.courseOverGroundDegrees = (rawCog < 3600U) ? (static_cast<double>(rawCog) * 0.1) : 0.0;

    const auto rawHdg = reader.readBits(9U);
    target.trueHeadingDegrees = (rawHdg < 511U) ? static_cast<double>(rawHdg) : 0.0;

    target.utcSecond = static_cast<std::uint8_t>(reader.readBits(6U));
    static_cast<void>(reader.readBits(4U)); // Regional reserved

    target.vesselName = reader.readString(20U);
    target.shipType = static_cast<std::uint8_t>(reader.readBits(8U));

    target.dimensions.toBow = static_cast<std::uint16_t>(reader.readBits(9U));
    target.dimensions.toStern = static_cast<std::uint16_t>(reader.readBits(9U));
    target.dimensions.toPort = static_cast<std::uint8_t>(reader.readBits(6U));
    target.dimensions.toStarboard = static_cast<std::uint8_t>(reader.readBits(6U));

    target.staticDataValid = !target.vesselName.empty();
    target.lastUpdate = std::chrono::steady_clock::now();

    return true;
}

bool AisDecoder::decodeClassBStatic(AisBitReader& reader, AisVesselTarget& target) noexcept
{
    // Total bits for Type 24: Part A (160 bits) or Part B (168 bits)
    if (reader.remainingBits() < 122U) {
        return false;
    }

    const auto partNum = reader.readBits(2U);
    if (partNum == 0U) {
        // Part A: Vessel Name
        target.vesselName = reader.readString(20U);
        target.staticDataValid = !target.vesselName.empty();
    } else if (partNum == 1U) {
        // Part B: Ship Type, Vendor ID, Call Sign, Dimensions
        if (reader.remainingBits() >= 128U) {
            target.shipType = static_cast<std::uint8_t>(reader.readBits(8U));
            static_cast<void>(reader.readBits(42U)); // Vendor ID
            target.callSign = reader.readString(7U);

            target.dimensions.toBow = static_cast<std::uint16_t>(reader.readBits(9U));
            target.dimensions.toStern = static_cast<std::uint16_t>(reader.readBits(9U));
            target.dimensions.toPort = static_cast<std::uint8_t>(reader.readBits(6U));
            target.dimensions.toStarboard = static_cast<std::uint8_t>(reader.readBits(6U));
            target.staticDataValid = true;
        }
    }

    target.lastUpdate = std::chrono::steady_clock::now();
    return true;
}

} // namespace Nmea
