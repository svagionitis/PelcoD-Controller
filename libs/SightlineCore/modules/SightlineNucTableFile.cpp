/// @file SightlineNucTableFile.cpp
/// @brief Implementation of the NUC / dead pixel table file codec (EAN Appendix A2 / A3).

#include "SightlineNucTableFile.h"

#include "../SightlineFraming.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace Sightline {

namespace {

    /// @brief NUC header: version, magic, height, width.
    constexpr std::size_t kNucHeader { 12U };
    /// @brief NUC per-pixel entry: u16 gain + s16 offset.
    constexpr std::size_t kNucEntry { 4U };
    /// @brief Dead header common to all versions: version, magic, count.
    constexpr std::size_t kDeadHeader { 12U };
    /// @brief Dead calculation parameters block (v2+): 2 x f32, 2 x s32, 2 x u16, u32, s32.
    constexpr std::size_t kDeadParams { 28U };
    /// @brief Dead v3 extension: s32 replaceMethod + u8 numReplace.
    constexpr std::size_t kDeadV3Extra { 5U };
    /// @brief Dead entry with replacement offsets (v1 / v2).
    constexpr std::size_t kDeadEntryOld { 6U };
    /// @brief Dead entry without replacement offsets (v3).
    constexpr std::size_t kDeadEntryV3 { 4U };
    /// @brief First dead table version with calculation parameters.
    constexpr std::uint32_t kDeadParamsVer { 2U };
    /// @brief Highest 0x35 deadReplace value (median).
    constexpr std::int32_t kMaxReplace { 2 };
    /// @brief numReplace range from the 0x35 IDD table.
    constexpr std::uint8_t kMinNumReplace { 1U };
    /// @brief numReplace range from the 0x35 IDD table.
    constexpr std::uint8_t kMaxNumReplace { 8U };
    /// @brief Q3.13 scale factor.
    constexpr double kGainScale { 8192.0 };
    /// @brief Exclusive upper bound of a Q3.13 gain.
    constexpr double kGainLimit { 8.0 };
    /// @brief Largest raw u16 value.
    constexpr double kU16Max { 65535.0 };

    /// @brief Returns the header size of a dead table version.
    /// @param[in] version Dead table version (1..3).
    /// @return Header size in bytes.
    std::size_t deadHeaderSize(std::uint32_t version) noexcept
    {
        std::size_t n { kDeadHeader };
        if (version >= kDeadParamsVer) {
            n += kDeadParams;
        }
        if (version >= kDeadFileVersion) {
            n += kDeadV3Extra;
        }
        return n;
    }

    /// @brief Returns the per-pixel entry size of a dead table version.
    /// @param[in] version Dead table version (1..3).
    /// @return Entry size in bytes.
    std::size_t deadEntrySize(std::uint32_t version) noexcept
    {
        return (version >= kDeadFileVersion) ? kDeadEntryV3 : kDeadEntryOld;
    }

    /// @brief Converts a raw byte to a signed 8-bit value without implementation-defined casts.
    /// @param[in] b Raw byte.
    /// @return Two's-complement interpretation of @p b.
    std::int8_t toS8(std::uint8_t b) noexcept
    {
        const std::int32_t v { static_cast<std::int32_t>(b) };
        return static_cast<std::int8_t>((v > 127) ? (v - 256) : v);
    }

    /// @brief Orders dead pixels by row, then column.
    /// @param[in] a First entry.
    /// @param[in] b Second entry.
    /// @return True if @p a precedes @p b.
    bool deadLess(const DeadPixelEntry& a, const DeadPixelEntry& b) noexcept
    {
        return (a.row != b.row) ? (a.row < b.row) : (a.col < b.col);
    }

    /// @brief Tests whether two dead pixels share coordinates.
    /// @param[in] a First entry.
    /// @param[in] b Second entry.
    /// @return True if row and column match.
    bool sameCoord(const DeadPixelEntry& a, const DeadPixelEntry& b) noexcept
    {
        return (a.row == b.row) && (a.col == b.col);
    }

