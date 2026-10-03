/// @file TransactionalStorageWriter.cpp
/// @brief Implementation of crash-resilient transactional storage writer and fMP4 box builders.

#include "TransactionalStorageWriter.h"

#include <filesystem>
#include <utility>

#ifdef _WIN32
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Sightline {

namespace {

void writeUint32Be(std::vector<std::uint8_t>& buf, std::uint32_t val)
{
    buf.push_back(static_cast<std::uint8_t>((val >> 24U) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

void writeFourCC(std::vector<std::uint8_t>& buf, const char code[4])
{
    buf.push_back(static_cast<std::uint8_t>(code[0]));
    buf.push_back(static_cast<std::uint8_t>(code[1]));
    buf.push_back(static_cast<std::uint8_t>(code[2]));
    buf.push_back(static_cast<std::uint8_t>(code[3]));
}

} // namespace

TransactionalStorageWriter::TransactionalStorageWriter() = default;

TransactionalStorageWriter::~TransactionalStorageWriter()
{
    abort();
}

TransactionalStorageWriter::TransactionalStorageWriter(TransactionalStorageWriter&& other) noexcept
    : m_destinationPath(std::move(other.m_destinationPath))
    , m_stagingPath(std::move(other.m_stagingPath))
    , m_fileHandle(other.m_fileHandle)
    , m_bytesWritten(other.m_bytesWritten)
{
    other.m_fileHandle = nullptr;
    other.m_bytesWritten = 0ULL;
}

TransactionalStorageWriter& TransactionalStorageWriter::operator=(TransactionalStorageWriter&& other) noexcept
{
    if (this != &other) {
        abort();
        m_destinationPath = std::move(other.m_destinationPath);
        m_stagingPath = std::move(other.m_stagingPath);
        m_fileHandle = other.m_fileHandle;
        m_bytesWritten = other.m_bytesWritten;

        other.m_fileHandle = nullptr;
        other.m_bytesWritten = 0ULL;
    }
    return *this;
}

bool TransactionalStorageWriter::startTransaction(std::string_view destinationPath)
{
    abort(); // Ensure any previously pending transaction is cleaned up

    if (destinationPath.empty()) {
        return false;
    }

    m_destinationPath = std::string(destinationPath);
    m_stagingPath = m_destinationPath + ".tmp";

    // Ensure parent directories exist
    std::error_code ec {};
    const std::filesystem::path p(m_stagingPath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    // Open binary file for writing
#ifdef _WIN32
    errno_t err = fopen_s(&m_fileHandle, m_stagingPath.c_str(), "wb");
    if (err != 0 || !m_fileHandle) {
        m_fileHandle = nullptr;
        return false;
    }
#else
    m_fileHandle = std::fopen(m_stagingPath.c_str(), "wb");
    if (!m_fileHandle) {
        return false;
    }
#endif

    m_bytesWritten = 0ULL;
    return true;
}

bool TransactionalStorageWriter::writeChunk(const std::uint8_t* data, std::size_t size)
{
    if (!m_fileHandle || !data || size == 0U) {
        return false;
    }

    const std::size_t written = std::fwrite(data, 1U, size, m_fileHandle);
    if (written != size) {
        return false;
    }

    m_bytesWritten += written;
    return true;
}

bool TransactionalStorageWriter::writeChunk(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return true;
    }
    return writeChunk(data.data(), data.size());
}

bool TransactionalStorageWriter::syncBarrier()
{
    if (!m_fileHandle) {
        return false;
    }

    if (std::fflush(m_fileHandle) != 0) {
        return false;
    }

#ifdef _WIN32
    const int fd = _fileno(m_fileHandle);
    if (fd >= 0) {
        const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
        if (handle != INVALID_HANDLE_VALUE) {
            return FlushFileBuffers(handle) != FALSE;
        }
    }
    return true;
#else
    const int fd = fileno(m_fileHandle);
    if (fd >= 0) {
        return fsync(fd) == 0;
    }
    return true;
#endif
}

bool TransactionalStorageWriter::commit()
{
    if (!m_fileHandle) {
        return false;
    }

    // Perform sync barrier before closing
    (void)syncBarrier();

    std::fclose(m_fileHandle);
    m_fileHandle = nullptr;

    // Atomically rename staging file to final destination
    std::error_code ec {};
#ifdef _WIN32
    if (MoveFileExA(
            m_stagingPath.c_str(),
            m_destinationPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE)
    {
        // Fallback using std::filesystem
        std::filesystem::remove(m_destinationPath, ec);
        std::filesystem::rename(m_stagingPath, m_destinationPath, ec);
        if (ec) {
            return false;
        }
    }
#else
    if (std::rename(m_stagingPath.c_str(), m_destinationPath.c_str()) != 0) {
        std::filesystem::remove(m_destinationPath, ec);
        std::filesystem::rename(m_stagingPath, m_destinationPath, ec);
        if (ec) {
            return false;
        }
    }
#endif

    m_stagingPath.clear();
    m_destinationPath.clear();
    return true;
}

void TransactionalStorageWriter::abort()
{
    if (m_fileHandle) {
        std::fclose(m_fileHandle);
        m_fileHandle = nullptr;
    }

    if (!m_stagingPath.empty()) {
        std::error_code ec {};
        std::filesystem::remove(m_stagingPath, ec);
        m_stagingPath.clear();
    }

    m_destinationPath.clear();
    m_bytesWritten = 0ULL;
}

bool TransactionalStorageWriter::isActive() const noexcept
{
    return m_fileHandle != nullptr;
}

std::string TransactionalStorageWriter::stagingPath() const
{
    return m_stagingPath;
}

std::string TransactionalStorageWriter::destinationPath() const
{
    return m_destinationPath;
}

std::uint64_t TransactionalStorageWriter::bytesWritten() const noexcept
{
    return m_bytesWritten;
}

std::vector<std::uint8_t> TransactionalStorageWriter::buildFtypBox()
{
    std::vector<std::uint8_t> box {};
    box.reserve(32U);

    writeUint32Be(box, 32U); // Box total length
    writeFourCC(box, "ftyp");
    writeFourCC(box, "isom"); // Major brand
    writeUint32Be(box, 512U); // Minor version

    // Compatible brands
    writeFourCC(box, "isom");
    writeFourCC(box, "iso2");
    writeFourCC(box, "mp41");

    return box;
}

std::vector<std::uint8_t> TransactionalStorageWriter::buildMoovHeader(
    std::uint32_t width, std::uint32_t height, std::uint32_t timescale)
{
    // Minimal valid moov declaring fragmented movie (mvex + trex)
    std::vector<std::uint8_t> box {};
    box.reserve(160U);

    // Placeholder for moov box size
    const std::size_t moovStart = box.size();
    writeUint32Be(box, 0U);
    writeFourCC(box, "moov");

    // --- mvhd box (Movie Header Box) ---
    writeUint32Be(box, 108U);
    writeFourCC(box, "mvhd");
    box.push_back(0U); // version 0
    box.push_back(0U); box.push_back(0U); box.push_back(0U); // flags
    writeUint32Be(box, 0U); // creation time
    writeUint32Be(box, 0U); // modification time
    writeUint32Be(box, timescale); // timescale
    writeUint32Be(box, 0U); // duration (0 in fragmented MP4)
    writeUint32Be(box, 0x00010000U); // rate 1.0
    writeUint32Be(box, 0x01000000U); // volume 1.0 (fixed 8.8)
    for (std::size_t i = 0; i < 10; ++i) {
        box.push_back(0U); // reserved
    }
    // Unity matrix (3x3 fixed point 16.16)
    writeUint32Be(box, 0x00010000U); writeUint32Be(box, 0U); writeUint32Be(box, 0U);
    writeUint32Be(box, 0U); writeUint32Be(box, 0x00010000U); writeUint32Be(box, 0U);
    writeUint32Be(box, 0U); writeUint32Be(box, 0U); writeUint32Be(box, 0x40000000U);
    for (std::size_t i = 0; i < 24; ++i) {
        box.push_back(0U); // pre-defined
    }
    writeUint32Be(box, 2U); // next_track_ID (track 1 active)

    // --- mvex box (Movie Extends Box) ---
    writeUint32Be(box, 40U);
    writeFourCC(box, "mvex");

    // trex box (Track Extends Box)
    writeUint32Be(box, 32U);
    writeFourCC(box, "trex");
    box.push_back(0U); // version
    box.push_back(0U); box.push_back(0U); box.push_back(0U); // flags
    writeUint32Be(box, 1U); // track_ID = 1
    writeUint32Be(box, 1U); // default_sample_description_index = 1
    writeUint32Be(box, 3000U); // default_sample_duration
    writeUint32Be(box, 0U); // default_sample_size
    writeUint32Be(box, 0x01010000U); // default_sample_flags

    // Finalize total moov length
    const auto moovTotal = static_cast<std::uint32_t>(box.size() - moovStart);
    box[moovStart] = static_cast<std::uint8_t>((moovTotal >> 24U) & 0xFFU);
    box[moovStart + 1U] = static_cast<std::uint8_t>((moovTotal >> 16U) & 0xFFU);
    box[moovStart + 2U] = static_cast<std::uint8_t>((moovTotal >> 8U) & 0xFFU);
    box[moovStart + 3U] = static_cast<std::uint8_t>(moovTotal & 0xFFU);

    (void)width;
    (void)height;

    return box;
}

std::vector<std::uint8_t> TransactionalStorageWriter::buildFragment(
    std::uint32_t seq, const std::vector<std::uint8_t>& payload, std::uint32_t durationTicks)
{
    std::vector<std::uint8_t> fragment {};
    fragment.reserve(128U + payload.size());

    // --- moof (Movie Fragment Box) ---
    const std::size_t moofStart = fragment.size();
    writeUint32Be(fragment, 0U); // moof size placeholder
    writeFourCC(fragment, "moof");

    // mfhd (Movie Fragment Header)
    writeUint32Be(fragment, 16U);
    writeFourCC(fragment, "mfhd");
    fragment.push_back(0U); // version
    fragment.push_back(0U); fragment.push_back(0U); fragment.push_back(0U); // flags
    writeUint32Be(fragment, seq); // sequence_number

    // traf (Track Fragment Box)
    const std::size_t trafStart = fragment.size();
    writeUint32Be(fragment, 0U); // traf size placeholder
    writeFourCC(fragment, "traf");

    // tfhd (Track Fragment Header)
    writeUint32Be(fragment, 16U);
    writeFourCC(fragment, "tfhd");
    fragment.push_back(0U); // version
    fragment.push_back(0U); fragment.push_back(0U); fragment.push_back(0x02U); // flags (default_sample_duration_present)
    writeUint32Be(fragment, 1U); // track_ID = 1
    writeUint32Be(fragment, durationTicks); // default sample duration

    // trun (Track Run Box)
    writeUint32Be(fragment, 24U);
    writeFourCC(fragment, "trun");
    fragment.push_back(0U); // version
    fragment.push_back(0U); fragment.push_back(0x02U); fragment.push_back(0x01U); // data_offset_present | first_sample_flags_present
    writeUint32Be(fragment, 1U); // sample_count = 1
    // data_offset placeholder: computed after moof size is known
    const std::size_t dataOffsetPos = fragment.size();
    writeUint32Be(fragment, 0U);
    writeUint32Be(fragment, 0x02000000U); // first_sample_flags (sync/IDR)

    // Finalize traf size
    const auto trafTotal = static_cast<std::uint32_t>(fragment.size() - trafStart);
    fragment[trafStart] = static_cast<std::uint8_t>((trafTotal >> 24U) & 0xFFU);
    fragment[trafStart + 1U] = static_cast<std::uint8_t>((trafTotal >> 16U) & 0xFFU);
    fragment[trafStart + 2U] = static_cast<std::uint8_t>((trafTotal >> 8U) & 0xFFU);
    fragment[trafStart + 3U] = static_cast<std::uint8_t>(trafTotal & 0xFFU);

    // Finalize moof size
    const auto moofTotal = static_cast<std::uint32_t>(fragment.size() - moofStart);
    fragment[moofStart] = static_cast<std::uint8_t>((moofTotal >> 24U) & 0xFFU);
    fragment[moofStart + 1U] = static_cast<std::uint8_t>((moofTotal >> 16U) & 0xFFU);
    fragment[moofStart + 2U] = static_cast<std::uint8_t>((moofTotal >> 8U) & 0xFFU);
    fragment[moofStart + 3U] = static_cast<std::uint8_t>(moofTotal & 0xFFU);

    // Update trun data_offset: distance from start of moof to start of media data
    // mdat header is 8 bytes, so offset = moofTotal + 8
    const std::uint32_t dataOffset = moofTotal + 8U;
    fragment[dataOffsetPos] = static_cast<std::uint8_t>((dataOffset >> 24U) & 0xFFU);
    fragment[dataOffsetPos + 1U] = static_cast<std::uint8_t>((dataOffset >> 16U) & 0xFFU);
    fragment[dataOffsetPos + 2U] = static_cast<std::uint8_t>((dataOffset >> 8U) & 0xFFU);
    fragment[dataOffsetPos + 3U] = static_cast<std::uint8_t>(dataOffset & 0xFFU);

    // --- mdat (Media Data Box) ---
    const auto mdatTotal = static_cast<std::uint32_t>(8U + payload.size());
    writeUint32Be(fragment, mdatTotal);
    writeFourCC(fragment, "mdat");
    fragment.insert(fragment.end(), payload.begin(), payload.end());

    return fragment;
}

} // namespace Sightline
