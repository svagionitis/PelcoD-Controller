#include "MiisCoreId.h"
#include "KlvTypes.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <random>

namespace Klv {

namespace {

    /// @brief Helper to convert a single hex character to its 4-bit value.
    [[nodiscard]] int hexCharToInt(char c) noexcept
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return (c - 'a') + 10;
        if (c >= 'A' && c <= 'F')
            return (c - 'A') + 10;
        return -1;
    }

    /// @brief Lightweight SHA-1 implementation conforming to RFC 3174 / FIPS 180-1.
    class Sha1Hasher {
    public:
        Sha1Hasher() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            m_state[0] = 0x67452301U;
            m_state[1] = 0xEFCDAB89U;
            m_state[2] = 0x98BADCFEU;
            m_state[3] = 0x10325476U;
            m_state[4] = 0xC3D2E1F0U;
            m_byteCount = 0U;
            m_buffer.fill(0U);
        }

        void update(const std::uint8_t* data, std::size_t len) noexcept
        {
            for (std::size_t i = 0U; i < len; ++i) {
                const std::size_t bufferIdx = m_byteCount % 64U;
                m_buffer[bufferIdx] = data[i];
                ++m_byteCount;
                if (bufferIdx == 63U) {
                    transform(m_buffer.data());
                }
            }
        }

        [[nodiscard]] std::array<std::uint8_t, 20> finalize() noexcept
        {
            const std::uint64_t totalBits = m_byteCount * 8U;
            const std::size_t bufferIdx = m_byteCount % 64U;

            m_buffer[bufferIdx] = 0x80U;
            if (bufferIdx < 56U) {
                std::fill(m_buffer.begin() + static_cast<std::ptrdiff_t>(bufferIdx + 1U), m_buffer.begin() + 56,
                    std::uint8_t { 0U });
            } else {
                std::fill(m_buffer.begin() + static_cast<std::ptrdiff_t>(bufferIdx + 1U), m_buffer.end(),
                    std::uint8_t { 0U });
                transform(m_buffer.data());
                m_buffer.fill(0U);
            }

            for (std::size_t i = 0U; i < 8U; ++i) {
                m_buffer[56U + i] = static_cast<std::uint8_t>((totalBits >> ((7U - i) * 8U)) & 0xFFU);
            }
            transform(m_buffer.data());

            std::array<std::uint8_t, 20> digest {};
            for (std::size_t i = 0U; i < 5U; ++i) {
                digest[i * 4U] = static_cast<std::uint8_t>((m_state[i] >> 24U) & 0xFFU);
                digest[i * 4U + 1U] = static_cast<std::uint8_t>((m_state[i] >> 16U) & 0xFFU);
                digest[i * 4U + 2U] = static_cast<std::uint8_t>((m_state[i] >> 8U) & 0xFFU);
                digest[i * 4U + 3U] = static_cast<std::uint8_t>(m_state[i] & 0xFFU);
            }
            return digest;
        }

    private:
        std::array<std::uint32_t, 5> m_state {};
        std::uint64_t m_byteCount { 0U };
        std::array<std::uint8_t, 64> m_buffer {};

        [[nodiscard]] static std::uint32_t leftRotate(std::uint32_t val, std::uint32_t bits) noexcept
        {
            return (val << bits) | (val >> (32U - bits));
        }

        void transform(const std::uint8_t* block) noexcept
        {
            std::array<std::uint32_t, 80> w {};
            for (std::size_t t = 0U; t < 16U; ++t) {
                w[t] = (static_cast<std::uint32_t>(block[t * 4U]) << 24U)
                    | (static_cast<std::uint32_t>(block[t * 4U + 1U]) << 16U)
                    | (static_cast<std::uint32_t>(block[t * 4U + 2U]) << 8U)
                    | static_cast<std::uint32_t>(block[t * 4U + 3U]);
            }
            for (std::size_t t = 16U; t < 80U; ++t) {
                w[t] = leftRotate(w[t - 3U] ^ w[t - 8U] ^ w[t - 14U] ^ w[t - 16U], 1U);
            }

            std::uint32_t a = m_state[0];
            std::uint32_t b = m_state[1];
            std::uint32_t c = m_state[2];
            std::uint32_t d = m_state[3];
            std::uint32_t e = m_state[4];

            for (std::size_t t = 0U; t < 80U; ++t) {
                std::uint32_t f = 0U;
                std::uint32_t k = 0U;
                if (t < 20U) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999U;
                } else if (t < 40U) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1U;
                } else if (t < 60U) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDCU;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6U;
                }
                const std::uint32_t temp = leftRotate(a, 5U) + f + e + k + w[t];
                e = d;
                d = c;
                c = leftRotate(b, 30U);
                b = a;
                a = temp;
            }

            m_state[0] += a;
            m_state[1] += b;
            m_state[2] += c;
            m_state[3] += d;
            m_state[4] += e;
        }
    };

} // namespace

