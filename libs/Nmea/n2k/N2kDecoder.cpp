#include "N2kDecoder.h"

#include <cmath>
#include <cstring>

namespace Nmea::N2k {

namespace {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kRadToDeg = 180.0 / kPi;
    constexpr double kDegToRad = kPi / 180.0;
    constexpr double kMpsToKnots = 1.9438444924406;
    constexpr double kKnotsToMps = 1.0 / kMpsToKnots;

    inline std::uint16_t readU16LE(const std::uint8_t* p) noexcept
    {
        return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
    }

    inline std::int16_t readI16LE(const std::uint8_t* p) noexcept
    {
        return static_cast<std::int16_t>(readU16LE(p));
    }

    inline std::uint32_t readU32LE(const std::uint8_t* p) noexcept
    {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
            | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }

    inline std::int32_t readI32LE(const std::uint8_t* p) noexcept
    {
        return static_cast<std::int32_t>(readU32LE(p));
    }

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

bool N2kDecoder::parsePgn129025(const std::uint8_t* data, std::size_t len, PositionRapid& out) noexcept
{
    if (data == nullptr || len < 8U) {
        return false;
    }

    const std::int32_t rawLat = readI32LE(data);
    const std::int32_t rawLon = readI32LE(data + 4);

    if (rawLat == Sentinels::kUnavailableInt32 || rawLon == Sentinels::kUnavailableInt32) {
        out.isValid = false;
        out.latitudeDeg = 0.0;
        out.longitudeDeg = 0.0;
        return true;
    }

    out.latitudeDeg = static_cast<double>(rawLat) * 1e-7;
    out.longitudeDeg = static_cast<double>(rawLon) * 1e-7;
    out.isValid = (out.latitudeDeg >= -90.0 && out.latitudeDeg <= 90.0 && out.longitudeDeg >= -180.0
        && out.longitudeDeg <= 180.0);
    return true;
}

bool N2kDecoder::parsePgn129026(const std::uint8_t* data, std::size_t len, CogSogRapid& out) noexcept
{
    if (data == nullptr || len < 8U) {
        return false;
    }

    out.sid = data[0];
    const std::uint8_t refBits = data[1] & 0x03U;
    out.cogReference = static_cast<HeadingReference>(refBits);

    const std::uint16_t rawCog = readU16LE(data + 2);
    if (rawCog != Sentinels::kUnavailableUInt16) {
        out.cogDegrees = static_cast<double>(rawCog) * 0.0001 * kRadToDeg;
        if (out.cogDegrees >= 360.0) {
            out.cogDegrees = std::fmod(out.cogDegrees, 360.0);
        }
        out.hasCog = true;
    } else {
        out.cogDegrees = 0.0;
        out.hasCog = false;
    }

    const std::uint16_t rawSog = readU16LE(data + 4);
    if (rawSog != Sentinels::kUnavailableUInt16) {
        const double sogMps = static_cast<double>(rawSog) * 0.01;
        out.sogKnots = sogMps * kMpsToKnots;
        out.hasSog = true;
    } else {
        out.sogKnots = 0.0;
        out.hasSog = false;
    }

    return true;
}

bool N2kDecoder::parsePgn127250(const std::uint8_t* data, std::size_t len, VesselHeading& out) noexcept
{
    if (data == nullptr || len < 8U) {
        return false;
    }

    out.sid = data[0];

    const std::uint16_t rawHeading = readU16LE(data + 1);
    if (rawHeading != Sentinels::kUnavailableUInt16) {
        out.headingDegrees = static_cast<double>(rawHeading) * 0.0001 * kRadToDeg;
        if (out.headingDegrees >= 360.0) {
            out.headingDegrees = std::fmod(out.headingDegrees, 360.0);
        }
        out.hasHeading = true;
    } else {
        out.headingDegrees = 0.0;
        out.hasHeading = false;
    }

    const std::int16_t rawDev = readI16LE(data + 3);
    if (rawDev != Sentinels::kUnavailableInt16) {
        out.deviationDegrees = static_cast<double>(rawDev) * 0.0001 * kRadToDeg;
        out.hasDeviation = true;
    } else {
        out.deviationDegrees = 0.0;
        out.hasDeviation = false;
    }

    const std::int16_t rawVar = readI16LE(data + 5);
    if (rawVar != Sentinels::kUnavailableInt16) {
        out.variationDegrees = static_cast<double>(rawVar) * 0.0001 * kRadToDeg;
        out.hasVariation = true;
    } else {
        out.variationDegrees = 0.0;
        out.hasVariation = false;
    }

    const std::uint8_t refBits = data[7] & 0x03U;
    out.reference = static_cast<HeadingReference>(refBits);
    return true;
}

bool N2kDecoder::parsePgn127257(const std::uint8_t* data, std::size_t len, Attitude& out) noexcept
{
    if (data == nullptr || len < 7U) {
        return false;
    }

    out.sid = data[0];

    const std::int16_t rawYaw = readI16LE(data + 1);
    if (rawYaw != Sentinels::kUnavailableInt16) {
        out.yawDegrees = static_cast<double>(rawYaw) * 0.0001 * kRadToDeg;
        out.hasYaw = true;
    } else {
        out.yawDegrees = 0.0;
        out.hasYaw = false;
    }

    const std::int16_t rawPitch = readI16LE(data + 3);
    if (rawPitch != Sentinels::kUnavailableInt16) {
        out.pitchDegrees = static_cast<double>(rawPitch) * 0.0001 * kRadToDeg;
        out.hasPitch = true;
    } else {
        out.pitchDegrees = 0.0;
        out.hasPitch = false;
    }

    const std::int16_t rawRoll = readI16LE(data + 5);
    if (rawRoll != Sentinels::kUnavailableInt16) {
        out.rollDegrees = static_cast<double>(rawRoll) * 0.0001 * kRadToDeg;
        out.hasRoll = true;
    } else {
        out.rollDegrees = 0.0;
        out.hasRoll = false;
    }

    return true;
}

bool N2kDecoder::parsePgn129038(const std::uint8_t* data, std::size_t len, AisClassAPosition& out) noexcept
{
    if (data == nullptr || len < 22U) {
        return false;
    }

    out.messageId = data[0];
    out.repeatIndicator = data[1] & 0x03U;
    out.navStatus = (data[1] >> 2) & 0x0FU;

    out.mmsi = readU32LE(data + 2);

    const std::int32_t rawLon = readI32LE(data + 6);
    const std::int32_t rawLat = readI32LE(data + 10);

    if (rawLon != Sentinels::kUnavailableInt32 && rawLat != Sentinels::kUnavailableInt32) {
        out.longitudeDeg = static_cast<double>(rawLon) * 1e-7;
        out.latitudeDeg = static_cast<double>(rawLat) * 1e-7;
        out.positionValid = true;
    } else {
        out.longitudeDeg = 0.0;
        out.latitudeDeg = 0.0;
        out.positionValid = false;
    }

    const std::uint16_t rawCog = readU16LE(data + 15);
    if (rawCog != Sentinels::kUnavailableUInt16) {
        out.cogDegrees = static_cast<double>(rawCog) * 0.0001 * kRadToDeg;
        out.hasCog = true;
    } else {
        out.cogDegrees = 0.0;
        out.hasCog = false;
    }

    const std::uint16_t rawSog = readU16LE(data + 17);
    if (rawSog != Sentinels::kUnavailableUInt16) {
        out.sogKnots = (static_cast<double>(rawSog) * 0.01) * kMpsToKnots;
        out.hasSog = true;
    } else {
        out.sogKnots = 0.0;
        out.hasSog = false;
    }

    const std::uint16_t rawHeading = readU16LE(data + 20);
    if (rawHeading != Sentinels::kUnavailableUInt16) {
        out.headingDegrees = static_cast<double>(rawHeading) * 0.0001 * kRadToDeg;
        out.hasHeading = true;
    } else {
        out.headingDegrees = 0.0;
        out.hasHeading = false;
    }

    if (len >= 24U) {
        const std::int16_t rawRot = readI16LE(data + 22);
        if (rawRot != Sentinels::kUnavailableInt16) {
            out.rateOfTurnDegPerSec = static_cast<double>(rawRot) * 0.0001 * kRadToDeg;
            out.hasRateOfTurn = true;
        } else {
            out.rateOfTurnDegPerSec = 0.0;
            out.hasRateOfTurn = false;
        }
    }

    return true;
}

bool N2kDecoder::parsePgn129039(const std::uint8_t* data, std::size_t len, AisClassBPosition& out) noexcept
{
    if (data == nullptr || len < 23U) {
        return false;
    }

    out.mmsi = readU32LE(data + 2);

    const std::int32_t rawLon = readI32LE(data + 6);
    const std::int32_t rawLat = readI32LE(data + 10);

    if (rawLon != Sentinels::kUnavailableInt32 && rawLat != Sentinels::kUnavailableInt32) {
        out.longitudeDeg = static_cast<double>(rawLon) * 1e-7;
        out.latitudeDeg = static_cast<double>(rawLat) * 1e-7;
        out.positionValid = true;
    } else {
        out.longitudeDeg = 0.0;
        out.latitudeDeg = 0.0;
        out.positionValid = false;
    }

    const std::uint16_t rawCog = readU16LE(data + 15);
    if (rawCog != Sentinels::kUnavailableUInt16) {
        out.cogDegrees = static_cast<double>(rawCog) * 0.0001 * kRadToDeg;
        out.hasCog = true;
    } else {
        out.cogDegrees = 0.0;
        out.hasCog = false;
    }

    const std::uint16_t rawSog = readU16LE(data + 17);
    if (rawSog != Sentinels::kUnavailableUInt16) {
        out.sogKnots = (static_cast<double>(rawSog) * 0.01) * kMpsToKnots;
        out.hasSog = true;
    } else {
        out.sogKnots = 0.0;
        out.hasSog = false;
    }

    const std::uint16_t rawHeading = readU16LE(data + 21);
    if (rawHeading != Sentinels::kUnavailableUInt16) {
        out.headingDegrees = static_cast<double>(rawHeading) * 0.0001 * kRadToDeg;
        out.hasHeading = true;
    } else {
        out.headingDegrees = 0.0;
        out.hasHeading = false;
    }

    return true;
}

bool N2kDecoder::parsePgn130306(const std::uint8_t* data, std::size_t len, WindData& out) noexcept
{
    if (data == nullptr || len < 6U) {
        return false;
    }

    out.sid = data[0];

    const std::uint16_t rawSpeed = readU16LE(data + 1);
    if (rawSpeed != Sentinels::kUnavailableUInt16) {
        out.windSpeedMps = static_cast<double>(rawSpeed) * 0.01;
        out.windSpeedKnots = out.windSpeedMps * kMpsToKnots;
        out.hasWindSpeed = true;
    } else {
        out.windSpeedMps = 0.0;
        out.windSpeedKnots = 0.0;
        out.hasWindSpeed = false;
    }

    const std::uint16_t rawAngle = readU16LE(data + 3);
    if (rawAngle != Sentinels::kUnavailableUInt16) {
        out.windAngleDegrees = static_cast<double>(rawAngle) * 0.0001 * kRadToDeg;
        if (out.windAngleDegrees >= 360.0) {
            out.windAngleDegrees = std::fmod(out.windAngleDegrees, 360.0);
        }
        out.hasWindAngle = true;
    } else {
        out.windAngleDegrees = 0.0;
        out.hasWindAngle = false;
    }

    const std::uint8_t refBits = data[5] & 0x07U;
    out.reference = static_cast<WindReference>(refBits);
    return true;
}

std::vector<std::uint8_t> N2kDecoder::encodePgn129025(const PositionRapid& pos)
{
    std::vector<std::uint8_t> buf(8U, 0xFFU);
    if (pos.isValid) {
        const auto rawLat = static_cast<std::int32_t>(std::round(pos.latitudeDeg * 1e7));
        const auto rawLon = static_cast<std::int32_t>(std::round(pos.longitudeDeg * 1e7));
        writeI32LE(buf.data(), rawLat);
        writeI32LE(buf.data() + 4, rawLon);
    } else {
        writeI32LE(buf.data(), Sentinels::kUnavailableInt32);
        writeI32LE(buf.data() + 4, Sentinels::kUnavailableInt32);
    }
    return buf;
}

std::vector<std::uint8_t> N2kDecoder::encodePgn129026(const CogSogRapid& cogSog)
{
    std::vector<std::uint8_t> buf(8U, 0xFFU);
    buf[0] = cogSog.sid;
    buf[1] = static_cast<std::uint8_t>(cogSog.cogReference) | 0xFCU;

    if (cogSog.hasCog) {
        const double rad = cogSog.cogDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(buf.data() + 2, raw);
    }
    if (cogSog.hasSog) {
        const double mps = cogSog.sogKnots * kKnotsToMps;
        const auto raw = static_cast<std::uint16_t>(std::round(mps / 0.01));
        writeU16LE(buf.data() + 4, raw);
    }
    return buf;
}

std::vector<std::uint8_t> N2kDecoder::encodePgn127250(const VesselHeading& hdg)
{
    std::vector<std::uint8_t> buf(8U, 0xFFU);
    buf[0] = hdg.sid;

    if (hdg.hasHeading) {
        const double rad = hdg.headingDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(buf.data() + 1, raw);
    }
    if (hdg.hasDeviation) {
        const double rad = hdg.deviationDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(buf.data() + 3, raw);
    }
    if (hdg.hasVariation) {
        const double rad = hdg.variationDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(buf.data() + 5, raw);
    }
    buf[7] = static_cast<std::uint8_t>(hdg.reference) | 0xFCU;
    return buf;
}

std::vector<std::uint8_t> N2kDecoder::encodePgn127257(const Attitude& att)
{
    std::vector<std::uint8_t> buf(8U, 0xFFU);
    buf[0] = att.sid;

    if (att.hasYaw) {
        const double rad = att.yawDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(buf.data() + 1, raw);
    }
    if (att.hasPitch) {
        const double rad = att.pitchDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(buf.data() + 3, raw);
    }
    if (att.hasRoll) {
        const double rad = att.rollDegrees * kDegToRad;
        const auto raw = static_cast<std::int16_t>(std::round(rad / 0.0001));
        writeI16LE(buf.data() + 5, raw);
    }
    buf[7] = 0xFFU;
    return buf;
}

std::vector<std::uint8_t> N2kDecoder::encodePgn130306(const WindData& wind)
{
    std::vector<std::uint8_t> buf(6U, 0xFFU);
    buf[0] = wind.sid;

    if (wind.hasWindSpeed) {
        const auto raw = static_cast<std::uint16_t>(std::round(wind.windSpeedMps / 0.01));
        writeU16LE(buf.data() + 1, raw);
    }
    if (wind.hasWindAngle) {
        const double rad = wind.windAngleDegrees * kDegToRad;
        const auto raw = static_cast<std::uint16_t>(std::round(rad / 0.0001));
        writeU16LE(buf.data() + 3, raw);
    }
    buf[5] = static_cast<std::uint8_t>(wind.reference) | 0xF8U;
    return buf;
}

std::vector<CanFrame> N2kDecoder::splitFastPacket(
    const N2kHeader& header, const std::uint8_t* data, std::size_t len, std::uint8_t seqCounter)
{
    std::vector<CanFrame> frames {};
    if (data == nullptr || len == 0U) {
        return frames;
    }

    const std::uint32_t canId = header.toCanId();
    const std::uint8_t seq = seqCounter & 0x1FU;

    // Frame 0:
    // data[0]: (seq << 3) | 0x00
    // data[1]: totalBytes (len)
    // data[2..7]: up to 6 data bytes
    CanFrame f0 {};
    f0.id = canId;
    f0.dlc = 8U;
    f0.data.fill(0xFFU);
    f0.data[0] = static_cast<std::uint8_t>(seq << 3);
    f0.data[1] = static_cast<std::uint8_t>(len & 0xFFU);

    const std::size_t chunk0 = std::min<std::size_t>(len, 6U);
    std::memcpy(&f0.data[2], data, chunk0);
    frames.push_back(f0);

    std::size_t offset = chunk0;
    std::uint8_t frameCounter = 1U;

    while (offset < len) {
        CanFrame fn {};
        fn.id = canId;
        fn.dlc = 8U;
        fn.data.fill(0xFFU);
        fn.data[0] = static_cast<std::uint8_t>((seq << 3) | (frameCounter & 0x07U));

        const std::size_t remaining = len - offset;
        const std::size_t chunkN = std::min<std::size_t>(remaining, 7U);
        std::memcpy(&fn.data[1], data + offset, chunkN);
        frames.push_back(fn);

        offset += chunkN;
        frameCounter = static_cast<std::uint8_t>((frameCounter + 1U) & 0xFFU);
    }

    return frames;
}

} // namespace Nmea::N2k
