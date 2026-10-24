#include "StubMediaExtensionAdapter.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Onvif {

StubMediaExtensionAdapter::StubMediaExtensionAdapter() = default;

// =========================================================================
// IMaskHandler Implementation
// =========================================================================

MaskOptions StubMediaExtensionAdapter::handleGetMaskOptions(const std::string& /*configToken*/)
{
    std::scoped_lock lock(m_maskMutex);
    return m_maskOptions;
}

std::vector<PrivacyMask> StubMediaExtensionAdapter::handleGetMasks(const std::string& configToken)
{
    std::scoped_lock lock(m_maskMutex);
    if (configToken.empty()) {
        return m_masks;
    }
    std::vector<PrivacyMask> filtered { };
    for (const auto& m : m_masks) {
        if (m.configurationToken == configToken) {
            filtered.push_back(m);
        }
    }
    return filtered;
}

std::optional<PrivacyMask> StubMediaExtensionAdapter::handleGetMask(const std::string& maskToken)
{
    std::scoped_lock lock(m_maskMutex);
    const auto it = std::find_if(
        m_masks.begin(), m_masks.end(), [&maskToken](const PrivacyMask& m) { return m.token == maskToken; });
    if (it != m_masks.end()) {
        return *it;
    }
    return std::nullopt;
}

bool StubMediaExtensionAdapter::handleSetMask(const PrivacyMask& mask)
{
    std::scoped_lock lock(m_maskMutex);
    const auto it
        = std::find_if(m_masks.begin(), m_masks.end(), [&mask](const PrivacyMask& m) { return m.token == mask.token; });
    if (it != m_masks.end()) {
        *it = mask;
    } else {
        m_masks.push_back(mask);
    }
    return true;
}

std::string StubMediaExtensionAdapter::handleCreateMask(const PrivacyMask& mask)
{
    std::scoped_lock lock(m_maskMutex);
    PrivacyMask created = mask;
    if (created.token.empty()) {
        created.token = "Mask_" + std::to_string(m_nextMaskId++);
    }
    m_masks.push_back(created);
    return created.token;
}

bool StubMediaExtensionAdapter::handleDeleteMask(const std::string& maskToken)
{
    std::scoped_lock lock(m_maskMutex);
    const auto it = std::find_if(
        m_masks.begin(), m_masks.end(), [&maskToken](const PrivacyMask& m) { return m.token == maskToken; });
    if (it != m_masks.end()) {
        m_masks.erase(it);
        return true;
    }
    return false;
}

// =========================================================================
// IVideoSourceModeHandler Implementation
// =========================================================================

std::vector<VideoSourceMode> StubMediaExtensionAdapter::handleGetVideoSourceModes(
    const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_videoSourceModeMutex);
    return m_videoSourceModes;
}

bool StubMediaExtensionAdapter::handleSetVideoSourceMode(
    const std::string& /*videoSourceToken*/, const std::string& modeToken, bool& outRebootNeeded)
{
    std::scoped_lock lock(m_videoSourceModeMutex);
    bool found = false;
    outRebootNeeded = false;
    for (auto& mode : m_videoSourceModes) {
        if (mode.token == modeToken) {
            mode.enabled = true;
            outRebootNeeded = mode.reboot;
            found = true;
        } else {
            mode.enabled = false;
        }
    }
    return found;
}

} // namespace Onvif
