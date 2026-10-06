/// @file SightlineNucCaps.cpp
/// @brief Implementation of firmware-gated NUC/DPR capability queries.

#include "SightlineNucCaps.h"

namespace Sightline {

namespace {

    /// @brief Minor version for features introduced in 3.3 (Auto DPR, manual DPR, filters).
    constexpr std::uint8_t kMinorDpr { 3U };
    /// @brief Minor version for 3D noise statistics (3.4).
    constexpr std::uint8_t kMinorNoise { 4U };
    /// @brief Minor version for destripe and interpolation (3.9).
    constexpr std::uint8_t kMinorDestripe { 9U };
    /// @brief Minor version for multi-NUC creation (3.10).
    constexpr std::uint8_t kMinorNamed { 10U };
    /// @brief Minor version for saving / applying shutter flatten tables (3.11).
    constexpr std::uint8_t kMinorFlatFile { 11U };
    /// @brief Major version of every gate above.
    constexpr std::uint8_t kMajor3 { 3U };

} // namespace

FwVersion SightlineNucCaps::fwFromVersion(const MsgVersionNumber& ver) noexcept
{
    return FwVersion { ver.softwareMajor, ver.softwareMinor };
}

bool SightlineNucCaps::atLeast(FwVersion fw, std::uint8_t major, std::uint8_t minor) noexcept
{
    bool result { false };
    if (fw.major == 0U) {
        result = false;
    } else if (fw.major != major) {
        result = fw.major > major;
    } else {
        result = fw.minor >= minor;
    }
    return result;
}

NucTail SightlineNucCaps::maxNucTail(FwVersion fw) noexcept
{
    NucTail tail { NucTail::Base };
    if (atLeast(fw, kMajor3, kMinorNamed)) {
        tail = NucTail::Named;
    } else if (atLeast(fw, kMajor3, kMinorDestripe)) {
        tail = NucTail::Destripe;
    } else if (atLeast(fw, kMajor3, kMinorDpr)) {
        tail = NucTail::Dpr;
    } else {
        tail = NucTail::Base;
    }
    return tail;
}

bool SightlineNucCaps::supportsRun(FwVersion fw, NucRunMode run) noexcept
{
    bool result { true };
    switch (run) {
    case NucRunMode::AutoDead:
    case NucRunMode::ShutterFlatten:
        result = atLeast(fw, kMajor3, kMinorDpr);
        break;
    case NucRunMode::Noise3DStats:
        result = atLeast(fw, kMajor3, kMinorNoise);
        break;
    case NucRunMode::None:
    case NucRunMode::AddFrames:
    case NucRunMode::ClearFrames:
    case NucRunMode::ResetGain:
    case NucRunMode::ResetOffset:
    case NucRunMode::ResetDead:
    case NucRunMode::ResetAll:
    case NucRunMode::Calc1Point:
    case NucRunMode::Calc2Point:
    case NucRunMode::CalcDead:
    case NucRunMode::CalcReplace:
        result = true;
        break;
    default:
        result = false;
        break;
    }
    return result;
}

bool SightlineNucCaps::supportsFileOp(FwVersion fw, NucFileOp op) noexcept
{
    bool result { true };
    switch (op) {
    case NucFileOp::LoadInterpolated:
        result = atLeast(fw, kMajor3, kMinorDestripe);
        break;
    case NucFileOp::LoadShutterFlatten:
    case NucFileOp::SaveShutterFlatten:
        result = atLeast(fw, kMajor3, kMinorFlatFile);
        break;
    case NucFileOp::None:
    case NucFileOp::SaveNuc:
    case NucFileOp::LoadNuc:
    case NucFileOp::SaveDead:
    case NucFileOp::LoadDead:
        result = true;
        break;
    default:
        result = false;
        break;
    }
    return result;
}

bool SightlineNucCaps::supportsDeadPixel(FwVersion fw) noexcept
{
    return atLeast(fw, kMajor3, kMinorDpr);
}

const char* SightlineNucCaps::errorText(NucError err) noexcept
{
    const char* text { "Unknown error" };
    switch (err) {
    case NucError::Ok:
        text = "OK";
        break;
    case NucError::NumReplace:
        text = "Replacement neighbours must be 1..8";
        break;
    case NucError::FilterThresh:
        text = "Dead filter threshold must be 0..255";
        break;
    case NucError::GainRange:
        text = "Dead gain limits must be 0..999 %";
        break;
    case NucError::OffsetRange:
        text = "Dead offset limits must be within +/-999999";
        break;
    case NucError::StdDevRange:
        text = "Max standard deviation must be <= 65535";
        break;
    case NucError::DprFieldsSet:
        text = "DPR limits are only allowed with Calculate Dead Pixels";
        break;
    case NucError::NumFramesSet:
        text = "Frame count must be 0 for the 2-point calculation";
        break;
    case NucError::NameTooLong:
        text = "Name is too long";
        break;
    case NucError::NameInvalid:
        text = "Name may only contain letters, digits, '_' and '-' (no extension)";
        break;
    case NucError::FileNameBlank:
        text = "A table name is required";
        break;
    case NucError::SecondaryUnused:
        text = "Secondary table / ratio not used by this operation";
        break;
    case NucError::ReservedSet:
        text = "Reserved or flag field out of range";
        break;
    case NucError::Unsupported:
        text = "Not supported by this firmware version";
        break;
    case NucError::StateUnknown:
        text = "Board NUC settings unknown; query NUC parameters first";
        break;
    case NucError::NotSent:
        text = "Command could not be sent";
        break;
    case NucError::NotRunning:
        text = "No workflow step is pending";
        break;
    case NucError::Busy:
        text = "A NUC workflow is already running";
        break;
    default:
        text = "Unknown error";
        break;
    }
    return text;
}

} // namespace Sightline