    /// @brief Checks that a dead list is strictly ascending.
    /// @param[in] px Dead list.
    /// @return Ok, Duplicate or NotSorted.
    TableFileError checkList(const std::vector<DeadPixelEntry>& px) noexcept
    {
        for (std::size_t i { 1U }; i < px.size(); ++i) {
            if (sameCoord(px[i - 1U], px[i])) {
                return TableFileError::Duplicate;
            }
            if (!deadLess(px[i - 1U], px[i])) {
                return TableFileError::NotSorted;
            }
        }
        return TableFileError::Ok;
    }

    /// @brief Checks v3 replacement settings and that v3 entries carry no offsets.
    /// @param[in] t Dead table.
    /// @return Ok or BadValue.
    TableFileError checkReplace(const DeadTable& t) noexcept
    {
        if (t.version < kDeadFileVersion) {
            return TableFileError::Ok;
        }
        if ((t.replaceMethod < 0) || (t.replaceMethod > kMaxReplace)) {
            return TableFileError::BadValue;
        }
        if ((t.numReplace < kMinNumReplace) || (t.numReplace > kMaxNumReplace)) {
            return TableFileError::BadValue;
        }
        for (const DeadPixelEntry& e : t.pixels) {
            if ((e.rowOff != 0) || (e.colOff != 0)) {
                return TableFileError::BadValue;
            }
        }
        return TableFileError::Ok;
    }

    /// @brief Checks a buffer is exactly the expected size.
    /// @param[in] actual Buffer size.
    /// @param[in] expected Required size.
    /// @return Ok, Truncated or TrailingData.
    TableFileError checkExact(std::size_t actual, std::size_t expected) noexcept
    {
        if (actual < expected) {
            return TableFileError::Truncated;
        }
        return (actual > expected) ? TableFileError::TrailingData : TableFileError::Ok;
    }

    /// @brief Reads the calculation parameter block.
    /// @param[in] p Pointer to at least kDeadParams readable bytes.
    /// @return Decoded parameters.
    DeadCalcParams readParams(const std::uint8_t* p) noexcept
    {
        DeadCalcParams r {};
        r.maxGain = SightlineFraming::readFloat32Le(p);
        r.minGain = SightlineFraming::readFloat32Le(&p[4]);
        r.maxOff = SightlineFraming::readS32Le(&p[8]);
        r.minOff = SightlineFraming::readS32Le(&p[12]);
        r.maxVal = SightlineFraming::readU16Le(&p[16]);
        r.minVal = SightlineFraming::readU16Le(&p[18]);
        r.maxStdDev = SightlineFraming::readU32Le(&p[20]);
        r.maxNumDead = SightlineFraming::readS32Le(&p[24]);
        return r;
    }

    /// @brief Appends the calculation parameter block.
    /// @param[in,out] buf Target buffer.
    /// @param[in] r Parameters.
    void appendParams(std::vector<std::uint8_t>& buf, const DeadCalcParams& r)
    {
        SightlineFraming::appendFloat32Le(buf, r.maxGain);
        SightlineFraming::appendFloat32Le(buf, r.minGain);
        SightlineFraming::appendS32Le(buf, r.maxOff);
        SightlineFraming::appendS32Le(buf, r.minOff);
        SightlineFraming::appendU16Le(buf, r.maxVal);
        SightlineFraming::appendU16Le(buf, r.minVal);
        SightlineFraming::appendU32Le(buf, r.maxStdDev);
        SightlineFraming::appendS32Le(buf, r.maxNumDead);
    }

} // namespace

