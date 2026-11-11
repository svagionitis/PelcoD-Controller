#pragma once

/// @file LensTypes.h
/// @brief Strongly-typed value objects, limits, and domain enums for motorized lens control.

#include <cstddef>
#include <cstdint>

#include <algorithm>

namespace LensControl {

/// @class CameraIndex
/// @brief Strongly-typed channel/camera identifier [0..3].
class CameraIndex {
public:
    /// @brief Default constructor creating channel 0.
    constexpr CameraIndex() noexcept = default;

    /// @brief Constructs a camera channel index.
    /// @param index Channel index (typically 0..3).
    explicit constexpr CameraIndex(std::uint8_t index) noexcept
        : m_index { index }
    {
    }

    /// @brief Returns the raw channel index value.
    /// @return Unsigned 8-bit channel index.
    [[nodiscard]] constexpr std::uint8_t value() const noexcept
    {
        return m_index;
    }

    [[nodiscard]] constexpr bool operator==(CameraIndex other) const noexcept
    {
        return m_index == other.m_index;
    }
    [[nodiscard]] constexpr bool operator!=(CameraIndex other) const noexcept
    {
        return m_index != other.m_index;
    }
    [[nodiscard]] constexpr bool operator<(CameraIndex other) const noexcept
    {
        return m_index < other.m_index;
    }

private:
    std::uint8_t m_index { 0U };
};

/// @class MotorPosition
/// @brief Strongly-typed 16-bit motor encoder position [0..65535].
class MotorPosition {
public:
    /// @brief Default constructor initializing position to 0.
    constexpr MotorPosition() noexcept = default;

    /// @brief Constructs a motor encoder position.
    /// @param pos Encoder step coordinate.
    explicit constexpr MotorPosition(std::uint16_t pos) noexcept
        : m_pos { pos }
    {
    }

    /// @brief Returns the raw encoder coordinate.
    /// @return 16-bit unsigned position.
    [[nodiscard]] constexpr std::uint16_t value() const noexcept
    {
        return m_pos;
    }

    [[nodiscard]] constexpr bool operator==(MotorPosition other) const noexcept
    {
        return m_pos == other.m_pos;
    }
    [[nodiscard]] constexpr bool operator!=(MotorPosition other) const noexcept
    {
        return m_pos != other.m_pos;
    }
    [[nodiscard]] constexpr bool operator<(MotorPosition other) const noexcept
    {
        return m_pos < other.m_pos;
    }
    [[nodiscard]] constexpr bool operator<=(MotorPosition other) const noexcept
    {
        return m_pos <= other.m_pos;
    }
    [[nodiscard]] constexpr bool operator>(MotorPosition other) const noexcept
    {
        return m_pos > other.m_pos;
    }
    [[nodiscard]] constexpr bool operator>=(MotorPosition other) const noexcept
    {
        return m_pos >= other.m_pos;
    }

private:
    std::uint16_t m_pos { 0U };
};

/// @class MotorSpeed
/// @brief Strongly-typed actuator velocity count [0..255].
class MotorSpeed {
public:
    static constexpr std::uint8_t kMinSpeed = 0U;
    static constexpr std::uint8_t kDefaultSpeed = 127U;
    static constexpr std::uint8_t kMaxSpeed = 255U;

    /// @brief Default constructor setting speed to default (127).
    constexpr MotorSpeed() noexcept = default;

    /// @brief Constructs a motor speed value.
    /// @param speed Speed magnitude count [0..255].
    explicit constexpr MotorSpeed(std::uint8_t speed) noexcept
        : m_speed { speed }
    {
    }

    /// @brief Returns raw speed magnitude count.
    /// @return 8-bit unsigned speed count.
    [[nodiscard]] constexpr std::uint8_t value() const noexcept
    {
        return m_speed;
    }

    /// @brief Tests whether speed is zero (stopped).
    /// @return True if speed is 0.
    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return m_speed == kMinSpeed;
    }

    /// @brief Tests whether speed is maximum (255).
    /// @return True if speed is 255.
    [[nodiscard]] constexpr bool isMax() const noexcept
    {
        return m_speed == kMaxSpeed;
    }

    [[nodiscard]] constexpr bool operator==(MotorSpeed other) const noexcept
    {
        return m_speed == other.m_speed;
    }
    [[nodiscard]] constexpr bool operator!=(MotorSpeed other) const noexcept
    {
        return m_speed != other.m_speed;
    }
    [[nodiscard]] constexpr bool operator<(MotorSpeed other) const noexcept
    {
        return m_speed < other.m_speed;
    }

private:
    std::uint8_t m_speed { kDefaultSpeed };
};

