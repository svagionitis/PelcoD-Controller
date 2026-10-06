/// @file SightlineNucBuilder.cpp
/// @brief Implementation of Sightline NUC and dead pixel command serializers (IDD v3.11).

#include "SightlineNucBuilder.h"

namespace Sightline {

namespace {

    /// @brief Fixed (pre-nucName) payload size of SLANucParameters_t in IDD v3.11.
    constexpr std::size_t kNucParamsFixed { 35U };

    /// @brief 0x35 payload size through maxNumDead (NucTail::Base, EAN Appendix A4).
    constexpr std::size_t kNucBaseSize { 28U };

    /// @brief 0x35 payload size through deadFilterThresh (NucTail::Dpr).
    constexpr std::size_t kNucDprSize { 33U };

    /// @brief Maximum multi-NUC name length (EAN section 4.4: fewer than 64 characters).
    constexpr std::size_t kMaxNucName { 63U };

    /// @brief Maximum SVPLenString_t length (u8 length prefix).
    constexpr std::size_t kMaxFileName { 255U };

    /// @brief Maximum dead pixel gain limit, percent (IDD 0x35 bytes 8-11).
    constexpr std::uint16_t kMaxDeadGain { 999U };

    /// @brief Maximum magnitude of the dead pixel offset limits (IDD 0x35 bytes 16-23).
    constexpr std::int32_t kMaxDeadOff { 999999 };

    /// @brief Maximum standard deviation limit (IDD 0x35 bytes 24-27).
    constexpr std::uint32_t kMaxStdDev { 65535U };

    /// @brief Permitted numReplace range (IDD 0x35 byte 33).
    constexpr std::uint8_t kMinReplace { 1U };
    constexpr std::uint8_t kMaxReplace { 8U };

    /// @brief Maximum dead filter threshold (IDD 0x35 bytes 35-36).
    constexpr std::int16_t kMaxFilterThresh { 255 };

    /// @brief Firmware version introducing nucName (multi-NUC, EAN section 4.3).
    constexpr std::uint8_t kNamedMajor { 3U };
    constexpr std::uint8_t kNamedMinor { 10U };

    /// @brief Payload size of SLADeadPixel_t.
    constexpr std::size_t kDeadPixelSize { 8U };

    /// @brief Shift of NucDefaultOp within the 0x36 nucReadWriteMode byte.
    constexpr std::uint8_t kDefaultOpShift { 4U };

    /// @brief Converts a byte-sized enumerator to its wire value.
    /// @tparam E Enumeration with std::uint8_t underlying type.
    /// @param[in] value Enumerator.
    /// @return Underlying byte value.
    template <typename E>
    [[nodiscard]] constexpr std::uint8_t toByte(E value) noexcept
    {
        return static_cast<std::uint8_t>(value);
    }

    /// @brief Returns true if a blank 0x36 fileName is permitted by the IDD (mode 0x30 / 0x40).
    /// @param[in] msg Read/write NUC command.
    /// @return True for pure ClearNuc / ClearDead commands.
    [[nodiscard]] bool allowsBlankName(const MsgReadWriteNuc& msg) noexcept
    {
        return (msg.fileOp == NucFileOp::None)
            && ((msg.defaultOp == NucDefaultOp::ClearNuc) || (msg.defaultOp == NucDefaultOp::ClearDead));
    }

    /// @brief Encodes a 0xA8 add/remove pixel command.
    /// @param[in] cameraIndex Camera index.
    /// @param[in] mode Add or Remove.
    /// @param[in] column Pixel column (x).
    /// @param[in] row Pixel row (y).
    /// @param[in] deferUpdate True if more pixels follow.
    /// @return Framed packet.
    [[nodiscard]] std::vector<std::uint8_t> buildPixelOp(std::uint8_t cameraIndex, DeadPixelMode mode,
        std::uint16_t column, std::uint16_t row, bool deferUpdate)
    {
        MsgDeadPixel msg {};
        msg.cameraIndex = cameraIndex;
        msg.mode = mode;
        msg.a = column;
        msg.b = row;
        msg.c = deferUpdate ? 1U : 0U;
        return SightlineNucBuilder::buildDeadPixel(msg);
    }

    /// @brief Returns true for characters permitted in table names (EAN section 4.4).
    /// @param[in] ch Character.
    /// @return True for [A-Za-z0-9_-].
    [[nodiscard]] bool isNameChar(char ch) noexcept
    {
        const bool lower { (ch >= 'a') && (ch <= 'z') };
        const bool upper { (ch >= 'A') && (ch <= 'Z') };
        const bool digit { (ch >= '0') && (ch <= '9') };
        return lower || upper || digit || (ch == '_') || (ch == '-');
    }

    /// @brief Validates a table name's length and character set.
    /// @param[in] name Name without extension.
    /// @param[in] maxLen Maximum permitted length.
    /// @return NucError::Ok, NameTooLong, or NameInvalid.
    [[nodiscard]] NucError checkName(const std::string& name, std::size_t maxLen) noexcept
    {
        if (name.size() > maxLen) {
            return NucError::NameTooLong;
        }
        for (const char ch : name) {
            if (!isNameChar(ch)) {
                return NucError::NameInvalid;
            }
        }
        return NucError::Ok;
    }

