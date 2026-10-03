/// @file SightlineKlvParser.cpp
/// @brief Implementation of Sightline KLV Metadata response deserializer.

#include "SightlineKlvParser.h"

namespace Sightline {

bool SightlineKlvParser::parseMetadataValues(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    const auto payload { SightlineFraming::extractPayload(packet) };

    if (msgId == MessageId::SetMetadataValues) {
        if (payload.size() < 44U) {
            return false;
        }
        out.validDataMask = SightlineFraming::readU16Le(payload.data());
        out.utcTime = SightlineFraming::readU64Le(payload.data() + 2U);
        out.heading = SightlineFraming::readU16Le(payload.data() + 10U);
        out.pitch = SightlineFraming::readS16Le(payload.data() + 12U);
        out.roll = SightlineFraming::readS16Le(payload.data() + 14U);
        out.lat = SightlineFraming::readS32Le(payload.data() + 16U);
        out.lon = SightlineFraming::readS32Le(payload.data() + 20U);
        out.alt = SightlineFraming::readU16Le(payload.data() + 24U);
        out.hfov = SightlineFraming::readU16Le(payload.data() + 26U);
        out.vfov = SightlineFraming::readU16Le(payload.data() + 28U);
        out.az = SightlineFraming::readU32Le(payload.data() + 30U);
        out.el = SightlineFraming::readS32Le(payload.data() + 34U);
        out.sensorRoll = SightlineFraming::readU32Le(payload.data() + 38U);
        out.displayId = SightlineFraming::readU16Le(payload.data() + 42U);
        return true;
    }

    if (msgId == MessageId::CurrentMetadataValues) {
        if (payload.size() < 42U) {
            return false;
        }
        out.validDataMask = 0x0FFFU;
        out.utcTime = SightlineFraming::readU64Le(payload.data());
        out.heading = SightlineFraming::readU16Le(payload.data() + 8U);
        out.pitch = SightlineFraming::readS16Le(payload.data() + 10U);
        out.roll = SightlineFraming::readS16Le(payload.data() + 12U);
        out.lat = SightlineFraming::readS32Le(payload.data() + 14U);
        out.lon = SightlineFraming::readS32Le(payload.data() + 18U);
        out.alt = SightlineFraming::readU16Le(payload.data() + 22U);
        out.hfov = SightlineFraming::readU16Le(payload.data() + 24U);
        out.vfov = SightlineFraming::readU16Le(payload.data() + 26U);
        out.az = SightlineFraming::readU32Le(payload.data() + 28U);
        out.el = SightlineFraming::readS32Le(payload.data() + 32U);
        out.sensorRoll = SightlineFraming::readU32Le(payload.data() + 36U);
        out.displayId = SightlineFraming::readU16Le(payload.data() + 40U);
        return true;
    }

    return false;
}

bool SightlineKlvParser::parseCurrentValues(
    const std::vector<std::uint8_t>& packet, MsgCurrentMetadataValues& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    const auto payload { SightlineFraming::extractPayload(packet) };

    std::size_t offset { 0U };
    if (msgId == MessageId::CurrentMetadataValues) {
        if (payload.size() < 42U) {
            return false;
        }
        offset = 0U;
    } else if (msgId == MessageId::SetMetadataValues) {
        if (payload.size() < 44U) {
            return false;
        }
        offset = 2U; // Skip validDataMask
    } else {
        return false;
    }

    out.utcTime = SightlineFraming::readU64Le(payload.data() + offset);
    out.heading = SightlineFraming::readU16Le(payload.data() + offset + 8U);
    out.pitch = SightlineFraming::readS16Le(payload.data() + offset + 10U);
    out.roll = SightlineFraming::readS16Le(payload.data() + offset + 12U);
    out.lat = SightlineFraming::readS32Le(payload.data() + offset + 14U);
    out.lon = SightlineFraming::readS32Le(payload.data() + offset + 18U);
    out.alt = SightlineFraming::readU16Le(payload.data() + offset + 22U);
    out.hfov = SightlineFraming::readU16Le(payload.data() + offset + 24U);
    out.vfov = SightlineFraming::readU16Le(payload.data() + offset + 26U);
    out.az = SightlineFraming::readU32Le(payload.data() + offset + 28U);
    out.el = SightlineFraming::readS32Le(payload.data() + offset + 32U);
    out.sensorRoll = SightlineFraming::readU32Le(payload.data() + offset + 36U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + offset + 40U);
    return true;
}

bool SightlineKlvParser::parseMetadataStaticValues(
    const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::MetadataStaticValues) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.type = static_cast<StaticMetadataType>(payload[0U]);
    const std::size_t len { static_cast<std::size_t>(payload[1U]) };
    if (payload.size() < 2U + len) {
        return false;
    }

