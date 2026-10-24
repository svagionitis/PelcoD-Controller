#pragma once

/// @file StubMediaExtensionAdapter.h
/// @brief Adapter managing Media2 Privacy Masks and Video Source Capture Modes.

#include <cstdint>

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"

namespace Onvif {

/// @class StubMediaExtensionAdapter
/// @brief Provider for Media2 Privacy Masks (IMaskHandler) and Video Source Modes (IVideoSourceModeHandler).
/// @details Implements IMaskHandler and IVideoSourceModeHandler per ISP segregation.
class StubMediaExtensionAdapter : public IMaskHandler, public IVideoSourceModeHandler {
public:
    StubMediaExtensionAdapter();
    ~StubMediaExtensionAdapter() override = default;

    StubMediaExtensionAdapter(const StubMediaExtensionAdapter&) = delete;
    StubMediaExtensionAdapter& operator=(const StubMediaExtensionAdapter&) = delete;
    StubMediaExtensionAdapter(StubMediaExtensionAdapter&&) = delete;
    StubMediaExtensionAdapter& operator=(StubMediaExtensionAdapter&&) = delete;

    // =========================================================================
    // IMaskHandler Implementation
    // =========================================================================

    /// @brief Retrieves privacy mask capabilities for a video source configuration.
    /// @param[in] configToken VideoSourceConfiguration token.
    /// @return MaskOptions containing limits and supported types.
    [[nodiscard]] MaskOptions handleGetMaskOptions(const std::string& configToken) override;

    /// @brief Retrieves all configured privacy masks.
    /// @param[in] configToken Optional configuration token filter.
    /// @return Vector of active PrivacyMask definitions.
    [[nodiscard]] std::vector<PrivacyMask> handleGetMasks(const std::string& configToken) override;

    /// @brief Retrieves a specific privacy mask by token.
    /// @param[in] maskToken Mask token identifier.
    /// @return PrivacyMask if found.
    [[nodiscard]] std::optional<PrivacyMask> handleGetMask(const std::string& maskToken) override;

    /// @brief Modifies an existing privacy mask.
    /// @param[in] mask Updated mask settings.
    /// @return True on success.
    bool handleSetMask(const PrivacyMask& mask) override;

    /// @brief Creates a new privacy mask.
    /// @param[in] mask New mask to register.
    /// @return Assigned mask token on success, empty on failure.
    [[nodiscard]] std::string handleCreateMask(const PrivacyMask& mask) override;

    /// @brief Deletes a privacy mask by token.
    /// @param[in] maskToken Token of mask to delete.
    /// @return True if deleted.
    bool handleDeleteMask(const std::string& maskToken) override;

    // =========================================================================
    // IVideoSourceModeHandler Implementation
    // =========================================================================

    /// @brief Retrieves available video source sensor capture modes.
    /// @param[in] videoSourceToken Video source token.
    /// @return Vector of supported VideoSourceMode options.
    [[nodiscard]] std::vector<VideoSourceMode> handleGetVideoSourceModes(const std::string& videoSourceToken) override;

    /// @brief Switches camera sensor capture mode.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] modeToken Desired mode token.
    /// @param[out] outRebootNeeded Set to true if reboot is required.
    /// @return True on success.
    bool handleSetVideoSourceMode(
        const std::string& videoSourceToken, const std::string& modeToken, bool& outRebootNeeded) override;

private:
    mutable std::mutex m_maskMutex { };
    std::vector<PrivacyMask> m_masks { { "Mask_1", "VideoSource_1",
        { { 0.1f, 0.1f }, { 0.3f, 0.1f }, { 0.3f, 0.3f }, { 0.1f, 0.3f } }, MaskType::Color, { 0, 0, 0, "RGB" },
        true } };
    MaskOptions m_maskOptions { };
    std::uint32_t m_nextMaskId { 2U };

    mutable std::mutex m_videoSourceModeMutex { };
    std::vector<VideoSourceMode> m_videoSourceModes { { "Mode_1080p60", true, 60.0f, 1920, 1080, { "H264", "H265" },
                                                          false, "1080p 60fps" },
        { "Mode_4k30", false, 30.0f, 3840, 2160, { "H264", "H265" }, true, "4K UHD 30fps (reboot required)" } };
};

} // namespace Onvif