    /// @brief Returns true if any CalcDead-only limit field is non-zero.
    /// @param[in] msg NUC parameters.
    /// @return True if bytes 8..31 would not be all zero.
    [[nodiscard]] bool hasDprFields(const MsgNucParameters& msg) noexcept
    {
        return (msg.minDeadGain != 0U) || (msg.maxDeadGain != 0U) || (msg.minDeadVal != 0U)
            || (msg.maxDeadVal != 0U) || (msg.minDeadOff != 0) || (msg.maxDeadOff != 0)
            || (msg.maxStdDevDead != 0U) || (msg.maxNumDead != 0);
    }

    /// @brief Returns true if an offset limit is within the IDD range.
    /// @param[in] value Offset limit.
    /// @return True if -999999 <= value <= 999999.
    [[nodiscard]] constexpr bool offsetOk(std::int32_t value) noexcept
    {
        return (value >= -kMaxDeadOff) && (value <= kMaxDeadOff);
    }

    /// @brief Validates the CalcDead limit fields.
    /// @param[in] msg NUC parameters with nucRunMode == CalcDead.
    /// @return NucError::Ok, GainRange, OffsetRange, or StdDevRange.
    [[nodiscard]] NucError checkDprLimits(const MsgNucParameters& msg) noexcept
    {
        if ((msg.minDeadGain > kMaxDeadGain) || (msg.maxDeadGain > kMaxDeadGain)) {
            return NucError::GainRange;
        }
        if (!offsetOk(msg.minDeadOff) || !offsetOk(msg.maxDeadOff)) {
            return NucError::OffsetRange;
        }
        if (msg.maxStdDevDead > kMaxStdDev) {
            return NucError::StdDevRange;
        }
        return NucError::Ok;
    }