    out.value.assign(payload.begin() + 2U, payload.begin() + 2U + len);
    if (payload.size() >= 2U + len + 2U) {
        out.displayId = SightlineFraming::readU16Le(payload.data() + 2U + len);
    } else {
        out.displayId = 0x0002U;
    }
    return true;
}

bool SightlineKlvParser::parseFrameValues(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataFrameValues& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    const auto payload { SightlineFraming::extractPayload(packet) };

    if (msgId == MessageId::SetMetadataFrameValues) {
        if (payload.size() < 49U) {
            return false;
        }
        out.validDataMask = SightlineFraming::readU16Le(payload.data());
        out.frameCenterLat = SightlineFraming::readS32Le(payload.data() + 2U);
        out.frameCenterLon = SightlineFraming::readS32Le(payload.data() + 6U);
        out.frameCenterEl = SightlineFraming::readU16Le(payload.data() + 10U);
        out.frameWidth = SightlineFraming::readU16Le(payload.data() + 12U);
        out.slantRange = SightlineFraming::readU32Le(payload.data() + 14U);
        out.userSuppliedFlags = payload[18U];
        out.targetLat = SightlineFraming::readS32Le(payload.data() + 19U);
        out.targetLon = SightlineFraming::readS32Le(payload.data() + 23U);
        out.targetEl = SightlineFraming::readU16Le(payload.data() + 27U);
        out.targetTrackGateHeight = payload[29U];
        out.targetTrackGateWidth = payload[30U];
        out.offsetCornerLat1 = SightlineFraming::readS16Le(payload.data() + 31U);
        out.offsetCornerLon1 = SightlineFraming::readS16Le(payload.data() + 33U);
        out.offsetCornerLat2 = SightlineFraming::readS16Le(payload.data() + 35U);
        out.offsetCornerLon2 = SightlineFraming::readS16Le(payload.data() + 37U);
        out.offsetCornerLat3 = SightlineFraming::readS16Le(payload.data() + 39U);
        out.offsetCornerLon3 = SightlineFraming::readS16Le(payload.data() + 41U);
        out.offsetCornerLat4 = SightlineFraming::readS16Le(payload.data() + 43U);
        out.offsetCornerLon4 = SightlineFraming::readS16Le(payload.data() + 45U);
        out.displayId = SightlineFraming::readU16Le(payload.data() + 47U);
        return true;
    }

    if (msgId == MessageId::CurrentMetadataFrameValues) {
        if (payload.size() < 47U) {
            return false;
        }
        out.validDataMask = 0x001FU;
        out.frameCenterLat = SightlineFraming::readS32Le(payload.data());
        out.frameCenterLon = SightlineFraming::readS32Le(payload.data() + 4U);
        out.frameCenterEl = SightlineFraming::readU16Le(payload.data() + 8U);
        out.frameWidth = SightlineFraming::readU16Le(payload.data() + 10U);
        out.slantRange = SightlineFraming::readU32Le(payload.data() + 12U);
        out.userSuppliedFlags = payload[16U];
        out.targetLat = SightlineFraming::readS32Le(payload.data() + 17U);
        out.targetLon = SightlineFraming::readS32Le(payload.data() + 21U);
        out.targetEl = SightlineFraming::readU16Le(payload.data() + 25U);
        out.targetTrackGateHeight = payload[27U];
        out.targetTrackGateWidth = payload[28U];
        out.offsetCornerLat1 = SightlineFraming::readS16Le(payload.data() + 29U);
        out.offsetCornerLon1 = SightlineFraming::readS16Le(payload.data() + 31U);
        out.offsetCornerLat2 = SightlineFraming::readS16Le(payload.data() + 33U);
        out.offsetCornerLon2 = SightlineFraming::readS16Le(payload.data() + 35U);
        out.offsetCornerLat3 = SightlineFraming::readS16Le(payload.data() + 37U);
        out.offsetCornerLon3 = SightlineFraming::readS16Le(payload.data() + 39U);
        out.offsetCornerLat4 = SightlineFraming::readS16Le(payload.data() + 41U);
        out.offsetCornerLon4 = SightlineFraming::readS16Le(payload.data() + 43U);
        out.displayId = SightlineFraming::readU16Le(payload.data() + 45U);
        return true;
    }

    return false;
}