// ==============================================================================
// Uuid Implementation
// ==============================================================================

bool Uuid::isNull() const noexcept
{
    return std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t b) noexcept { return b == 0U; });
}

bool Uuid::isValidVariant() const noexcept
{
    // RFC 4122 Variant: most significant bits of octet 8 are 10xx (0x80..0xBF)
    return (bytes[8] & 0xC0U) == 0x80U;
}

std::uint8_t Uuid::version() const noexcept
{
    return static_cast<std::uint8_t>((bytes[6] >> 4U) & 0x0FU);
}

std::string Uuid::toString(bool uppercase) const
{
    char buf[40] {};
    if (uppercase) {
        std::snprintf(buf, sizeof(buf), "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9],
            bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    } else {
        std::snprintf(buf, sizeof(buf), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9],
            bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    }
    return std::string(buf);
}

std::string Uuid::toUrn() const
{
    return "urn:uuid:" + toString(false);
}

bool Uuid::fromString(std::string_view str, Uuid& out) noexcept
{
    std::string_view s = str;
    if (s.rfind("urn:uuid:", 0U) == 0U || s.rfind("URN:UUID:", 0U) == 0U) {
        s.remove_prefix(9U);
    }

    std::array<std::uint8_t, 16> parsed {};
    std::size_t byteIdx = 0U;
    int highNibble = -1;

    for (char ch : s) {
        if (ch == '-') {
            continue;
        }
        const int val = hexCharToInt(ch);
        if (val < 0) {
            return false;
        }
        if (highNibble < 0) {
            highNibble = val;
        } else {
            if (byteIdx >= 16U) {
                return false; // Too many hex characters
            }
            parsed[byteIdx] = static_cast<std::uint8_t>((highNibble << 4) | val);
            ++byteIdx;
            highNibble = -1;
        }
    }

    if (byteIdx != 16U || highNibble >= 0) {
        return false;
    }

    out.bytes = parsed;
    return true;
}

Uuid Uuid::generateV4()
{
    std::random_device rd {};
    std::mt19937_64 gen { rd() };
    std::uniform_int_distribution<std::uint64_t> dist {};

    const std::uint64_t part1 = dist(gen);
    const std::uint64_t part2 = dist(gen);

    Uuid u {};
    for (std::size_t i = 0U; i < 8U; ++i) {
        u.bytes[i] = static_cast<std::uint8_t>((part1 >> ((7U - i) * 8U)) & 0xFFU);
        u.bytes[8U + i] = static_cast<std::uint8_t>((part2 >> ((7U - i) * 8U)) & 0xFFU);
    }

    // Version 4: bits 12..15 of octet 6 = 0100
    u.bytes[6] = static_cast<std::uint8_t>((u.bytes[6] & 0x0FU) | 0x40U);
    // Variant 1: bits 6..7 of octet 8 = 10
    u.bytes[8] = static_cast<std::uint8_t>((u.bytes[8] & 0x3FU) | 0x80U);

    return u;
}

Uuid Uuid::generateV5(const Uuid& ns, std::string_view name)
{
    Sha1Hasher hasher {};
    hasher.update(ns.bytes.data(), ns.bytes.size());
    hasher.update(reinterpret_cast<const std::uint8_t*>(name.data()), name.size());
    const auto digest = hasher.finalize();

    Uuid u {};
    std::copy_n(digest.begin(), 16U, u.bytes.begin());

    // Version 5: bits 12..15 of octet 6 = 0101
    u.bytes[6] = static_cast<std::uint8_t>((u.bytes[6] & 0x0FU) | 0x50U);
    // Variant 1: bits 6..7 of octet 8 = 10
    u.bytes[8] = static_cast<std::uint8_t>((u.bytes[8] & 0x3FU) | 0x80U);

    return u;
}

Uuid Uuid::generateV1(
    const std::array<std::uint8_t, 6>& mac, std::uint64_t timestamp100ns, std::uint16_t clockSeq) noexcept
{
    Uuid u {};

    // time_low (bytes 0..3)
    u.bytes[0] = static_cast<std::uint8_t>((timestamp100ns >> 24U) & 0xFFU);
    u.bytes[1] = static_cast<std::uint8_t>((timestamp100ns >> 16U) & 0xFFU);
    u.bytes[2] = static_cast<std::uint8_t>((timestamp100ns >> 8U) & 0xFFU);
    u.bytes[3] = static_cast<std::uint8_t>(timestamp100ns & 0xFFU);

    // time_mid (bytes 4..5)
    u.bytes[4] = static_cast<std::uint8_t>((timestamp100ns >> 40U) & 0xFFU);
    u.bytes[5] = static_cast<std::uint8_t>((timestamp100ns >> 32U) & 0xFFU);

    // time_hi_and_version (bytes 6..7)
    u.bytes[6] = static_cast<std::uint8_t>(((timestamp100ns >> 56U) & 0x0FU) | 0x10U); // Version 1
    u.bytes[7] = static_cast<std::uint8_t>((timestamp100ns >> 48U) & 0xFFU);

    // clock_seq_hi_and_reserved & clock_seq_low (bytes 8..9)
    u.bytes[8] = static_cast<std::uint8_t>(((clockSeq >> 8U) & 0x3FU) | 0x80U); // RFC 4122 variant
    u.bytes[9] = static_cast<std::uint8_t>(clockSeq & 0xFFU);

    // node (bytes 10..15)
    std::copy(mac.begin(), mac.end(), u.bytes.begin() + 10);

    return u;
}

// ==============================================================================
// MiisCoreId Implementation
// ==============================================================================

MiisCoreId::MiisCoreId(const Uuid& id) noexcept
    : streamId(id)
{
    sensorId = MiisSubId { MiisIdType::Sensor, MiisIdQuality::Physical, id };
}

MiisCoreId::MiisCoreId(
    const Uuid& sensor, const Uuid& platform, MiisIdQuality sensorQuality, MiisIdQuality platformQuality) noexcept
{
    sensorId = MiisSubId { MiisIdType::Sensor, sensorQuality, sensor };
    platformId = MiisSubId { MiisIdType::Platform, platformQuality, platform };
}

bool MiisCoreId::validate() const noexcept
{
    if (sensorId) {
        if (sensorId->uuid.isNull())
            return false;
        if (!sensorId->uuid.isValidVariant())
            return false;
    }
    if (platformId) {
        if (platformId->uuid.isNull())
            return false;
        if (!platformId->uuid.isValidVariant())
            return false;
    }
    if (streamId) {
        if (streamId->isNull())
            return false;
        if (!streamId->isValidVariant())
            return false;
    }
    return sensorId.has_value() || platformId.has_value() || streamId.has_value();
}

std::vector<std::uint8_t> MiisCoreId::encode(bool includeQuality) const
{
    std::vector<std::uint8_t> out {};

    if (sensorId && platformId) {
        if (includeQuality) {
            // 35-byte Versioned Quality Composite
            out.reserve(35U);
            out.push_back(version);
            out.push_back(static_cast<std::uint8_t>(sensorId->quality));
            out.insert(out.end(), sensorId->uuid.bytes.begin(), sensorId->uuid.bytes.end());
            out.push_back(static_cast<std::uint8_t>(platformId->quality));
            out.insert(out.end(), platformId->uuid.bytes.begin(), platformId->uuid.bytes.end());
        } else {
            // 33-byte Versioned Composite (ST 1204 standard)
            out.reserve(33U);
            out.push_back(version);
            out.insert(out.end(), sensorId->uuid.bytes.begin(), sensorId->uuid.bytes.end());
            out.insert(out.end(), platformId->uuid.bytes.begin(), platformId->uuid.bytes.end());
        }
    } else if (sensorId) {
        // 16-byte raw UUID
        out.insert(out.end(), sensorId->uuid.bytes.begin(), sensorId->uuid.bytes.end());
    } else if (streamId) {
        // 16-byte raw UUID
        out.insert(out.end(), streamId->bytes.begin(), streamId->bytes.end());
    }

    return out;
}

KlvStatus MiisCoreId::decode(const std::uint8_t* data, std::size_t length) noexcept
{
    if (data == nullptr || length == 0U) {
        return KlvStatus::BufferUnderflow;
    }

    if (length == 16U) {
        // 16-byte raw UUID
        Uuid u {};
        std::copy_n(data, 16U, u.bytes.begin());
        streamId = u;
        sensorId = MiisSubId { MiisIdType::Sensor, MiisIdQuality::Physical, u };
        platformId.reset();
        return KlvStatus::Success;
    }

    if (length == 32U) {
        // 32-byte Dual UUID: Sensor (16B) + Platform (16B)
        version = 1U;
        Uuid sUuid {};
        std::copy_n(data, 16U, sUuid.bytes.begin());
        Uuid pUuid {};
        std::copy_n(data + 16U, 16U, pUuid.bytes.begin());
        sensorId = MiisSubId { MiisIdType::Sensor, MiisIdQuality::Physical, sUuid };
        platformId = MiisSubId { MiisIdType::Platform, MiisIdQuality::Physical, pUuid };
        streamId.reset();
        return KlvStatus::Success;
    }

    if (length == 33U) {
        // 33-byte Versioned Composite: Version (1B) + Sensor (16B) + Platform (16B)
        version = data[0];
        Uuid sUuid {};
        std::copy_n(data + 1U, 16U, sUuid.bytes.begin());
        Uuid pUuid {};
        std::copy_n(data + 17U, 16U, pUuid.bytes.begin());
        sensorId = MiisSubId { MiisIdType::Sensor, MiisIdQuality::Physical, sUuid };
        platformId = MiisSubId { MiisIdType::Platform, MiisIdQuality::Physical, pUuid };
        streamId.reset();
        return KlvStatus::Success;
    }

    if (length == 35U) {
        // 35-byte Versioned Quality Composite
        version = data[0];
        const auto sQual = static_cast<MiisIdQuality>(data[1]);
        Uuid sUuid {};
        std::copy_n(data + 2U, 16U, sUuid.bytes.begin());

        const auto pQual = static_cast<MiisIdQuality>(data[18]);
        Uuid pUuid {};
        std::copy_n(data + 19U, 16U, pUuid.bytes.begin());

        sensorId = MiisSubId { MiisIdType::Sensor, sQual, sUuid };
        platformId = MiisSubId { MiisIdType::Platform, pQual, pUuid };
        streamId.reset();
        return KlvStatus::Success;
    }

    // Fallback: try parsing text representation if length matches UUID / URN
    std::string text(reinterpret_cast<const char*>(data), length);
    if (fromString(text)) {
        return KlvStatus::Success;
    }

    return KlvStatus::TagError;
}

std::string MiisCoreId::toString() const
{
    if (sensorId && platformId) {
        return sensorId->uuid.toString() + ":" + platformId->uuid.toString();
    }
    if (sensorId) {
        return sensorId->uuid.toString();
    }
    if (streamId) {
        return streamId->toString();
    }
    return {};
}

std::string MiisCoreId::toUrn() const
{
    return "urn:uuid:" + primaryUuid().toString();
}

bool MiisCoreId::fromString(std::string_view str) noexcept
{
    // Check for composite format: "UUID:UUID"
    const auto colonPos = str.find(':');
    if (colonPos != std::string_view::npos && str.rfind("urn:", 0U) != 0U && str.rfind("URN:", 0U) != 0U) {
        const auto sPart = str.substr(0U, colonPos);
        const auto pPart = str.substr(colonPos + 1U);
        Uuid sUuid {};
        Uuid pUuid {};
        if (Uuid::fromString(sPart, sUuid) && Uuid::fromString(pPart, pUuid)) {
            sensorId = MiisSubId { MiisIdType::Sensor, MiisIdQuality::Physical, sUuid };
            platformId = MiisSubId { MiisIdType::Platform, MiisIdQuality::Physical, pUuid };
            streamId.reset();
            return true;
        }
    }

    Uuid u {};
    if (Uuid::fromString(str, u)) {
        streamId = u;
        sensorId = MiisSubId { MiisIdType::Sensor, MiisIdQuality::Physical, u };
        platformId.reset();
        return true;
    }

    return false;
}

Uuid MiisCoreId::primaryUuid() const noexcept
{
    if (sensorId)
        return sensorId->uuid;
    if (streamId)
        return *streamId;
    return Uuid {};
}

bool MiisCoreId::operator==(const MiisCoreId& other) const noexcept
{
    if (version != other.version)
        return false;
    if (sensorId.has_value() != other.sensorId.has_value())
        return false;
    if (sensorId && (sensorId->uuid != other.sensorId->uuid || sensorId->quality != other.sensorId->quality)) {
        return false;
    }
    if (platformId.has_value() != other.platformId.has_value())
        return false;
    if (platformId
        && (platformId->uuid != other.platformId->uuid || platformId->quality != other.platformId->quality)) {
        return false;
    }
    if (streamId.has_value() != other.streamId.has_value())
        return false;
    if (streamId && (*streamId != *other.streamId))
        return false;
    return true;
}

} // namespace Klv
