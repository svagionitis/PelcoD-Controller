#pragma once

/// @file MacroScript.h
/// @brief Pelco-D / Pelco-P command sequence scripting and serialization definitions.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD {

/// @struct MacroStep
/// @brief Represents a single discrete command frame and post-transmission wait interval in a macro.
struct MacroStep {
    std::string label {};               ///< Human-readable step label or annotation.
    std::vector<std::uint8_t> frame {}; ///< Raw protocol frame bytes.
    std::uint32_t delayMs { 100U };     ///< Delay in milliseconds to wait before executing next step.
    bool expectResponse { false };      ///< Whether this step expects an inbound response.
};

/// @struct MacroSequence
/// @brief Represents an ordered macro script composed of multiple steps with repeat rules.
struct MacroSequence {
    std::string name { "Untitled Macro" }; ///< Name of the macro sequence.
    std::string description {};            ///< Description and operator notes.
    std::uint32_t repeatCount { 1U };      ///< Repeat execution loop count (0 = infinite loop).
    std::vector<MacroStep> steps {};       ///< Ordered sequence of macro steps.
};

/// @class MacroSerializer
/// @brief Serialization, deserialization, and validation utilities for macro sequences.
/// @details Supports both structured JSON format and line-based plain hex script format.
class MacroSerializer {
public:
    /// @brief Deserializes a macro sequence from JSON formatted text.
    /// @param[in] json Text containing JSON representation of the macro sequence.
    /// @return Parsed MacroSequence object.
    /// @throws std::runtime_error If JSON parsing or field validation fails.
    [[nodiscard]] static MacroSequence fromJson(const std::string& json);

    /// @brief Serializes a macro sequence to formatted JSON text.
    /// @param[in] sequence Macro sequence to serialize.
    /// @return Formatted JSON string.
    [[nodiscard]] static std::string toJson(const MacroSequence& sequence);

    /// @brief Deserializes a macro sequence from a plaintext hex script.
    /// @details Format supports lines with: HEX_BYTES [delayMs] [# comment]
    /// @param[in] script Plaintext script content.
    /// @return Parsed MacroSequence object.
    /// @throws std::runtime_error If syntax errors or invalid hex strings are encountered.
    [[nodiscard]] static MacroSequence fromScript(const std::string& script);

    /// @brief Serializes a macro sequence to a clean plaintext hex script.
    /// @param[in] sequence Macro sequence to export.
    /// @return Formatted line-oriented script.
    [[nodiscard]] static std::string toScript(const MacroSequence& sequence);

    /// @brief Validates if a macro sequence contains valid protocol frames.
    /// @param[in] sequence Sequence to check.
    /// @param[out] errorMsg If non-null and validation fails, populated with explanation.
    /// @return True if valid, false otherwise.
    [[nodiscard]] static bool validate(const MacroSequence& sequence, std::string* errorMsg = nullptr) noexcept;
};

} // namespace PelcoD
