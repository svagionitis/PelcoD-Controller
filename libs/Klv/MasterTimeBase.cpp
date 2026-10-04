#include "MasterTimeBase.h"

namespace Klv {

SystemMasterTimeBase::SystemMasterTimeBase(TimeBaseMode mode) noexcept
    : m_mode(mode)
    , m_steadyEpoch(std::chrono::steady_clock::now()) {
    calibrateOffset();
}

std::uint64_t SystemMasterTimeBase::nowUs() const noexcept {
    if (m_mode == TimeBaseMode::SystemEpoch) {
        const auto now = std::chrono::system_clock::now();
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return static_cast<std::uint64_t>(us > 0 ? us : 0);
    }

    // MonotonicOffset
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - m_steadyEpoch).count();
    const auto total = elapsed + m_monotonicOffsetUs;
    return static_cast<std::uint64_t>(total > 0 ? total : 0);
}

std::uint64_t SystemMasterTimeBase::toPts(std::uint64_t timestampUs) const noexcept {
    return ((timestampUs * 90ULL) / 1000ULL) & 0x1FFFFFFFFULL;
}

std::uint64_t SystemMasterTimeBase::toUs(std::uint64_t pts) const noexcept {
    return (pts * 1000ULL) / 90ULL;
}

void SystemMasterTimeBase::setMonotonicOffset(std::int64_t offsetUs) noexcept {
    m_monotonicOffsetUs = offsetUs;
}

void SystemMasterTimeBase::calibrateOffset() noexcept {
    const auto sysNow = std::chrono::system_clock::now();
    const auto sysUs = std::chrono::duration_cast<std::chrono::microseconds>(sysNow.time_since_epoch()).count();
    m_steadyEpoch = std::chrono::steady_clock::now();
    m_monotonicOffsetUs = sysUs;
}

TimeBaseMode SystemMasterTimeBase::mode() const noexcept {
    return m_mode;
}

} // namespace Klv
