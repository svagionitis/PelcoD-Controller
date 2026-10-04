#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Klv {

// Forward declaration
enum class KlvStatus;

/// @enum MiisIdType
/// @brief Identification type categories defined in MISB ST 1204 Section 6.
enum class MiisIdType : std::uint8_t {
    Undetermined = 0U, ///< Undetermined identification type
    Sensor = 1U,       ///< Sensor / Device Sub-Identifier
    Platform = 2U,     ///< Platform / System Sub-Identifier
    Window = 3U        ///< Window / Sub-frame Sub-Identifier
};

/// @enum MiisIdQuality
/// @brief Identification quality defined in MISB ST 1204 Section 7.
enum class MiisIdQuality : std::uint8_t {
    Unknown = 0U,  ///< Unknown quality
    Physical = 1U, ///< Physical device / real hardware asset
    Virtual = 2U   ///< Virtual / synthetic / simulated asset
};

/// @struct Uuid
/// @brief 128-bit Universally Unique Identifier conforming to RFC 4122.
struct Uuid {
    std::array<std::uint8_t, 16> bytes {}; ///< 16-byte binary payload

    /// @brief Checks if this UUID is non-zero.
    /// @return True if any byte is non-zero.
    [[nodiscard]] bool isNull() const noexcept;

    /// @brief Checks if this UUID has a valid RFC 4122 variant (bits 10xx).
    /// @return True if variant matches RFC 4122.
    [[nodiscard]] bool isValidVariant() const noexcept;

    /// @brief Gets the UUID version number (1..5).
    /// @return Version nibble.
    [[nodiscard]] std::uint8_t version() const noexcept;

    /// @brief Formats as a 36-character hyphenated string (8-4-4-4-12).
    /// @param[in] uppercase True for uppercase hex, false for lowercase.
    /// @return 36-char string representation.
    [[nodiscard]] std::string toString(bool uppercase = false) const;

    /// @brief Formats as a URN string: urn:uuid:xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.
    /// @return 45-character URN string.
    [[nodiscard]] std::string toUrn() const;

    /// @brief Parses a UUID from a hex, hyphenated, or URN string.
    /// @param[in] str Input string.
    /// @param[out] out Resulting UUID.
    /// @return True if successfully parsed.
    [[nodiscard]] static bool fromString(std::string_view str, Uuid& out) noexcept;

    /// @brief Generates an RFC 4122 Version 4 (Random) UUID.
    /// @return Version 4 UUID.
    [[nodiscard]] static Uuid generateV4();

    /// @brief Generates an RFC 4122 Version 5 (SHA-1 Name-based) UUID.
    /// @param[in] ns Namespace UUID.
    /// @param[in] name Entity name string.
    /// @return Version 5 UUID.
    [[nodiscard]] static Uuid generateV5(const Uuid& ns, std::string_view name);

    /// @brief Generates an RFC 4122 Version 1 (Time and MAC-based) UUID.
    /// @param[in] mac 6-byte IEEE 802 MAC node address.
    /// @param[in] timestamp100ns 100-nanosecond intervals since Oct 15 1582.
    /// @param[in] clockSeq Clock sequence counter.
    /// @return Version 1 UUID.
    [[nodiscard]] static Uuid generateV1(const std::array<std::uint8_t, 6>& mac,
                                         std::uint64_t timestamp100ns,
                                         std::uint16_t clockSeq) noexcept;

    /// @brief Equality comparison operator.
    [[nodiscard]] bool operator==(const Uuid& other) const noexcept {
        return bytes == other.bytes;
    }

    /// @brief Inequality comparison operator.
    [[nodiscard]] bool operator!=(const Uuid& other) const noexcept {
        return bytes != other.bytes;
    }
};

/// @struct MiisSubId
/// @brief Represents a typed Sub-Identifier in MISB ST 1204.
struct MiisSubId {
    MiisIdType type { MiisIdType::Sensor };       ///< Sub-identifier type category
    MiisIdQuality quality { MiisIdQuality::Physical }; ///< Asset quality
    Uuid uuid {};                                ///< 16-byte UUID
};

/// @class MiisCoreId
/// @brief Motion Imagery Identification System (MIIS) Core Identifier per MISB ST 1204.
/// @details Supports encoding and decoding in ST 0601 Tag 94 and ST 0903 Item 13 formats:
///          - 16-byte binary single stream UUID
///          - 32-byte composite UUID (Sensor UUID + Platform UUID)
///          - 33-byte versioned composite (Version + Sensor UUID + Platform UUID)
///          - 35-byte versioned quality composite (Version + Quality + UUID + Quality + UUID)
///          - RFC 4122 canonical text and URN formats.
class MiisCoreId {
public:
    std::uint8_t version { 1U };                 ///< MISB ST 1204 version (default 1)
    std::optional<MiisSubId> sensorId {};       ///< Sensor Sub-Identifier
    std::optional<MiisSubId> platformId {};     ///< Platform Sub-Identifier
    std::optional<MiisSubId> windowId {};       ///< Optional Window Sub-Identifier
    std::optional<Uuid> streamId {};             ///< Direct single stream UUID

    /// @brief Default constructor.
    MiisCoreId() noexcept = default;

    /// @brief Constructs from a single stream UUID.
    /// @param[in] id Stream UUID.
    explicit MiisCoreId(const Uuid& id) noexcept;

    /// @brief Constructs a composite Core Identifier from sensor and platform UUIDs.
    /// @param[in] sensor Sensor UUID.
    /// @param[in] platform Platform UUID.
    /// @param[in] sensorQuality Sensor quality (Physical or Virtual).
    /// @param[in] platformQuality Platform quality (Physical or Virtual).
    MiisCoreId(const Uuid& sensor, const Uuid& platform,
               MiisIdQuality sensorQuality = MiisIdQuality::Physical,
               MiisIdQuality platformQuality = MiisIdQuality::Physical) noexcept;

    /// @brief Validates conformance with MISB ST 1204 requirements.
    /// @return True if valid, false if non-conforming.
    [[nodiscard]] bool validate() const noexcept;

    /// @brief Encodes the MIIS Core Identifier into binary form for Tag 94.
    /// @param[in] includeQuality True to emit 35-byte quality composite if both sub-IDs present.
    /// @return Serialized byte buffer.
    [[nodiscard]] std::vector<std::uint8_t> encode(bool includeQuality = false) const;

    /// @brief Decodes an MIIS Core Identifier from a binary buffer.
    /// @param[in] data Pointer to incoming buffer.
    /// @param[in] length Number of bytes.
    /// @return KlvStatus code.
    [[nodiscard]] KlvStatus decode(const std::uint8_t* data, std::size_t length) noexcept;

    /// @brief Formats as a human-readable display string or URN.
    /// @return Display string.
    [[nodiscard]] std::string toString() const;

    /// @brief Formats the primary identifier as a URN string.
    /// @return urn:uuid string.
    [[nodiscard]] std::string toUrn() const;

    /// @brief Parses an MIIS Core Identifier from a canonical UUID or URN string.
    /// @param[in] str String containing UUID or URN.
    /// @return True on success.
    [[nodiscard]] bool fromString(std::string_view str) noexcept;

    /// @brief Primary UUID accessor (sensor ID, stream ID, or null).
    /// @return Effective primary UUID.
    [[nodiscard]] Uuid primaryUuid() const noexcept;

    /// @brief Equality comparison operator.
    [[nodiscard]] bool operator==(const MiisCoreId& other) const noexcept;

    /// @brief Inequality comparison operator.
    [[nodiscard]] bool operator!=(const MiisCoreId& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace Klv
