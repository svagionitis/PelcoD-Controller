/// @file SightlineNucParser.cpp
/// @brief Implementation of Sightline SLA NUC and dead pixel frame deserializers (IDD v3.11).

#include "SightlineNucParser.h"

namespace Sightline {

namespace {

    /// @brief Minimum 0x35 payload: cameraIndex, nucShow, nucRunMode, numFrames.
    constexpr std::size_t kNucParamsMin { 4U };

    /// @brief Minimum 0x36 payload: cameraIndex, reserved, nucReadWriteMode, fileName.len.
    constexpr std::size_t kReadWriteMin { 4U };

    /// @brief Exact 0xA8 payload size.
    constexpr std::size_t kDeadPixelSize { 8U };

    /// @brief Exact 0xA1 payload size (u8 + s32 + 7 x u32).
    constexpr std::size_t kDeadStatsSize { 33U };

    /// @brief Largest valid raw values of the byte-sized NUC enumerations.
    constexpr std::uint8_t kMaxNucShow { static_cast<std::uint8_t>(NucShow::DeadImage) };
    constexpr std::uint8_t kMaxNucRunMode { static_cast<std::uint8_t>(NucRunMode::Noise3DStats) };
    constexpr std::uint8_t kMaxDeadReplace { static_cast<std::uint8_t>(DeadReplace::Median) };
    constexpr std::uint8_t kMaxFileOp { static_cast<std::uint8_t>(NucFileOp::SaveShutterFlatten) };
    constexpr std::uint8_t kMaxDefaultOp { static_cast<std::uint8_t>(NucDefaultOp::ClearDead) };
    constexpr std::uint8_t kMaxDeadPixelMode { static_cast<std::uint8_t>(DeadPixelMode::DynamicDetect) };

    /// @brief Low / high nibble masks for 0x36 nucReadWriteMode.
    constexpr std::uint8_t kNibbleMask { 0x0FU };
    constexpr std::uint8_t kNibbleShift { 4U };

    /// @class PayloadReader
    /// @brief Bounds-checked little-endian cursor over a payload view.
    /// @details Every read verifies the remaining length first and leaves the output untouched on
    ///          failure, so callers never index past the end of the buffer (CERT CTR50-CPP).
    class PayloadReader {
    public:
        /// @brief Constructs a reader at offset 0.
        /// @param[in] data Payload view; must outlive the reader.
        explicit PayloadReader(ByteView data) noexcept
            : m_data { data }
        {
        }

        /// @brief Number of unread bytes.
        /// @return Remaining byte count.
        [[nodiscard]] std::size_t remaining() const noexcept { return m_data.size() - m_pos; }

        /// @brief Reads one byte.
        /// @param[out] value Destination.
        /// @return False if fewer than 1 byte remain.
        [[nodiscard]] bool readU8(std::uint8_t& value) noexcept
        {
            if (remaining() < 1U) {
                return false;
            }
            value = m_data[m_pos];
            ++m_pos;
            return true;
        }

        /// @brief Reads a little-endian u16.
        /// @param[out] value Destination.
        /// @return False if fewer than 2 bytes remain.
        [[nodiscard]] bool readU16(std::uint16_t& value) noexcept
        {
            if (remaining() < 2U) {
                return false;
            }
            value = SightlineFraming::readU16Le(m_data.data() + m_pos);
            m_pos += 2U;
            return true;
        }

        /// @brief Reads a little-endian s16.
        /// @param[out] value Destination.
        /// @return False if fewer than 2 bytes remain.
        [[nodiscard]] bool readS16(std::int16_t& value) noexcept
        {
            if (remaining() < 2U) {
                return false;
            }
            value = SightlineFraming::readS16Le(m_data.data() + m_pos);
            m_pos += 2U;
            return true;
        }

        /// @brief Reads a little-endian u32.
        /// @param[out] value Destination.
        /// @return False if fewer than 4 bytes remain.
        [[nodiscard]] bool readU32(std::uint32_t& value) noexcept
        {
            if (remaining() < 4U) {
                return false;
            }
            value = SightlineFraming::readU32Le(m_data.data() + m_pos);
            m_pos += 4U;
            return true;
        }

        /// @brief Reads a little-endian s32.
        /// @param[out] value Destination.
        /// @return False if fewer than 4 bytes remain.
        [[nodiscard]] bool readS32(std::int32_t& value) noexcept
        {
            if (remaining() < 4U) {
                return false;
            }
            value = SightlineFraming::readS32Le(m_data.data() + m_pos);
            m_pos += 4U;
            return true;
        }

