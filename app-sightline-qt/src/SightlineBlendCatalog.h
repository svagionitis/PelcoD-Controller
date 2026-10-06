#pragma once

/// @file SightlineBlendCatalog.h
/// @brief Read-only catalogue of Sightline blend-mode metadata for the QML UI.
/// @details Single C++ source of truth for blend-mode names, per-mode control capabilities, preset
///          alignment slots and operator-facing labels, derived from Sightline::BlendMode
///          (IDD 3.11 / EAN-Blending Table 3). Replaces the former QML-side BlendModes.js helper.

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

/// @class SightlineBlendCatalog
/// @brief Stateless QObject exposing blend-mode metadata to QML as the `blendCatalog` context property.
/// @details All data is constant. Integer inputs coming from QML are validated against
///          Sightline::BlendMode before use; unknown or reserved values (e.g. 5) never map to an enumerator.
class SightlineBlendCatalog : public QObject {
    Q_OBJECT
    /// @brief Ordered list of `{ value, name }` maps for every selectable blend mode.
    Q_PROPERTY(QVariantList modes READ modes CONSTANT)
    /// @brief Display names of every selectable blend mode, in the same order as `modes`.
    Q_PROPERTY(QStringList modeNames READ modeNames CONSTANT)
    /// @brief Valid 0x2F preset alignment indices (0..4 = 0xB9 slots, 10..14 = 0x95 slots).
    Q_PROPERTY(QVariantList presetValues READ presetValues CONSTANT)
    /// @brief Display names of every preset alignment index, in the same order as `presetValues`.
    Q_PROPERTY(QStringList presetNames READ presetNames CONSTANT)

public:
    /// @brief Constructs the catalogue.
    /// @param[in] parent Optional QObject parent.
    explicit SightlineBlendCatalog(QObject* parent = nullptr);

    /// @brief Check whether a wire value is a defined, non-reserved Sightline::BlendMode.
    /// @param[in] mode Wire value of the blend mode.
    /// @return True for 0..4 and 6..12; false for the reserved value 5 and anything out of range.
    [[nodiscard]] static bool isValidMode(int mode) noexcept;

    /// @brief Check whether an index is a valid 0x2F preset alignment slot.
    /// @param[in] index Preset alignment index.
    /// @return True for 0..4 (SLABlendAlign_t slots) and 10..14 (SLAFourAlignPoints_t slots).
    [[nodiscard]] static bool isValidPreset(int index) noexcept;

    /// @brief Get the ordered `{ value, name }` list of selectable blend modes.
    /// @return List of QVariantMap entries.
    [[nodiscard]] QVariantList modes() const;

    /// @brief Get the display names of every selectable blend mode.
    /// @return Names ordered like modes().
    [[nodiscard]] QStringList modeNames() const;

    /// @brief Get the valid 0x2F preset alignment indices.
    /// @return Ordered list of integer indices.
    [[nodiscard]] QVariantList presetValues() const;

    /// @brief Get the display names of every preset alignment index.
    /// @return Names ordered like presetValues().
    [[nodiscard]] QStringList presetNames() const;

    /// @brief Find the list position of a blend mode.
    /// @param[in] mode Wire value of the blend mode.
    /// @return Position in modes(), or the position of Frame Blend (Warped EO) if @p mode is invalid.
    Q_INVOKABLE [[nodiscard]] int indexOfMode(int mode) const noexcept;

    /// @brief Get the wire value at a list position.
    /// @param[in] index Position in modes().
    /// @return Wire value, or Frame Blend (Warped EO) if @p index is out of range.
    Q_INVOKABLE [[nodiscard]] int modeAt(int index) const noexcept;

    /// @brief Get the display name of a blend mode.
    /// @param[in] mode Wire value of the blend mode.
    /// @return Display name, or "Unknown (N)" for undefined values.
    Q_INVOKABLE [[nodiscard]] QString modeName(int mode) const;

    /// @brief Check whether a mode uses the hue field.
    /// @param[in] mode Wire value of the blend mode.
    /// @return True for Night, Color and Color IR blends, and for "No Change".
    Q_INVOKABLE [[nodiscard]] bool usesHue(int mode) const noexcept;

    /// @brief Check whether a mode honours the "hue controls colour" flag (bit 1).
    /// @param[in] mode Wire value of the blend mode.
    /// @return True for Color and Color IR blends, and for "No Change".
    Q_INVOKABLE [[nodiscard]] bool usesHueFlag(int mode) const noexcept;

    /// @brief Check whether a mode uses the thermal window (hot start / cold end).
    /// @param[in] mode Wire value of the blend mode.
    /// @return True for Thermal and Color IR blends, and for "No Change".
    Q_INVOKABLE [[nodiscard]] bool usesThermal(int mode) const noexcept;

    /// @brief Check whether a mode honours IR histogram equalisation (flag bit 0).
    /// @param[in] mode Wire value of the blend mode.
    /// @return True for Thermal blends, and for "No Change".
    Q_INVOKABLE [[nodiscard]] bool usesHistEq(int mode) const noexcept;

    /// @brief Check whether a mode colours IR with the user palette.
    /// @param[in] mode Wire value of the blend mode.
    /// @return True for the two Color IR blends only.
    Q_INVOKABLE [[nodiscard]] bool usesPalette(int mode) const noexcept;

    /// @brief Format the blend mix ratio for display.
    /// @param[in] mode Wire value of the blend mode.
    /// @param[in] amt Blend amount (clamped to 0..255; 255 = all EO / all warp).
    /// @return "x% EO · y% IR", or "x% Warp · y% Fixed" for the dual-EO mode.
    Q_INVOKABLE [[nodiscard]] QString mixLabel(int mode, int amt) const;

    /// @brief Decode the 0x2F rotation byte into degrees.
    /// @param[in] raw Rotation byte (clamped to 0..255; 0 = no change).
    /// @return Rotation in degrees: 1..255 maps linearly to -5..+5; 0 yields 0.
    Q_INVOKABLE [[nodiscard]] double rotationDeg(int raw) const noexcept;

    /// @brief Format a preset alignment index for display.
    /// @param[in] index Preset alignment index.
    /// @return "4-Point Slot N (0x95)" for 10..14, otherwise "Align Slot N (0xB9)".
    Q_INVOKABLE [[nodiscard]] QString presetLabel(int index) const;

    /// @brief Format a BlendFlags bitmask for display.
    /// @param[in] flags BlendFlags bitmask.
    /// @return "HistEq", "UseHue", "HistEq + UseHue" or "None".
    Q_INVOKABLE [[nodiscard]] QString flagsLabel(int flags) const;
};
