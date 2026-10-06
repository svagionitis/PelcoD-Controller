#pragma once

/// @file SightlineNucCaps.h
/// @brief Firmware-dependent NUC/DPR capabilities and validation result codes.
/// @details Version gates are taken from the "New in x.y" notes in
///          docs/protocols/Sightline/EAN-NUC-and-DPR.pdf, because the IDD change log marks
///          SLANucParameters_t / SLAReadWriteNuc_t as modified without naming the fields.
///          An unknown version (0.0, i.e. no 0x40 reply yet) is treated conservatively as 3.0.

#include "../SightlineTypes.h"
#include "SightlineGeneral.h"
#include "SightlineNuc.h"

#include <cstdint>

namespace Sightline {

/// @struct FwVersion
/// @brief Sightline software version as reported by SLAVersionNumber_t (0x40) swMajor / swMinor.
struct FwVersion {
    std::uint8_t major { 0U }; ///< Software major version (0 = unknown)
    std::uint8_t minor { 0U }; ///< Software minor version
};

/// @enum NucTail
/// @brief How much of the optional 0x35 tail to serialise.
/// @details Truncation always ends on an IDD field boundary. Shorter tails avoid overwriting
///          settings that have no "keep current" value (deadReplace, numReplace, destripe*).
enum class NucTail : std::uint8_t {
    Base = 0U, ///< Through maxNumDead: 28-byte payload (EAN Appendix A4 form, FW 3.0)
    Dpr = 1U, ///< Through deadFilterThresh: 33 bytes (FW 3.3)
    Destripe = 2U, ///< Through destripeSections: 35 bytes (FW 3.9)
    Named = 3U ///< Full layout including nucName: 36 + name length (FW 3.10)
};

/// @enum NucError
/// @brief Result of validating an outgoing NUC/DPR command against the IDD and firmware.
enum class NucError : std::uint8_t {
    Ok = 0U, ///< Command is valid
    NumReplace = 1U, ///< numReplace outside 1..8
    FilterThresh = 2U, ///< deadFilterThresh outside 0..255
    GainRange = 3U, ///< min/maxDeadGain above 999 %
    OffsetRange = 4U, ///< min/maxDeadOff outside -999999..999999
    StdDevRange = 5U, ///< maxStdDevDead above 65535
    DprFieldsSet = 6U, ///< DPR limit fields non-zero while nucRunMode is not CalcDead
    NumFramesSet = 7U, ///< numFrames non-zero for Calc2Point
    NameTooLong = 8U, ///< Name exceeds the permitted length
    NameInvalid = 9U, ///< Name has an extension or characters other than [A-Za-z0-9_-]
    FileNameBlank = 10U, ///< Required file name is empty
    SecondaryUnused = 11U, ///< Secondary name / ratio set for an operation that ignores them
    ReservedSet = 12U, ///< Reserved or flag field outside its permitted values
    Unsupported = 13U, ///< Feature not available on the reported firmware version
    StateUnknown = 14U, ///< Command would overwrite board settings not yet read back (query 0x35)
    NotSent = 15U, ///< Transport rejected the packet
    NotRunning = 16U, ///< No workflow step is pending
    Busy = 17U ///< A workflow is already running
};

/// @class SightlineNucCaps
/// @brief Stateless queries for firmware-gated NUC/DPR features.
class SightlineNucCaps {
public:
    /// @brief Extracts the software version from a Version Number reply.
    /// @param[in] ver Parsed SLAVersionNumber_t (0x40).
    /// @return swMajor / swMinor as a FwVersion.
    [[nodiscard]] static FwVersion fwFromVersion(const MsgVersionNumber& ver) noexcept;

    /// @brief Tests whether a firmware version is at least major.minor.
    /// @details An unknown version (major 0) never satisfies any requirement.
    /// @param[in] fw Firmware version.
    /// @param[in] major Required major version.
    /// @param[in] minor Required minor version.
    /// @return True if @p fw >= major.minor.
    [[nodiscard]] static bool atLeast(FwVersion fw, std::uint8_t major, std::uint8_t minor) noexcept;

    /// @brief Returns the longest 0x35 tail the firmware understands.
    /// @param[in] fw Firmware version.
    /// @return NucTail::Base for unknown or pre-3.3 firmware, up to NucTail::Named for 3.10+.
    [[nodiscard]] static NucTail maxNucTail(FwVersion fw) noexcept;

    /// @brief Tests whether a 0x35 run mode is available.
    /// @details AutoDead / ShutterFlatten need 3.3; Noise3DStats needs 3.4; others always.
    /// @param[in] fw Firmware version.
    /// @param[in] run Run mode.
    /// @return True if supported.
    [[nodiscard]] static bool supportsRun(FwVersion fw, NucRunMode run) noexcept;

    /// @brief Tests whether a 0x36 file operation is available.
    /// @details LoadInterpolated needs 3.9; Load/SaveShutterFlatten need 3.11; others always.
    /// @param[in] fw Firmware version.
    /// @param[in] op File operation.
    /// @return True if supported.
    [[nodiscard]] static bool supportsFileOp(FwVersion fw, NucFileOp op) noexcept;

    /// @brief Tests whether manual / dynamic dead pixel editing (0xA8) is available (3.3+).
    /// @param[in] fw Firmware version.
    /// @return True if supported.
    [[nodiscard]] static bool supportsDeadPixel(FwVersion fw) noexcept;

    /// @brief Returns a short human-readable description of a validation result.
    /// @param[in] err Validation result.
    /// @return Static, null-terminated string; never null.
    [[nodiscard]] static const char* errorText(NucError err) noexcept;
};

} // namespace Sightline
