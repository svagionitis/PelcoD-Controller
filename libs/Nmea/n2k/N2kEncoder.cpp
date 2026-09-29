#include "N2kEncoder.h"
#include "N2kDecoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Nmea::N2k {

namespace {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kDegToRad = kPi / 180.0;
    constexpr double kMpsToKnots = 1.9438444924406;
    constexpr double kKnotsToMps = 1.0 / kMpsToKnots;

    inline void writeU16LE(std::uint8_t* p, std::uint16_t val) noexcept
    {
        p[0] = static_cast<std::uint8_t>(val & 0xFFU);
        p[1] = static_cast<std::uint8_t>((val >> 8) & 0xFFU);
    }

    inline void writeI16LE(std::uint8_t* p, std::int16_t val) noexcept
    {
        writeU16LE(p, static_cast<std::uint16_t>(val));
    }

    inline void writeU32LE(std::uint8_t* p, std::uint32_t val) noexcept
    {
        p[0] = static_cast<std::uint8_t>(val & 0xFFU);
        p[1] = static_cast<std::uint8_t>((val >> 8) & 0xFFU);
        p[2] = static_cast<std::uint8_t>((val >> 16) & 0xFFU);
        p[3] = static_cast<std::uint8_t>((val >> 24) & 0xFFU);
    }

    inline void writeI32LE(std::uint8_t* p, std::int32_t val) noexcept
    {
        writeU32LE(p, static_cast<std::uint32_t>(val));
    }
} // namespace

CanFrame N2kEncoder::encodePositionRapid(const PositionRapid& pos, std::uint8_t srcAddr, std::uint8_t priority) noexcept
{
    N2kHeader hdr {};
    hdr.priority = priority & 0x07U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::PositionRapidUpdate);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    frame.data.fill(0xFFU);

    if (pos.isValid) {
        const auto rawLat = static_cast<std::int32_t>(std::round(pos.latitudeDeg * 1e7));
        const auto rawLon = static_cast<std::int32_t>(std::round(pos.longitudeDeg * 1e7));
        writeI32LE(frame.data.data(), rawLat);
        writeI32LE(frame.data.data() + 4, rawLon);
    } else {
        writeI32LE(frame.data.data(), Sentinels::kUnavailableInt32);
        writeI32LE(frame.data.data() + 4, Sentinels::kUnavailableInt32);
    }

    return frame;
}

CanFrame N2kEncoder::encodeCogSogRapid(const CogSogRapid& cogSog, std::uint8_t srcAddr, std::uint8_t priority) noexcept
{
    N2kHeader hdr {};
    hdr.priority = priority & 0x07U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::CogSogRapidUpdate);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    frame.data.fill(0xFFU);

    frame.data[0] = cogSog.sid;
    frame.data[1] = static_cast<std::uint8_t>(cogSog.cogReference) | 0xFCU;

    if (cogSog.hasCog) {
        const double rad = cogSog.cogDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(frame.data.data() + 2, raw);
    }
    if (cogSog.hasSog) {
        const double mps = cogSog.sogKnots * kKnotsToMps;
        const auto raw = static_cast<std::uint16_t>(std::round(mps / 0.01));
        writeU16LE(frame.data.data() + 4, raw);
    }

    return frame;
}

CanFrame N2kEncoder::encodeVesselHeading(const VesselHeading& hdg, std::uint8_t srcAddr, std::uint8_t priority) noexcept
{
    N2kHeader hdr {};
    hdr.priority = priority & 0x07U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::VesselHeading);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    frame.data.fill(0xFFU);

    frame.data[0] = hdg.sid;

    if (hdg.hasHeading) {
        const double rad = hdg.headingDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(frame.data.data() + 1, raw);
    }
    if (hdg.hasDeviation) {
        const double rad = hdg.deviationDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(frame.data.data() + 3, raw);
    } else {
        writeI16LE(frame.data.data() + 3, Sentinels::kUnavailableInt16);
    }
    if (hdg.hasVariation) {
        const double rad = hdg.variationDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(frame.data.data() + 5, raw);
    } else {
        writeI16LE(frame.data.data() + 5, Sentinels::kUnavailableInt16);
    }
    frame.data[7] = static_cast<std::uint8_t>(hdg.reference) | 0xFCU;

    return frame;
}

CanFrame N2kEncoder::encodeAttitude(const Attitude& att, std::uint8_t srcAddr, std::uint8_t priority) noexcept
{
    N2kHeader hdr {};
    hdr.priority = priority & 0x07U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::Attitude);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    frame.data.fill(0xFFU);

    frame.data[0] = att.sid;

    if (att.hasYaw) {
        const double rad = att.yawDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(frame.data.data() + 1, raw);
    } else {
        writeI16LE(frame.data.data() + 1, Sentinels::kUnavailableInt16);
    }
    if (att.hasPitch) {
        const double rad = att.pitchDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(frame.data.data() + 3, raw);
    } else {
        writeI16LE(frame.data.data() + 3, Sentinels::kUnavailableInt16);
    }
    if (att.hasRoll) {
        const double rad = att.rollDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(frame.data.data() + 5, raw);
    } else {
        writeI16LE(frame.data.data() + 5, Sentinels::kUnavailableInt16);
    }
    frame.data[7] = 0xFFU;

    return frame;
}