        /// @brief Reads an SVPLenString_t (u8 length + characters).
        /// @param[out] value Destination string.
        /// @return False if the prefix is missing or the declared length overruns the payload.
        [[nodiscard]] bool readLenString(std::string& value)
        {
            if (remaining() < 1U) {
                return false;
            }
            const std::size_t len { m_data[m_pos] };
            if (remaining() < (1U + len)) {
                return false;
            }
            ++m_pos;
            value.clear();
            value.reserve(len);
            for (std::size_t i { 0U }; i < len; ++i) {
                value.push_back(static_cast<char>(m_data[m_pos + i]));
            }
            m_pos += len;
            return true;
        }

    private:
        ByteView m_data {}; ///< Payload being read
        std::size_t m_pos { 0U }; ///< Current read offset
    };

    /// @brief Extracts the payload of @p packet if it carries message @p id.
    /// @param[in] packet Framed packet.
    /// @param[in] id Expected message ID.
    /// @param[out] payload Payload view on success.
    /// @return True if the IDs match.
    [[nodiscard]] bool payloadFor(ByteView packet, MessageId id, ByteView& payload) noexcept
    {
        if (SightlineFraming::identifyMessage(packet) != id) {
            return false;
        }
        payload = SightlineFraming::extractPayload(packet);
        return true;
    }

    /// @brief Reads the optional 0x35 tail (fields appended across firmware releases).
    /// @details Stops silently at the end of a legacy (truncated) reply, leaving defaults in
    ///          place. Fails only on a value that violates the IDD (bad enum, string overrun).
    /// @param[in,out] rd Reader positioned after numFrames.
    /// @param[in,out] msg Message receiving the decoded fields.
    /// @return False on a malformed field.
    [[nodiscard]] bool readNucTail(PayloadReader& rd, MsgNucParameters& msg)
    {
        if ((rd.remaining() == 0U) || !rd.readU16(msg.minDeadGain) || !rd.readU16(msg.maxDeadGain)
            || !rd.readU16(msg.minDeadVal) || !rd.readU16(msg.maxDeadVal) || !rd.readS32(msg.minDeadOff)
            || !rd.readS32(msg.maxDeadOff) || !rd.readU32(msg.maxStdDevDead) || !rd.readS32(msg.maxNumDead)) {
            return rd.remaining() == 0U;
        }
        std::uint8_t raw { 0U };
        if (!rd.readU8(raw)) {
            return true; // legacy: ends after maxNumDead
        }
        if (raw > kMaxDeadReplace) {
            return false;
        }
        msg.deadReplace = static_cast<DeadReplace>(raw);
        if (!rd.readU8(msg.numReplace) || !rd.readU8(raw)) {
            return rd.remaining() == 0U;
        }
        const bool filterOk { (raw <= static_cast<std::uint8_t>(DeadFilter::NearFar))
            || (raw == static_cast<std::uint8_t>(DeadFilter::Ignore)) };
        if (!filterOk) {
            return false;
        }
        msg.deadFilter = static_cast<DeadFilter>(raw);
        if (!rd.readS16(msg.deadFilterThresh) || !rd.readU8(msg.destripeAmount)
            || !rd.readU8(msg.destripeSections)) {
            return rd.remaining() == 0U;
        }
        if (rd.remaining() == 0U) {
            return true; // pre-3.10 firmware: no nucName
        }
        return rd.readLenString(msg.nucName);
    }

} // namespace

bool SightlineNucParser::parseNucParameters(ByteView packet, MsgNucParameters& out)
{
    ByteView payload {};
    if (!payloadFor(packet, MessageId::NucParameters, payload) || (payload.size() < kNucParamsMin)) {
        return false;
    }
    if ((payload[1U] > kMaxNucShow) || (payload[2U] > kMaxNucRunMode)) {
        return false;
    }

    MsgNucParameters msg {};
    msg.cameraIndex = payload[0U];
    msg.nucShow = static_cast<NucShow>(payload[1U]);
    msg.nucRunMode = static_cast<NucRunMode>(payload[2U]);
    msg.numFrames = payload[3U];

    PayloadReader rd { payload.subspan(kNucParamsMin) };
    if (!readNucTail(rd, msg)) {
        return false;
    }
    out = msg;
    return true;
}

bool SightlineNucParser::parseDeadPixel(ByteView packet, MsgDeadPixel& out)
{
    ByteView payload {};
    if (!payloadFor(packet, MessageId::DeadPixel, payload) || (payload.size() < kDeadPixelSize)) {
        return false;
    }
    if (payload[1U] > kMaxDeadPixelMode) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = static_cast<DeadPixelMode>(payload[1U]);
    out.a = SightlineFraming::readU16Le(payload.data() + 2U);
    out.b = SightlineFraming::readU16Le(payload.data() + 4U);
    out.c = payload[6U];
    out.d = payload[7U];
    return true;
}

bool SightlineNucParser::parseReadWriteNuc(ByteView packet, MsgReadWriteNuc& out)
{
    ByteView payload {};
    if (!payloadFor(packet, MessageId::ReadWriteNuc, payload) || (payload.size() < kReadWriteMin)) {
        return false;
    }
    const std::uint8_t fileOp { static_cast<std::uint8_t>(payload[2U] & kNibbleMask) };
    const std::uint8_t defaultOp { static_cast<std::uint8_t>(payload[2U] >> kNibbleShift) };
    if ((fileOp > kMaxFileOp) || (defaultOp > kMaxDefaultOp)) {
        return false;
    }

    MsgReadWriteNuc msg {};
    msg.cameraIndex = payload[0U];
    msg.fileOp = static_cast<NucFileOp>(fileOp);
    msg.defaultOp = static_cast<NucDefaultOp>(defaultOp);

    PayloadReader rd { payload.subspan(3U) };
    if (!rd.readLenString(msg.fileName)) {
        return false;
    }
    if ((rd.remaining() > 0U) && !rd.readLenString(msg.secondaryFileName)) {
        return false;
    }
    if (rd.remaining() > 0U) {
        static_cast<void>(rd.readU8(msg.interpolationRatio));
    }
    out = msg;
    return true;
}

bool SightlineNucParser::parseUserPalette(ByteView packet, MsgUserPalette& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::SetUserPalette && id != MessageId::CurrentUserPalette) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    if (payload.size() == 768U) {
        out.paletteIndex = 0U;
        out.lutData.assign(payload.data(), payload.data() + payload.size());
    } else {
        out.paletteIndex = payload[0U];
        if (payload.size() > 1U) {
            out.lutData.assign(payload.data() + 1U, payload.data() + payload.size());
        } else {
            out.lutData.clear();
        }
    }
    return true;
}