bool SightlineKlvParser::parseCurrentFrameValues(
    const std::vector<std::uint8_t>& packet, MsgCurrentMetadataFrameValues& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    const auto payload { SightlineFraming::extractPayload(packet) };

    std::size_t offset { 0U };
    if (msgId == MessageId::CurrentMetadataFrameValues) {
        if (payload.size() < 47U) {
            return false;
        }
        offset = 0U;
    } else if (msgId == MessageId::SetMetadataFrameValues) {
        if (payload.size() < 49U) {
            return false;
        }
        offset = 2U; // Skip validDataMask
    } else {
        return false;
    }

    out.frameCenterLat = SightlineFraming::readS32Le(payload.data() + offset);
    out.frameCenterLon = SightlineFraming::readS32Le(payload.data() + offset + 4U);
    out.frameCenterEl = SightlineFraming::readU16Le(payload.data() + offset + 8U);
    out.frameWidth = SightlineFraming::readU16Le(payload.data() + offset + 10U);
    out.slantRange = SightlineFraming::readU32Le(payload.data() + offset + 12U);
    out.userSuppliedFlags = payload[offset + 16U];
    out.targetLat = SightlineFraming::readS32Le(payload.data() + offset + 17U);
    out.targetLon = SightlineFraming::readS32Le(payload.data() + offset + 21U);
    out.targetEl = SightlineFraming::readU16Le(payload.data() + offset + 25U);
    out.targetTrackGateHeight = payload[offset + 27U];
    out.targetTrackGateWidth = payload[offset + 28U];
    out.offsetCornerLat1 = SightlineFraming::readS16Le(payload.data() + offset + 29U);
    out.offsetCornerLon1 = SightlineFraming::readS16Le(payload.data() + offset + 31U);
    out.offsetCornerLat2 = SightlineFraming::readS16Le(payload.data() + offset + 33U);
    out.offsetCornerLon2 = SightlineFraming::readS16Le(payload.data() + offset + 35U);
    out.offsetCornerLat3 = SightlineFraming::readS16Le(payload.data() + offset + 37U);
    out.offsetCornerLon3 = SightlineFraming::readS16Le(payload.data() + offset + 39U);
    out.offsetCornerLat4 = SightlineFraming::readS16Le(payload.data() + offset + 41U);
    out.offsetCornerLon4 = SightlineFraming::readS16Le(payload.data() + offset + 43U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + offset + 45U);
    return true;
}

bool SightlineKlvParser::parseMetadataRate(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    if (msgId != MessageId::SetMetadataRate) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 9U) {
        return false;
    }

    out.enables = SightlineFraming::readU64Le(payload.data());
    out.frameStep = payload[8U];
    if (payload.size() >= 11U) {
        out.displayId = SightlineFraming::readU16Le(payload.data() + 9U);
    } else {
        out.displayId = 0x0002U;
    }
    return true;
}

bool SightlineKlvParser::parseCurrentRate(
    const std::vector<std::uint8_t>& packet, MsgCurrentMetadataRate& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentMetadataRate) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.index = payload[0U];
    out.frameStep = payload[1U];
    out.displayId = SightlineFraming::readU16Le(payload.data() + 2U);
    return true;
}

bool SightlineKlvParser::parseTagData(
    const std::vector<std::uint8_t>& packet, MsgTagData& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TagData) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 9U) {
        return false;
    }

    out.reserved1 = payload[0U];
    out.reserved2 = payload[1U];
    out.tagId = payload[2U];
    out.tagSubId = payload[3U];
    out.reservedInternal = SightlineFraming::readU16Le(payload.data() + 4U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + 6U);
    const std::size_t len { static_cast<std::size_t>(payload[8U]) };
    if (payload.size() >= 9U + len) {
        out.data.assign(payload.begin() + 9U, payload.begin() + 9U + len);
    } else {
        out.data.assign(payload.begin() + 9U, payload.end());
    }
    return true;
}