CanFrame N2kEncoder::encodeWindData(const WindData& wind, std::uint8_t srcAddr, std::uint8_t priority) noexcept
{
    N2kHeader hdr {};
    hdr.priority = priority & 0x07U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::WindData);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    frame.data.fill(0xFFU);

    frame.data[0] = wind.sid;

    if (wind.hasWindSpeed) {
        const auto raw = static_cast<std::uint16_t>(std::round(wind.windSpeedMps / 0.01));
        writeU16LE(frame.data.data() + 1, raw);
    }
    if (wind.hasWindAngle) {
        const double rad = wind.windAngleDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(frame.data.data() + 3, raw);
    }
    frame.data[5] = static_cast<std::uint8_t>(wind.reference) | 0xF8U;

    return frame;
}

std::vector<std::uint8_t> N2kEncoder::encodePgn129038Payload(const AisClassAPosition& ais)
{
    std::vector<std::uint8_t> payload(28U, 0xFFU);

    payload[0] = (ais.messageId != 0U) ? ais.messageId : 1U;
    payload[1] = static_cast<std::uint8_t>((ais.repeatIndicator & 0x03U) | ((ais.navStatus & 0x0FU) << 2) | 0xC0U);

    writeU32LE(payload.data() + 2, ais.mmsi);

    if (ais.positionValid) {
        const auto rawLon = static_cast<std::int32_t>(std::round(ais.longitudeDeg * 1e7));
        const auto rawLat = static_cast<std::int32_t>(std::round(ais.latitudeDeg * 1e7));
        writeI32LE(payload.data() + 6, rawLon);
        writeI32LE(payload.data() + 10, rawLat);
    } else {
        writeI32LE(payload.data() + 6, Sentinels::kUnavailableInt32);
        writeI32LE(payload.data() + 10, Sentinels::kUnavailableInt32);
    }

    payload[14] = 0xFFU; // accuracy / raim flags

    if (ais.hasCog) {
        const double rad = ais.cogDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(payload.data() + 15, raw);
    }
    if (ais.hasSog) {
        const double mps = ais.sogKnots * kKnotsToMps;
        const auto raw = static_cast<std::uint16_t>(std::round(mps / 0.01));
        writeU16LE(payload.data() + 17, raw);
    }

    payload[19] = 0xFFU;

    if (ais.hasHeading) {
        const double rad = ais.headingDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(payload.data() + 20, raw);
    }
    if (ais.hasRateOfTurn) {
        const double radPerSec = ais.rateOfTurnDegPerSec * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(radPerSec / 0.0001));
        writeI16LE(payload.data() + 22, raw);
    } else {
        writeI16LE(payload.data() + 22, Sentinels::kUnavailableInt16);
    }

    return payload;
}

std::vector<CanFrame> N2kEncoder::encodeAisClassAPosition(
    const AisClassAPosition& ais, std::uint8_t srcAddr, std::uint8_t seqCounter)
{
    const auto payload = encodePgn129038Payload(ais);
    N2kHeader hdr {};
    hdr.priority = 4U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassAPositionReport);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    return N2kDecoder::splitFastPacket(hdr, payload.data(), payload.size(), seqCounter);
}

std::vector<std::uint8_t> N2kEncoder::encodePgn129039Payload(const AisClassBPosition& ais)
{
    std::vector<std::uint8_t> payload(26U, 0xFFU);

    payload[0] = 18U; // AIS Class B standard position report
    payload[1] = 0xFCU;

    writeU32LE(payload.data() + 2, ais.mmsi);

    if (ais.positionValid) {
        const auto rawLon = static_cast<std::int32_t>(std::round(ais.longitudeDeg * 1e7));
        const auto rawLat = static_cast<std::int32_t>(std::round(ais.latitudeDeg * 1e7));
        writeI32LE(payload.data() + 6, rawLon);
        writeI32LE(payload.data() + 10, rawLat);
    } else {
        writeI32LE(payload.data() + 6, Sentinels::kUnavailableInt32);
        writeI32LE(payload.data() + 10, Sentinels::kUnavailableInt32);
    }

    payload[14] = 0xFFU;

    if (ais.hasCog) {
        const double rad = ais.cogDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(payload.data() + 15, raw);
    }
    if (ais.hasSog) {
        const double mps = ais.sogKnots * kKnotsToMps;
        const auto raw = static_cast<std::uint16_t>(std::round(mps / 0.01));
        writeU16LE(payload.data() + 17, raw);
    }

    payload[19] = 0xFFU;
    payload[20] = 0xFFU;

    if (ais.hasHeading) {
        const double rad = ais.headingDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(payload.data() + 21, raw);
    }

    return payload;
}

std::vector<CanFrame> N2kEncoder::encodeAisClassBPosition(
    const AisClassBPosition& ais, std::uint8_t srcAddr, std::uint8_t seqCounter)
{
    const auto payload = encodePgn129039Payload(ais);
    N2kHeader hdr {};
    hdr.priority = 4U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassBPositionReport);
    hdr.sourceAddress = srcAddr;
    hdr.destinationAddress = 0xFFU;

    return N2kDecoder::splitFastPacket(hdr, payload.data(), payload.size(), seqCounter);
}

} // namespace Nmea::N2k
