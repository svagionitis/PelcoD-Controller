#pragma once

/// @file MasterTimeBase.h
/// @brief Master clock and timebase reference for STANAG 4609 PTS generation.

#include "PtsSyncTypes.h"
#include <chrono>
#include <cstdint>
#include <memory>

namespace Klv {

/// @class IMasterTimeBase
/// @brief Pure virtual interface providing unified time stamping and 90 kHz PTS conversions.
class IMasterTimeBase {
public:
    virtual ~IMasterTimeBase() = default;

    /// @brief Current time in microseconds since Unix epoch.
    /// @return Microsecond epoch timestamp.
    [[nodiscard]] virtual std::uint64_t nowUs() const noexcept = 0;

    /// @brief Converts a microsecond timestamp to 33-bit 90 kHz PTS ticks.
    /// @param[in] timestampUs Microseconds since epoch.
    /// @return 33-bit PTS tick integer modulo 2^33.
    [[nodiscard]] virtual std::uint64_t toPts(std::uint64_t timestampUs) const noexcept = 0;

    /// @brief Converts 90 kHz PTS ticks to microseconds.
    /// @param[in] pts 33-bit PTS ticks.
    /// @return Microseconds equivalent.
    [[nodiscard]] virtual std::uint64_t toUs(std::uint64_t pts) const noexcept = 0;
};

/// @class SystemMasterTimeBase
/// @brief High-precision time base supporting system UTC and monotonic clocks.
class SystemMasterTimeBase : public IMasterTimeBase {
public:
    /// @brief Constructs time base with selected operating mode.
    /// @param[in] mode Operating time base mode.
    explicit SystemMasterTimeBase(TimeBaseMode mode = TimeBaseMode::SystemEpoch) noexcept;

    [[nodiscard]] std::uint64_t nowUs() const noexcept override;
    [[nodiscard]] std::uint64_t toPts(std::uint64_t timestampUs) const noexcept override;
    [[nodiscard]] std::uint64_t toUs(std::uint64_t pts) const noexcept override;

    /// @brief Sets monotonic offset relative to UTC epoch.
    /// @param[in] offsetUs Calibration offset in microseconds.
    void setMonotonicOffset(std::int64_t offsetUs) noexcept;

    /// @brief Calibrates monotonic offset using current system_clock and steady_clock.
    void calibrateOffset() noexcept;

    /// @brief Gets active mode.
    /// @return Active TimeBaseMode.
    [[nodiscard]] TimeBaseMode mode() const noexcept;

private:
    TimeBaseMode m_mode { TimeBaseMode::SystemEpoch };
    std::int64_t m_monotonicOffsetUs { 0 };
    std::chrono::steady_clock::time_point m_steadyEpoch {};
};

} // namespace Klv