bool SightlineKlvParser::parseTagDataRate(
    const std::vector<std::uint8_t>& packet, MsgTagDataRate& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TagDataRate) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 9U) {
        return false;
    }

    out.reserved1 = payload[0U];
    out.reserved2 = payload[1U];
    out.mode = payload[2U];
    out.tagId1 = payload[3U];
    out.tagId2 = payload[4U];
    out.frameStep = SightlineFraming::readU16Le(payload.data() + 5U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + 7U);
    return true;
}

bool SightlineKlvParser::parseTagSourceSelector(
    const std::vector<std::uint8_t>& packet, MsgTagSourceSelector& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TagSourceSelector) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 9U) {
        return false;
    }

    out.reserved1 = payload[0U];
    out.reserved2 = payload[1U];
    out.mode = payload[2U];
    out.tagId1 = payload[3U];
    out.tagId2 = payload[4U];
    out.selector = SightlineFraming::readU16Le(payload.data() + 5U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + 7U);
    return true;
}

bool SightlineKlvParser::parseCursorOnTarget(
    const std::vector<std::uint8_t>& packet, MsgCursorOnTarget& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CursorOnTarget) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 12U) {
        return false;
    }

    out.mode = SightlineFraming::readU16Le(payload.data());
    out.ipAddress = (static_cast<std::uint32_t>(payload[2U]) << 24U) |
                    (static_cast<std::uint32_t>(payload[3U]) << 16U) |
                    (static_cast<std::uint32_t>(payload[4U]) << 8U) |
                    static_cast<std::uint32_t>(payload[5U]);
    out.port = static_cast<std::uint16_t>((static_cast<std::uint16_t>(payload[6U]) << 8U) |
                                          static_cast<std::uint16_t>(payload[7U]));
    out.rate = SightlineFraming::readU16Le(payload.data() + 8U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + 10U);
    return true;
}

bool SightlineKlvParser::parseVmtiChips(
    const std::vector<std::uint8_t>& packet, MsgVmtiChips& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::VMTIChips) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.mode = payload[0U];
    out.format = static_cast<VmtiChipFormat>(payload[1U]);
    out.sizeType = static_cast<VmtiChipSizeType>(payload[2U]);
    out.sizeHint = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxPerFrame = payload[5U];
    out.minFramesBetween = payload[6U];
    out.reserved0 = payload[7U];
    if (payload.size() >= 10U) {
        out.displayId = SightlineFraming::readU16Le(payload.data() + 8U);
    } else {
        out.displayId = 0x0002U;
    }
    return true;
}

bool SightlineKlvParser::parseVmtiFields(
    const std::vector<std::uint8_t>& packet, MsgVmtiFields& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::VMTIFields) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.displayId = SightlineFraming::readU16Le(payload.data());
    out.fields = SightlineFraming::readU16Le(payload.data() + 2U);
    out.ontologySeriesRate = SightlineFraming::readU16Le(payload.data() + 4U);
    return true;
}

bool SightlineKlvParser::parseAncillaryText(
    const std::vector<std::uint8_t>& packet, MsgAncillaryTextMetadata& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::AncillaryTextMetadata) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 11U) {
        return false;
    }

    out.creationTime = SightlineFraming::readU64Le(payload.data());
    std::size_t offset { 8U };

    auto readLenString = [&](std::string& str) -> bool {
        if (offset >= payload.size()) {
            return false;
        }
        const std::size_t len = payload[offset++];
        if (offset + len > payload.size()) {
            return false;
        }
        str.clear();
        str.reserve(len);
        for (std::size_t i { 0U }; i < len; ++i) {
            str.push_back(static_cast<char>(payload[offset++]));
        }
        return true;
    };

    if (!readLenString(out.source) ||
        !readLenString(out.originator) ||
        !readLenString(out.messageBody)) {
        return false;
    }

    if (offset + 2U <= payload.size()) {
        out.displayId = SightlineFraming::readU16Le(payload.data() + offset);
    } else {
        out.displayId = 0x0002U;
    }
    return true;
}

bool SightlineKlvParser::parseAppendedMetadata(
    const std::vector<std::uint8_t>& packet, MsgAppendedMetadata& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::AppendedMetadata) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.displayId = SightlineFraming::readU16Le(payload.data());
    out.data.assign(payload.begin() + 2U, payload.end());
    return true;
}

} // namespace Sightline
