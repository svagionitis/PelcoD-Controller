#pragma once

/// @file StubAnalyticsAdapter.h
/// @brief Adapter managing ONVIF Video Analytics (Profile M & T) and Metadata configuration.

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"

namespace Onvif {

/// @class StubAnalyticsAdapter
/// @brief Provider for ONVIF Video Analytics (Profile M & T) and Metadata configuration.
/// @details Implements IAnalyticsHandler and IMetadataHandler per Interface Segregation Principle.
class StubAnalyticsAdapter : public IAnalyticsHandler, public IMetadataHandler {
public:
    StubAnalyticsAdapter();
    ~StubAnalyticsAdapter() override = default;

    StubAnalyticsAdapter(const StubAnalyticsAdapter&) = delete;
    StubAnalyticsAdapter& operator=(const StubAnalyticsAdapter&) = delete;
    StubAnalyticsAdapter(StubAnalyticsAdapter&&) = delete;
    StubAnalyticsAdapter& operator=(StubAnalyticsAdapter&&) = delete;

    /// @brief Registers an event publisher callback for analytics alarms.
    /// @param[in] publisher Callback function.
    void setEventPublisher(EventCallback publisher);

    // =========================================================================
    // IAnalyticsHandler Implementation
    // =========================================================================

    /// @brief Retrieves supported analytics rule descriptions for a video analytics configuration.
    /// @param[in] configToken VideoAnalyticsConfiguration token.
    /// @return Vector of supported rule descriptions.
    [[nodiscard]] std::vector<AnalyticsRuleDescription> handleGetSupportedRules(
        const std::string& configToken) override;

    /// @brief Retrieves active analytics rules for a configuration.
    /// @param[in] configToken VideoAnalyticsConfiguration token.
    /// @return Vector of active AnalyticsRule definitions.
    [[nodiscard]] std::vector<AnalyticsRule> handleGetRules(const std::string& configToken) override;

    /// @brief Creates new analytics rules.
    /// @param[in] configToken VideoAnalyticsConfiguration token.
    /// @param[in] rules Rules to create.
    /// @return True on success.
    [[nodiscard]] bool handleCreateRules(
        const std::string& configToken, const std::vector<AnalyticsRule>& rules) override;

    /// @brief Modifies existing analytics rules.
    /// @param[in] configToken VideoAnalyticsConfiguration token.
    /// @param[in] rules Updated rules.
    /// @return True on success.
    [[nodiscard]] bool handleModifyRules(
        const std::string& configToken, const std::vector<AnalyticsRule>& rules) override;

    /// @brief Deletes analytics rules by name.
    /// @param[in] configToken VideoAnalyticsConfiguration token.
    /// @param[in] ruleNames Names of rules to delete.
    /// @return True on success.
    [[nodiscard]] bool handleDeleteRules(
        const std::string& configToken, const std::vector<std::string>& ruleNames) override;

    /// @brief Retrieves supported analytics modules.
    /// @param[in] configToken Configuration token.
    /// @return Vector of supported analytics module descriptions.
    [[nodiscard]] std::vector<AnalyticsModuleDescription> handleGetSupportedModules(
        const std::string& configToken) override;

    /// @brief Retrieves configured analytics modules.
    /// @param[in] configToken Configuration token.
    /// @return Vector of active AnalyticsModule definitions.
    [[nodiscard]] std::vector<AnalyticsModule> handleGetAnalyticsModules(const std::string& configToken) override;

    /// @brief Creates new analytics modules.
    /// @param[in] configToken Configuration token.
    /// @param[in] modules Modules to create.
    /// @return True on success.
    [[nodiscard]] bool handleCreateAnalyticsModules(
        const std::string& configToken, const std::vector<AnalyticsModule>& modules) override;

    /// @brief Modifies existing analytics modules.
    /// @param[in] configToken Configuration token.
    /// @param[in] modules Updated modules.
    /// @return True on success.
    [[nodiscard]] bool handleModifyAnalyticsModules(
        const std::string& configToken, const std::vector<AnalyticsModule>& modules) override;

    /// @brief Deletes analytics modules by name.
    /// @param[in] configToken Configuration token.
    /// @param[in] moduleNames Names of modules to delete.
    /// @return True on success.
    [[nodiscard]] bool handleDeleteAnalyticsModules(
        const std::string& configToken, const std::vector<std::string>& moduleNames) override;

    // =========================================================================
    // IMetadataHandler Implementation
    // =========================================================================

    /// @brief Retrieves all metadata configurations.
    /// @return Vector of MetadataConfiguration records.
    [[nodiscard]] std::vector<MetadataConfiguration> handleGetMetadataConfigs() override;

    /// @brief Retrieves a specific metadata configuration by token.
    /// @param[in] token Configuration identifier.
    /// @return MetadataConfiguration or nullopt if not found.
    [[nodiscard]] std::optional<MetadataConfiguration> handleGetMetadataConfig(const std::string& token) override;

    /// @brief Modifies a metadata configuration.
    /// @param[in] config Updated configuration.
    /// @return True on success.
    [[nodiscard]] bool handleSetMetadataConfig(const MetadataConfiguration& config) override;

    /// @brief Retrieves options and capabilities for metadata configuration.
    /// @param[in] configToken Configuration token.
    /// @param[in] profileToken Profile token.
    /// @return MetadataConfigurationOptions structure.
    [[nodiscard]] MetadataConfigurationOptions handleGetMetaConfigOptions(
        const std::string& configToken, const std::string& profileToken = "") override;

    /// @brief Produces a live snapshot of active metadata.
    /// @param[in] profileToken Media profile token.
    /// @return MetadataStreamPayload structure.
    [[nodiscard]] MetadataStreamPayload handleGetCurrentMetadata(const std::string& profileToken = "") override;

    /// @brief Injects or updates active analytics detected objects for metadata streaming.
    /// @param[in] objects List of detected objects.
    void setDetectedObjects(std::vector<AnalyticsObject> objects);

    /// @brief Appends a single detected analytics object.
    /// @param[in] object AnalyticsObject to track.
    void addDetectedObject(const AnalyticsObject& object);

    /// @brief Clears current active detected analytics objects.
    void clearDetectedObjects();

private:
    void evaluateRulesForObject(
        const AnalyticsObject& prevObj, const AnalyticsObject& currentObj, std::vector<OnvifEvent>& outEvents);
    void evaluateRulesForFrame(std::vector<OnvifEvent>& outEvents);

    struct ObjectTrackState {
        AnalyticsObject lastObject { };
        std::chrono::steady_clock::time_point firstSeen { };
        std::chrono::steady_clock::time_point lastSeen { };
        std::map<std::string, bool> triggeredRules { };
    };

    mutable std::mutex m_mutex { };
    EventCallback m_eventPublisher { };
    std::vector<MetadataConfiguration> m_metadataConfigs { { "MetadataConfig_1", "DefaultMetadataConfig", 1, "PT60S",
        true, true, true, true } };
    std::vector<AnalyticsObject> m_detectedObjects { };
    std::chrono::steady_clock::time_point m_startTime { std::chrono::steady_clock::now() };
    std::vector<AnalyticsRule> m_rules { };
    std::vector<AnalyticsModule> m_analyticsModules { };
    std::map<std::int32_t, ObjectTrackState> m_objectTracks { };
};

} // namespace Onvif