TableFileError SightlineNucTableFile::parseNuc(ByteView data, NucTable& out)
{
    if (data.size() < kNucHeader) {
        return TableFileError::Truncated;
    }
    const std::uint8_t* const p { data.data() };
    if (SightlineFraming::readU32Le(&p[4]) != kNucFileMagic) {
        return TableFileError::BadMagic;
    }
    const std::uint32_t version { SightlineFraming::readU32Le(p) };
    if (version != kNucFileVersion) {
        return TableFileError::BadVersion;
    }
    const std::int16_t height { SightlineFraming::readS16Le(&p[8]) };
    const std::int16_t width { SightlineFraming::readS16Le(&p[10]) };
    if ((height <= 0) || (width <= 0)) {
        return TableFileError::BadSize;
    }
    const std::size_t count { static_cast<std::size_t>(height) * static_cast<std::size_t>(width) };
    if (count > kMaxTablePixels) {
        return TableFileError::TooLarge;
    }
    const TableFileError sz { checkExact(data.size(), kNucHeader + (count * kNucEntry)) };
    if (sz != TableFileError::Ok) {
        return sz;
    }

    NucTable t {};
    t.version = version;
    t.height = height;
    t.width = width;
    t.pixels.reserve(count);
    for (std::size_t i { 0U }; i < count; ++i) {
        const std::size_t off { kNucHeader + (i * kNucEntry) };
        const NucGainOffset px { SightlineFraming::readU16Le(&p[off]), SightlineFraming::readS16Le(&p[off + 2U]) };
        t.pixels.push_back(px);
    }
    out = std::move(t);
    return TableFileError::Ok;
}

TableFileError SightlineNucTableFile::writeNuc(const NucTable& table, std::vector<std::uint8_t>& out)
{
    if (table.version != kNucFileVersion) {
        return TableFileError::BadVersion;
    }
    if ((table.height <= 0) || (table.width <= 0)) {
        return TableFileError::BadSize;
    }
    const std::size_t count { static_cast<std::size_t>(table.height) * static_cast<std::size_t>(table.width) };
    if (count > kMaxTablePixels) {
        return TableFileError::TooLarge;
    }
    if (table.pixels.size() != count) {
        return TableFileError::BadSize;
    }

    std::vector<std::uint8_t> buf {};
    buf.reserve(kNucHeader + (count * kNucEntry));
    SightlineFraming::appendU32Le(buf, table.version);
    SightlineFraming::appendU32Le(buf, kNucFileMagic);
    SightlineFraming::appendS16Le(buf, table.height);
    SightlineFraming::appendS16Le(buf, table.width);
    for (const NucGainOffset& px : table.pixels) {
        SightlineFraming::appendU16Le(buf, px.gain);
        SightlineFraming::appendS16Le(buf, px.offset);
    }
    out = std::move(buf);
    return TableFileError::Ok;
}

TableFileError SightlineNucTableFile::parseDead(ByteView data, DeadTable& out)
{
    if (data.size() < kDeadHeader) {
        return TableFileError::Truncated;
    }
    const std::uint8_t* const p { data.data() };
    if (SightlineFraming::readU32Le(&p[4]) != kNucFileMagic) {
        return TableFileError::BadMagic;
    }
    const std::uint32_t version { SightlineFraming::readU32Le(p) };
    if ((version == 0U) || (version > kDeadFileVersion)) {
        return TableFileError::BadVersion;
    }
    const std::int32_t rawCount { SightlineFraming::readS32Le(&p[8]) };
    if (rawCount < 0) {
        return TableFileError::BadSize;
    }
    const std::size_t count { static_cast<std::size_t>(rawCount) };
    if (count > kMaxTablePixels) {
        return TableFileError::TooLarge;
    }
    const std::size_t hdr { deadHeaderSize(version) };
    const std::size_t entry { deadEntrySize(version) };
    if (data.size() < hdr) {
        return TableFileError::Truncated;
    }
    const TableFileError sz { checkExact(data.size(), hdr + (count * entry)) };
    if (sz != TableFileError::Ok) {
        return sz;
    }

    DeadTable t {};
    t.version = version;
    std::size_t off { kDeadHeader };
    if (version >= kDeadParamsVer) {
        t.params = readParams(&p[off]);
        off += kDeadParams;
    }
    if (version >= kDeadFileVersion) {
        t.replaceMethod = SightlineFraming::readS32Le(&p[off]);
        t.numReplace = p[off + 4U];
        off += kDeadV3Extra;
    }
    t.pixels.reserve(count);
    for (std::size_t i { 0U }; i < count; ++i) {
        DeadPixelEntry e {};
        e.row = SightlineFraming::readU16Le(&p[off]);
        e.col = SightlineFraming::readU16Le(&p[off + 2U]);
        if (version < kDeadFileVersion) {
            e.rowOff = toS8(p[off + 4U]);
            e.colOff = toS8(p[off + 5U]);
        }
        t.pixels.push_back(e);
        off += entry;
    }

    const TableFileError rep { checkReplace(t) };
    if (rep != TableFileError::Ok) {
        return rep;
    }
    const TableFileError order { checkList(t.pixels) };
    if (order != TableFileError::Ok) {
        return order;
    }
    out = std::move(t);
    return TableFileError::Ok;
}