    /// @brief Serialises the 35 fixed 0x35 bytes, then trims / extends per @p tail.
    /// @param[in] msg NUC parameters.
    /// @param[in] tail Last field group to serialise.
    /// @return Framed packet, or empty on a dropped / overlong name.
    [[nodiscard]] std::vector<std::uint8_t> encodeNuc(const MsgNucParameters& msg, NucTail tail)
    {
        if ((tail != NucTail::Named) && !msg.nucName.empty()) {
            return {};
        }
        std::vector<std::uint8_t> payload {};
        payload.reserve(kNucParamsFixed + 1U + msg.nucName.size());
        payload.push_back(msg.cameraIndex);
        payload.push_back(toByte(msg.nucShow));
        payload.push_back(toByte(msg.nucRunMode));
        payload.push_back(msg.numFrames);
        SightlineFraming::appendU16Le(payload, msg.minDeadGain);
        SightlineFraming::appendU16Le(payload, msg.maxDeadGain);
        SightlineFraming::appendU16Le(payload, msg.minDeadVal);
        SightlineFraming::appendU16Le(payload, msg.maxDeadVal);
        SightlineFraming::appendS32Le(payload, msg.minDeadOff);
        SightlineFraming::appendS32Le(payload, msg.maxDeadOff);
        SightlineFraming::appendU32Le(payload, msg.maxStdDevDead);
        SightlineFraming::appendS32Le(payload, msg.maxNumDead);
        payload.push_back(toByte(msg.deadReplace));
        payload.push_back(msg.numReplace);
        payload.push_back(toByte(msg.deadFilter));
        SightlineFraming::appendS16Le(payload, msg.deadFilterThresh);
        payload.push_back(msg.destripeAmount);
        payload.push_back(msg.destripeSections);

        switch (tail) {
        case NucTail::Base:
            payload.resize(kNucBaseSize);
            break;
        case NucTail::Dpr:
            payload.resize(kNucDprSize);
            break;
        case NucTail::Destripe:
            break;
        case NucTail::Named:
            if (!SightlineFraming::appendLenString(payload, msg.nucName)) {
                return {};
            }
            break;
        default:
            return {};
        }
        return SightlineFraming::buildPacket(MessageId::NucParameters, payload);
    }

} // namespace

std::vector<std::uint8_t> SightlineNucBuilder::buildNucParameters(const MsgNucParameters& msg)
{
    return encodeNuc(msg, NucTail::Named);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildNucParameters(const MsgNucParameters& msg, NucTail tail)
{
    return encodeNuc(msg, tail);
}

NucError SightlineNucBuilder::checkNucParams(const MsgNucParameters& msg, FwVersion fw)
{
    if (!SightlineNucCaps::supportsRun(fw, msg.nucRunMode)) {
        return NucError::Unsupported;
    }
    if ((msg.nucRunMode == NucRunMode::Calc2Point) && (msg.numFrames != 0U)) {
        return NucError::NumFramesSet;
    }
    if (msg.nucRunMode == NucRunMode::CalcDead) {
        const NucError limits { checkDprLimits(msg) };
        if (limits != NucError::Ok) {
            return limits;
        }
    } else if (hasDprFields(msg)) {
        return NucError::DprFieldsSet;
    } else {
        // No CalcDead limits to validate.
    }
    if ((msg.numReplace < kMinReplace) || (msg.numReplace > kMaxReplace)) {
        return NucError::NumReplace;
    }
    if ((msg.deadFilterThresh < 0) || (msg.deadFilterThresh > kMaxFilterThresh)) {
        return NucError::FilterThresh;
    }
    if (!msg.nucName.empty()) {
        if (!SightlineNucCaps::atLeast(fw, kNamedMajor, kNamedMinor)) {
            return NucError::Unsupported;
        }
        return checkName(msg.nucName, kMaxNucName);
    }
    return NucError::Ok;
}

NucError SightlineNucBuilder::checkReadWriteNuc(const MsgReadWriteNuc& msg, FwVersion fw)
{
    if (!SightlineNucCaps::supportsFileOp(fw, msg.fileOp)) {
        return NucError::Unsupported;
    }
    if (msg.fileName.empty()) {
        if (!allowsBlankName(msg)) {
            return NucError::FileNameBlank;
        }
    } else {
        const NucError primary { checkName(msg.fileName, kMaxFileName) };
        if (primary != NucError::Ok) {
            return primary;
        }
    }

    const bool needsSecondary { (msg.fileOp == NucFileOp::LoadInterpolated)
        || (msg.fileOp == NucFileOp::LoadShutterFlatten) };
    if (needsSecondary) {
        if (msg.secondaryFileName.empty()) {
            return NucError::FileNameBlank;
        }
        const NucError secondary { checkName(msg.secondaryFileName, kMaxFileName) };
        if (secondary != NucError::Ok) {
            return secondary;
        }
    } else if (!msg.secondaryFileName.empty()) {
        return NucError::SecondaryUnused;
    } else {
        // Secondary name correctly absent.
    }
    if ((msg.fileOp != NucFileOp::LoadInterpolated) && (msg.interpolationRatio != 0U)) {
        return NucError::SecondaryUnused;
    }
    return NucError::Ok;
}

NucError SightlineNucBuilder::checkDeadPixel(const MsgDeadPixel& msg, FwVersion fw)
{
    if (!SightlineNucCaps::supportsDeadPixel(fw)) {
        return NucError::Unsupported;
    }
    if (msg.d != 0U) {
        return NucError::ReservedSet;
    }
    if ((msg.mode != DeadPixelMode::DynamicDetect) && (msg.c > 1U)) {
        return NucError::ReservedSet;
    }
    return NucError::Ok;
}

std::vector<std::uint8_t> SightlineNucBuilder::buildDeadPixel(const MsgDeadPixel& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(kDeadPixelSize);
    payload.push_back(msg.cameraIndex);
    payload.push_back(toByte(msg.mode));
    SightlineFraming::appendU16Le(payload, msg.a);
    SightlineFraming::appendU16Le(payload, msg.b);
    payload.push_back(msg.c);
    payload.push_back(msg.d);
    return SightlineFraming::buildPacket(MessageId::DeadPixel, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildAddDeadPixel(
    std::uint8_t cameraIndex, std::uint16_t column, std::uint16_t row, bool deferUpdate)
{
    return buildPixelOp(cameraIndex, DeadPixelMode::Add, column, row, deferUpdate);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildRemoveDeadPixel(
    std::uint8_t cameraIndex, std::uint16_t column, std::uint16_t row, bool deferUpdate)
{
    return buildPixelOp(cameraIndex, DeadPixelMode::Remove, column, row, deferUpdate);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildDynamicDead(
    std::uint8_t cameraIndex, std::uint8_t kernelSize, std::uint8_t maxPixelDiff)
{
    MsgDeadPixel msg {};
    msg.cameraIndex = cameraIndex;
    msg.mode = DeadPixelMode::DynamicDetect;
    msg.a = kernelSize;
    msg.b = maxPixelDiff;
    return buildDeadPixel(msg);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetNucParameters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::NucParameters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildReadWriteNuc(const MsgReadWriteNuc& msg)
{
    if (msg.fileName.empty() && !allowsBlankName(msg)) {
        return {};
    }

    const auto mode { static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(toByte(msg.defaultOp) << kDefaultOpShift) | toByte(msg.fileOp)) };

    std::vector<std::uint8_t> payload {};
    payload.reserve(6U + msg.fileName.size() + msg.secondaryFileName.size());
    payload.push_back(msg.cameraIndex);
    payload.push_back(0x00U); // reserved
    payload.push_back(mode);
    if (!SightlineFraming::appendLenString(payload, msg.fileName)
        || !SightlineFraming::appendLenString(payload, msg.secondaryFileName)) {
        return {};
    }
    payload.push_back(msg.interpolationRatio);
    return SightlineFraming::buildPacket(MessageId::ReadWriteNuc, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetReadWriteNuc(NucTableQuery query, std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::ReadWriteNuc), toByte(query), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetDeadPixelStats(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::DeadPixelStats), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
