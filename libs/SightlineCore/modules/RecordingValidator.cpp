/// @file RecordingValidator.cpp
/// @brief Implementation of pre-flight recording and snapshot parameter validation.

#include "RecordingValidator.h"

namespace Sightline {

RecordingStatusCode RecordingValidator::checkFilename(
    std::string_view filename,
    std::uint8_t flags) noexcept
{
    if (filename.empty() || filename.size() > MaxFilenameLength) {
        return RecordingStatusCode::ErrInvalidCharacters;
    }

    constexpr std::string_view kIllegalChars { "/\\:*?\"<>|" };
    if (filename.find_first_of(kIllegalChars) != std::string_view::npos) {
        return RecordingStatusCode::ErrInvalidCharacters;
    }

    const char lastChar { filename.back() };
    const bool isDigit { lastChar >= '0' && lastChar <= '9' };
    const bool allowNumeric {
        (flags & static_cast<std::uint8_t>(RecordingFlags::AllowNumericOverwrite)) != 0U
    };

    if (isDigit && !allowNumeric) {
        return RecordingStatusCode::ErrNumericFilename;
    }

    return RecordingStatusCode::Success;
}

RecordingStatusCode RecordingValidator::checkCamera(
    std::uint8_t cameraIndex,
    bool allowAllCameras) noexcept
{
    if (allowAllCameras && cameraIndex == 0xFFU) {
        return RecordingStatusCode::Success;
    }

    if (cameraIndex > 2U) {
        return RecordingStatusCode::ErrChannelUnsupported;
    }

    return RecordingStatusCode::Success;
}

RecordingStatusCode RecordingValidator::checkSnapshot(
    const MsgDoSnapShotV2& msg) noexcept
{
    const auto camStatus { checkCamera(msg.cameraIndex, true) };
    if (camStatus != RecordingStatusCode::Success) {
        return camStatus;
    }

    if (msg.qualityLevel == 0U || msg.qualityLevel > 100U) {
        return RecordingStatusCode::ErrMalformedPayload;
    }

    if (msg.burstCount == 0U || msg.burstCount > 120U) {
        return RecordingStatusCode::ErrMalformedPayload;
    }

    if (!msg.customFilename.empty()) {
        const auto fileStatus { checkFilename(msg.customFilename, 0U) };
        if (fileStatus != RecordingStatusCode::Success) {
            return fileStatus;
        }
    }

    return RecordingStatusCode::Success;
}

RecordingStatusCode RecordingValidator::checkStorage(
    StorageDestination destination,
    std::uint64_t availableBytes,
    bool isMounted,
    std::uint32_t& outFreeMB) noexcept
{
    outFreeMB = static_cast<std::uint32_t>(availableBytes / (1024ULL * 1024ULL));

    if (destination == StorageDestination::FtpPush) {
        return RecordingStatusCode::Success;
    }

    if (!isMounted) {
        return RecordingStatusCode::ErrMediaUnavailable;
    }

    if (availableBytes < MinRequiredFreeBytes) {
        return RecordingStatusCode::ErrInsufficientStorage;
    }

    return RecordingStatusCode::Success;
}

} // namespace Sightline