/// @class ZoomSpeed
/// @brief Distinct strong value type for optical zoom velocity.
class ZoomSpeed {
public:
    explicit constexpr ZoomSpeed(std::uint8_t speed = MotorSpeed::kDefaultSpeed) noexcept
        : m_speed { speed }
    {
    }
    explicit constexpr ZoomSpeed(MotorSpeed speed) noexcept
        : m_speed { speed.value() }
    {
    }

    [[nodiscard]] constexpr std::uint8_t value() const noexcept
    {
        return m_speed;
    }
    [[nodiscard]] constexpr MotorSpeed toMotorSpeed() const noexcept
    {
        return MotorSpeed { m_speed };
    }
    [[nodiscard]] constexpr bool operator==(ZoomSpeed other) const noexcept
    {
        return m_speed == other.m_speed;
    }
    [[nodiscard]] constexpr bool operator!=(ZoomSpeed other) const noexcept
    {
        return m_speed != other.m_speed;
    }

private:
    std::uint8_t m_speed { MotorSpeed::kDefaultSpeed };
};

/// @class FocusSpeed
/// @brief Distinct strong value type for optical focus velocity.
class FocusSpeed {
public:
    explicit constexpr FocusSpeed(std::uint8_t speed = MotorSpeed::kDefaultSpeed) noexcept
        : m_speed { speed }
    {
    }
    explicit constexpr FocusSpeed(MotorSpeed speed) noexcept
        : m_speed { speed.value() }
    {
    }

    [[nodiscard]] constexpr std::uint8_t value() const noexcept
    {
        return m_speed;
    }
    [[nodiscard]] constexpr MotorSpeed toMotorSpeed() const noexcept
    {
        return MotorSpeed { m_speed };
    }
    [[nodiscard]] constexpr bool operator==(FocusSpeed other) const noexcept
    {
        return m_speed == other.m_speed;
    }
    [[nodiscard]] constexpr bool operator!=(FocusSpeed other) const noexcept
    {
        return m_speed != other.m_speed;
    }

private:
    std::uint8_t m_speed { MotorSpeed::kDefaultSpeed };
};

/// @enum LensMode
/// @brief High-level optical operating mode matching Sightline SetLensMode (0x6C).
enum class LensMode : std::uint8_t {
    ManualJog = 0U,
    AutoFocusOnce = 1U,
    AutoFocusContinuous = 2U,
    ZoomTrackOnly = 3U,
    Calibrating = 4U,
    Idle = 5U
};

/// @enum LensLimitFlags
/// @brief Actuator limit switch states matching Sightline CurrentLensStatus (0x6D).
enum class LensLimitFlags : std::uint8_t {
    None = 0x00U,
    NearLimit = 0x01U,
    FarLimit = 0x02U,
    WideLimit = 0x04U,
    TeleLimit = 0x08U
};