bool SightlineNucParser::parseDeadPixelStats(ByteView packet, MsgDeadPixelStats& out)
{
    ByteView payload {};
    if (!payloadFor(packet, MessageId::DeadPixelStats, payload) || (payload.size() < kDeadStatsSize)) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.nDead = SightlineFraming::readS32Le(payload.data() + 1U);
    out.nGainLo = SightlineFraming::readU32Le(payload.data() + 5U);
    out.nGainHi = SightlineFraming::readU32Le(payload.data() + 9U);
    out.nAvgLo = SightlineFraming::readU32Le(payload.data() + 13U);
    out.nAvgHi = SightlineFraming::readU32Le(payload.data() + 17U);
    out.nOffLo = SightlineFraming::readU32Le(payload.data() + 21U);
    out.nOffHi = SightlineFraming::readU32Le(payload.data() + 25U);
    out.nDevHi = SightlineFraming::readU32Le(payload.data() + 29U);
    return true;
}

bool SightlineNucParser::parseCameraCalibration(ByteView packet, MsgCameraCalibration& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraCalibration) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 33U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.focalLengthX = SightlineFraming::readFloat32Le(payload.data() + 1U);
    out.focalLengthY = SightlineFraming::readFloat32Le(payload.data() + 5U);
    out.principalPointX = SightlineFraming::readFloat32Le(payload.data() + 9U);
    out.principalPointY = SightlineFraming::readFloat32Le(payload.data() + 13U);
    out.radialDistortionK1 = SightlineFraming::readFloat32Le(payload.data() + 17U);
    out.radialDistortionK2 = SightlineFraming::readFloat32Le(payload.data() + 21U);
    out.tangentialP1 = SightlineFraming::readFloat32Le(payload.data() + 25U);
    out.tangentialP2 = SightlineFraming::readFloat32Le(payload.data() + 29U);
    return true;
}

bool SightlineNucParser::parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraParameterFile) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.action = payload[1U];

    if (payload.size() > 2U) {
        const char* strStart = reinterpret_cast<const char*>(payload.data() + 2U);
        std::size_t strLen { payload.size() - 2U };
        while (strLen > 0U && strStart[strLen - 1U] == '\0') {
            --strLen;
        }
        out.filename.assign(strStart, strLen);
    } else {
        out.filename.clear();
    }

    return true;
}

} // namespace Sightline