TableFileError SightlineNucTableFile::writeDead(const DeadTable& table, std::vector<std::uint8_t>& out)
{
    if ((table.version == 0U) || (table.version > kDeadFileVersion)) {
        return TableFileError::BadVersion;
    }
    if (table.pixels.size() > kMaxTablePixels) {
        return TableFileError::TooLarge;
    }
    const TableFileError rep { checkReplace(table) };
    if (rep != TableFileError::Ok) {
        return rep;
    }
    const TableFileError order { checkList(table.pixels) };
    if (order != TableFileError::Ok) {
        return order;
    }

    const std::size_t entry { deadEntrySize(table.version) };
    std::vector<std::uint8_t> buf {};
    buf.reserve(deadHeaderSize(table.version) + (table.pixels.size() * entry));
    SightlineFraming::appendU32Le(buf, table.version);
    SightlineFraming::appendU32Le(buf, kNucFileMagic);
    SightlineFraming::appendS32Le(buf, static_cast<std::int32_t>(table.pixels.size()));
    if (table.version >= kDeadParamsVer) {
        appendParams(buf, table.params);
    }
    if (table.version >= kDeadFileVersion) {
        SightlineFraming::appendS32Le(buf, table.replaceMethod);
        buf.push_back(table.numReplace);
    }
    for (const DeadPixelEntry& e : table.pixels) {
        SightlineFraming::appendU16Le(buf, e.row);
        SightlineFraming::appendU16Le(buf, e.col);
        if (table.version < kDeadFileVersion) {
            buf.push_back(static_cast<std::uint8_t>(e.rowOff));
            buf.push_back(static_cast<std::uint8_t>(e.colOff));
        }
    }
    out = std::move(buf);
    return TableFileError::Ok;
}

bool SightlineNucTableFile::isSorted(const std::vector<DeadPixelEntry>& pixels) noexcept
{
    return checkList(pixels) == TableFileError::Ok;
}

std::size_t SightlineNucTableFile::sortDead(std::vector<DeadPixelEntry>& pixels)
{
    const std::size_t before { pixels.size() };
    std::stable_sort(pixels.begin(), pixels.end(), deadLess);
    const auto last { std::unique(pixels.begin(), pixels.end(), sameCoord) };
    static_cast<void>(pixels.erase(last, pixels.end()));
    return before - pixels.size();
}

double SightlineNucTableFile::gainToReal(std::uint16_t raw) noexcept
{
    return static_cast<double>(raw) / kGainScale;
}

bool SightlineNucTableFile::gainFromReal(double gain, std::uint16_t& raw) noexcept
{
    if ((!std::isfinite(gain)) || (gain < 0.0) || (gain >= kGainLimit)) {
        return false;
    }
    const double scaled { std::round(gain * kGainScale) };
    if (scaled > kU16Max) {
        return false;
    }
    raw = static_cast<std::uint16_t>(scaled);
    return true;
}

const char* SightlineNucTableFile::errorText(TableFileError err) noexcept
{
    switch (err) {
    case TableFileError::Ok:
        return "OK";
    case TableFileError::Truncated:
        return "File is truncated";
    case TableFileError::BadMagic:
        return "Bad magic number (expected 0x51ACD00D)";
    case TableFileError::BadVersion:
        return "Unsupported table version";
    case TableFileError::BadSize:
        return "Invalid dimensions or pixel count";
    case TableFileError::TrailingData:
        return "Unexpected data after table";
    case TableFileError::NotSorted:
        return "Dead list not sorted by row, then column";
    case TableFileError::Duplicate:
        return "Duplicate dead pixel";
    case TableFileError::BadValue:
        return "Field out of range";
    case TableFileError::TooLarge:
        return "Table too large";
    default:
        break;
    }
    return "Unknown error";
}

} // namespace Sightline
