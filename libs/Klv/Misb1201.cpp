#include "Misb1201.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Klv {

namespace {

constexpr std::uint8_t kPosInfinityTop { 0xC8U };
constexpr std::uint8_t kNegInfinityTop { 0xE8U };
constexpr std::uint8_t kPosQuietNanTop { 0xD0U };
constexpr std::uint8_t kNegQuietNanTop { 0xF0U };
constexpr std::uint8_t kPosSignalingNanTop { 0xD8U };
constexpr std::uint8_t kNegSignalingNanTop { 0xF8U };
constexpr std::uint8_t kBelowMinTop { 0xE0U };
constexpr std::uint8_t kAboveMaxTop { 0xE1U };

[[nodiscard]] constexpr std::uint64_t lengthMask(std::size_t lengthBytes) noexcept {
    if (lengthBytes >= 8U) {
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    return (1ULL << (8U * lengthBytes)) - 1ULL;
}

} // namespace

std::size_t Misb1201::computeLength(double minVal, double maxVal, double precision) noexcept {
    if (minVal >= maxVal || precision <= 0.0 || precision >= (maxVal - minVal)) {
        return 0U;
    }

    const double span = maxVal - minVal;
    const double bPow = std::ceil(std::log2(span));
    const double gPow = std::floor(std::log2(precision));
    const double lBits = bPow - gPow + 1.0;
    const auto lengthBytes = static_cast<std::size_t>(std::ceil(lBits / 8.0));
    return std::clamp(lengthBytes, std::size_t{ 1U }, std::size_t{ 8U });
}

Misb1201Params Misb1201::computeParams(double minVal, double maxVal, std::size_t lengthBytes) noexcept {
    if (minVal >= maxVal || lengthBytes == 0U) {
        return Misb1201Params{};
    }

    const std::size_t clampedLen = std::min(lengthBytes, std::size_t{ 8U });
    const double span = maxVal - minVal;
    const double bPow = std::ceil(std::log2(span));
    const double dPow = static_cast<double>(8U * clampedLen - 1U);
    const double sF = std::exp2(dPow - bPow);
    const double sR = std::exp2(bPow - dPow);

    double zOffset { 0.0 };
    if (minVal < 0.0 && maxVal > 0.0) {
        zOffset = sF * minVal - std::floor(sF * minVal);
    }

    return Misb1201Params{ minVal, maxVal, clampedLen, sF, sR, zOffset, sR };
}

std::uint64_t Misb1201::encode(double val, double minVal, double maxVal, std::size_t lengthBytes) noexcept {
    const Misb1201Params params = computeParams(minVal, maxVal, lengthBytes);
    return encode(val, params);
}

std::uint64_t Misb1201::encode(double val, const Misb1201Params& params) noexcept {
    if (params.lengthBytes == 0U || params.lengthBytes > 8U) {
        return 0ULL;
    }

    const std::size_t shift = 8U * (params.lengthBytes - 1U);

    if (std::isnan(val)) {
        const std::uint8_t topByte = std::signbit(val) ? kNegQuietNanTop : kPosQuietNanTop;
        return static_cast<std::uint64_t>(topByte) << shift;
    }

    if (std::isinf(val)) {
        const std::uint8_t topByte = (val > 0.0) ? kPosInfinityTop : kNegInfinityTop;
        return static_cast<std::uint64_t>(topByte) << shift;
    }

    if (val < params.minVal) {
        return static_cast<std::uint64_t>(kBelowMinTop) << shift;
    }

    if (val > params.maxVal) {
        return static_cast<std::uint64_t>(kAboveMaxTop) << shift;
    }

    // Normal mapped value (ST 1201.1-12: negative zero treated as positive zero)
    double mappedVal = val;
    if (mappedVal == -0.0) {
        mappedVal = 0.0;
    }

    const double scaled = std::trunc(params.forwardScale * (mappedVal - params.minVal) + params.zeroOffset);
    const auto intVal = static_cast<std::uint64_t>(scaled);
    return intVal & lengthMask(params.lengthBytes);
}

void Misb1201::encodeBytes(double val, double minVal, double maxVal, std::size_t lengthBytes, std::vector<std::uint8_t>& out) {
    const std::uint64_t encoded = encode(val, minVal, maxVal, lengthBytes);
    for (std::size_t i = 0U; i < lengthBytes; ++i) {
        const std::size_t shiftBits = 8U * (lengthBytes - 1U - i);
        out.push_back(static_cast<std::uint8_t>((encoded >> shiftBits) & 0xFFULL));
    }
}

Misb1201Result Misb1201::decode(std::uint64_t rawInt, double minVal, double maxVal, std::size_t lengthBytes) noexcept {
    const Misb1201Params params = computeParams(minVal, maxVal, lengthBytes);
    return decode(rawInt, params);
}

Misb1201Result Misb1201::decode(std::uint64_t rawInt, const Misb1201Params& params) noexcept {
    if (params.lengthBytes == 0U || params.lengthBytes > 8U) {
        return Misb1201Result{ 0.0, Misb1201SpecialValue::Reserved };
    }

    const std::uint64_t val = rawInt & lengthMask(params.lengthBytes);
    const std::size_t nBits = 8U * params.lengthBytes;
    const std::uint64_t bitN = (val >> (nBits - 1U)) & 1ULL;
    const std::uint64_t bitN_1 = (val >> (nBits - 2U)) & 1ULL;

    if (bitN == 1ULL && bitN_1 == 1ULL) {
        const std::size_t shift = 8U * (params.lengthBytes - 1U);
        const auto topByte = static_cast<std::uint8_t>((val >> shift) & 0xFFULL);

        switch (topByte) {
            case kPosInfinityTop:
                return Misb1201Result{ std::numeric_limits<double>::infinity(), Misb1201SpecialValue::PositiveInfinity };
            case kNegInfinityTop:
                return Misb1201Result{ -std::numeric_limits<double>::infinity(), Misb1201SpecialValue::NegativeInfinity };
            case kPosQuietNanTop:
                return Misb1201Result{ std::numeric_limits<double>::quiet_NaN(), Misb1201SpecialValue::PositiveQuietNan };
            case kNegQuietNanTop:
                return Misb1201Result{ -std::numeric_limits<double>::quiet_NaN(), Misb1201SpecialValue::NegativeQuietNan };
            case kPosSignalingNanTop:
                return Misb1201Result{ std::numeric_limits<double>::signaling_NaN(), Misb1201SpecialValue::PositiveSignalingNan };
            case kNegSignalingNanTop:
                return Misb1201Result{ -std::numeric_limits<double>::signaling_NaN(), Misb1201SpecialValue::NegativeSignalingNan };
            case kBelowMinTop:
                return Misb1201Result{ params.minVal, Misb1201SpecialValue::BelowMinimum };
            case kAboveMaxTop:
                return Misb1201Result{ params.maxVal, Misb1201SpecialValue::AboveMaximum };
            default:
                if ((topByte & 0xF8U) == 0xC0U) {
                    return Misb1201Result{ 0.0, Misb1201SpecialValue::UserDefined };
                }
                return Misb1201Result{ 0.0, Misb1201SpecialValue::Reserved };
        }
    }

    if (bitN == 1ULL && bitN_1 == 0ULL) {
        // Table 1: msb=1, msb-1=0 is only valid if all lower bits are zero (max mapped value)
        const std::uint64_t lowerMask = (1ULL << (nBits - 1U)) - 1ULL;
        if ((val & lowerMask) == 0ULL) {
            const double normalVal = params.reverseScale * (static_cast<double>(val) - params.zeroOffset) + params.minVal;
            return Misb1201Result{ normalVal, Misb1201SpecialValue::Valid };
        }
        return Misb1201Result{ 0.0, Misb1201SpecialValue::Reserved };
    }

    // Normal mapped value
    const double normalVal = params.reverseScale * (static_cast<double>(val) - params.zeroOffset) + params.minVal;
    return Misb1201Result{ normalVal, Misb1201SpecialValue::Valid };
}

Misb1201Result Misb1201::decodeBytes(const std::uint8_t* data, std::size_t lengthBytes, double minVal, double maxVal) noexcept {
    if (data == nullptr || lengthBytes == 0U || lengthBytes > 8U) {
        return Misb1201Result{ 0.0, Misb1201SpecialValue::Reserved };
    }

    std::uint64_t rawInt { 0ULL };
    for (std::size_t i = 0U; i < lengthBytes; ++i) {
        rawInt = (rawInt << 8U) | static_cast<std::uint64_t>(data[i]);
    }

    return decode(rawInt, minVal, maxVal, lengthBytes);
}

} // namespace Klv