[[nodiscard]] constexpr LensLimitFlags operator|(LensLimitFlags a, LensLimitFlags b) noexcept
{
    return static_cast<LensLimitFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr LensLimitFlags operator&(LensLimitFlags a, LensLimitFlags b) noexcept
{
    return static_cast<LensLimitFlags>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr bool hasFlag(std::uint8_t bitmask, LensLimitFlags flag) noexcept
{
    return (bitmask & static_cast<std::uint8_t>(flag)) != 0U;
}

/// @enum LensCommandId
/// @brief Discrete lens actuation commands matching Sightline LensCommand (0xB2 / 0x73).
enum class LensCommandId : std::uint8_t {
    Stop = 0x00U,
    FocusNear = 0x01U,
    FocusFar = 0x02U,
    ZoomTele = 0x03U,
    ZoomWide = 0x04U,
    GoTo = 0x05U,
    AutoFocus = 0x0AU,
    RawSerial = 0x80U
};

/// @enum AutoFocusMethod
/// @brief Search algorithm variant for closed-loop software autofocus.
enum class AutoFocusMethod : std::uint8_t {
    SeekAdaptive = 0U, ///< Sightline Seek: Direction flip on metric drop with RateAdjust
    ScanThreePhase = 1U, ///< Sightline Scan: Coarse -> Mid -> Fine sweep
    GoldenSection = 2U, ///< Golden Section Search for logarithmic peak convergence
    AdaptiveMomentum = 3U, ///< Variable step hill-climb with momentum and backlash correction
    DctFrequency = 4U, ///< 2D 8x8 DCT AC high-frequency energy peak
    BuiltInDriver = 5U ///< Hardware native AF (Sony VISCA, etc.)
};

/// @enum FocusMetricType
/// @brief Spatial and frequency metric modes for focus evaluation.
enum class FocusMetricType : std::uint8_t {
    SobelGradientSquared = 0U,
    DiscreteLaplacian = 1U,
    TenengradThreshold = 2U,
    VollathCorrelationF4 = 3U,
    VollathCorrelationF5 = 4U,
    DctHighFrequency = 5U
};

/// @struct LensRanges
/// @brief Mechanical travel boundaries matching Sightline LensRanges (0xB4).
struct LensRanges {
    std::uint16_t focusMin { 0U };
    std::uint16_t focusMax { 65535U };
    std::uint16_t zoomMin { 0U };
    std::uint16_t zoomMax { 65535U };

    /// @brief Validates that minimum bounds do not exceed maximum bounds.
    /// @return True if both focus and zoom boundaries are well-ordered.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return focusMin <= focusMax && zoomMin <= zoomMax;
    }

    /// @brief Clamps a candidate focus position to the configured travel range.
    /// @param pos Requested focus encoder coordinate.
    /// @return Clamped MotorPosition within [focusMin, focusMax].
    [[nodiscard]] constexpr MotorPosition clampFocus(MotorPosition pos) const noexcept
    {
        const std::uint16_t clamped = std::clamp(pos.value(), focusMin, focusMax);
        return MotorPosition { clamped };
    }

    /// @brief Clamps a candidate zoom position to the configured travel range.
    /// @param pos Requested zoom encoder coordinate.
    /// @return Clamped MotorPosition within [zoomMin, zoomMax].
    [[nodiscard]] constexpr MotorPosition clampZoom(MotorPosition pos) const noexcept
    {
        const std::uint16_t clamped = std::clamp(pos.value(), zoomMin, zoomMax);
        return MotorPosition { clamped };
    }
};

/// @struct NormalizedRoi
/// @brief Region of Interest coordinates normalized to [0..255] fractional coordinates.
struct NormalizedRoi {
    std::uint8_t colFrac { 64U }; ///< Left offset fraction [0..255] (default 25%)
    std::uint8_t rowFrac { 64U }; ///< Top offset fraction [0..255] (default 25%)
    std::uint8_t widthFrac { 128U }; ///< Width fraction [0..255] (default center 50%)
    std::uint8_t highFrac { 128U }; ///< Height fraction [0..255] (default center 50%)

    /// @brief Verifies that ROI dimensions are non-zero and remain inside normalized unit frame.
    /// @return True if valid rectangle wholly contained in [0..256].
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (widthFrac > 0U) && (highFrac > 0U)
            && (static_cast<std::uint16_t>(colFrac) + static_cast<std::uint16_t>(widthFrac) <= 256U)
            && (static_cast<std::uint16_t>(rowFrac) + static_cast<std::uint16_t>(highFrac) <= 256U);
    }
};

/// @struct FocusMetricScore
/// @brief Telemetry record for sharpness evaluation results matching Sightline Message 0x55.
struct FocusMetricScore {
    std::uint32_t metric { 0U };
    std::uint32_t mean { 0U };
    std::uint32_t variance { 0U };
    std::uint32_t pixelCount { 0U };
    std::uint64_t timestampNs { 0U };
    std::uint32_t frameId { 0U };
};

/// @struct LensStatus
/// @brief Operational status record matching Sightline Message 0x6D.
struct LensStatus {
    MotorPosition focusPos { 0U };
    MotorPosition zoomPos { 0U };
    std::uint8_t focusLimits { 0U };
    std::uint8_t zoomLimits { 0U };
    CameraIndex camIdx { 0U };
    bool isAfActive { false };
    LensMode mode { LensMode::ManualJog };
};

/// @enum LensLoadResult
/// @brief Status codes for dynamic Lens DLL loading operations.
enum class LensLoadResult : std::uint8_t {
    Success = 0U,
    FileNotFound = 1U,
    MissingCreateSymbol = 2U,
    PortCollision = 3U,
    LicenseDisallowed = 4U,
    InitFailed = 5U,
    UnloadPreviousFailed = 6U
};

} // namespace LensControl
