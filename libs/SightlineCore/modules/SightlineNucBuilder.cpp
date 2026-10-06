/// @file SightlineNucBuilder.cpp
/// @brief Implementation of Sightline NUC and dead pixel command serializers (IDD v3.11).

#include "SightlineNucBuilder.h"

namespace Sightline {

namespace {

    /// @brief Fixed (pre-nucName) payload size of SLANucParameters_t in IDD v3.11.
    constexpr std::size_t kNucParamsFixed { 35U };

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

} // namespace

std::vector<std::uint8_t> SightlineNucBuilder::buildNucParameters(const MsgNucParameters& msg)
{
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
    if (!SightlineFraming::appendLenString(payload, msg.nucName)) {
        return {};
    }
    return SightlineFraming::buildPacket(MessageId::NucParameters, payload);
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

std::vector<std::uint8_t> SightlineNucBuilder::buildSetUserPalette(const MsgUserPalette& msg)
{
    std::vector<std::uint8_t> payload {};
    if (msg.lutData.size() == 768U && msg.paletteIndex == 0U) {
        payload = msg.lutData;
    } else {
        payload.reserve(1U + msg.lutData.size());
        payload.push_back(msg.paletteIndex);
        payload.insert(payload.end(), msg.lutData.begin(), msg.lutData.end());
    }
    return SightlineFraming::buildPacket(MessageId::SetUserPalette, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetUserPalette(std::uint8_t paletteIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetUserPalette), paletteIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetDeadPixelStats(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::DeadPixelStats), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildCameraCalibration(const MsgCameraCalibration& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + (8U * sizeof(float)));
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendFloat32Le(payload, msg.focalLengthX);
    SightlineFraming::appendFloat32Le(payload, msg.focalLengthY);
    SightlineFraming::appendFloat32Le(payload, msg.principalPointX);
    SightlineFraming::appendFloat32Le(payload, msg.principalPointY);
    SightlineFraming::appendFloat32Le(payload, msg.radialDistortionK1);
    SightlineFraming::appendFloat32Le(payload, msg.radialDistortionK2);
    SightlineFraming::appendFloat32Le(payload, msg.tangentialP1);
    SightlineFraming::appendFloat32Le(payload, msg.tangentialP2);
    return SightlineFraming::buildPacket(MessageId::CameraCalibration, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetCameraCalibration(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CameraCalibration), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildCameraParameterFile(const MsgCameraParameterFile& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + msg.filename.size() + 1U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.action);
    SightlineFraming::appendString(payload, msg.filename);
    return SightlineFraming::buildPacket(MessageId::CameraParameterFile, payload);
}

} // namespace Sightline
