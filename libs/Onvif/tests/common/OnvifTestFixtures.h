#pragma once

/// @file OnvifTestFixtures.h
/// @brief Reusable RAII GoogleTest fixture for OnvifServer tests with ephemeral port allocation.

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestClient.h"
#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifServerTypes.h>

namespace Onvif::Test {

/// @brief Utility to allocate a free ephemeral TCP port on localhost.
/// @return Bound and immediately released port number.
[[nodiscard]] std::uint16_t getFreeEphemeralPort();

/// @brief Normalizes XML string by parsing and re-emitting with canonical formatting.
/// @param[in] rawXml Input raw XML string.
/// @param[out] outFormatted Destination string receiving formatted XML.
/// @return True if parsing succeeded; false otherwise.
[[nodiscard]] bool normalizeXml(std::string_view rawXml, std::string& outFormatted);

/// @class MockPtzHandler
/// @brief In-memory mock implementation of IPtzHandler for unit and integration testing.
class MockPtzHandler : public IPtzHandler {
public:
    void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) override
    {
        static_cast<void>(timeout);
        lastPan = panSpeed;
        lastTilt = tiltSpeed;
        lastZoom = zoomSpeed;
        moveCount++;
    }

    void handleAbsoluteMove(std::optional<float> pan, std::optional<float> tilt, std::optional<float> zoom,
        const std::string& spaceUri = "") override
    {
        static_cast<void>(spaceUri);
        absPan = pan.value_or(0.0f);
        absTilt = tilt.value_or(0.0f);
        absZoom = zoom.value_or(0.0f);
        absMoveCount++;
    }

    void handleStop(bool stopPanTilt, bool stopZoom) override
    {
        lastStopPt = stopPanTilt;
        lastStopZ = stopZoom;
        stopCount++;
    }

    std::string handleSetPreset(const std::string& name, const std::string& token) override
    {
        std::string tok = token.empty() ? "1" : token;
        presets.push_back({ tok, name });
        return tok;
    }

    bool handleGotoPreset(const std::string& token) override
    {
        lastGotoPreset = token;
        if (token.find("NoSuch") != std::string::npos || token.find("Invalid") != std::string::npos
            || token.find("Bad") != std::string::npos || token.find("NonExistent") != std::string::npos) {
            return false;
        }
        return true;
    }

    bool handleRemovePreset(const std::string& token) override
    {
        lastRemovePreset = token;
        return true;
    }

    std::vector<PtzPreset> handleGetPresets() override
    {
        return presets;
    }

    PtzStatus handleGetStatus() override
    {
        PtzStatus st { };
        st.pan = 180.0;
        st.tilt = 45.0;
        st.zoom = 0.5;
        st.isMoving = false;
        return st;
    }

    float lastPan { 0.0f };
    float lastTilt { 0.0f };
    float lastZoom { 0.0f };
    int moveCount { 0 };

    float absPan { 0.0f };
    float absTilt { 0.0f };
    float absZoom { 0.0f };
    int absMoveCount { 0 };

    bool lastStopPt { false };
    bool lastStopZ { false };
    int stopCount { 0 };

    std::string lastGotoPreset { };
    std::string lastRemovePreset { };
    std::vector<PtzPreset> presets { };
};

/// @class MockImagingHandler
/// @brief In-memory mock implementation of IImagingHandler for testing.
class MockImagingHandler : public IImagingHandler {
public:
    ImagingSettings handleGetImagingSettings(const std::string& /*videoSourceToken*/) override
    {
        return settings;
    }

    bool handleSetImagingSettings(const std::string& /*videoSourceToken*/, const ImagingSettings& newSettings) override
    {
        settings = newSettings;
        setSettingsCount++;
        return true;
    }

    void handleMoveFocus(const std::string& /*videoSourceToken*/, float speed) override
    {
        lastFocusSpeed = speed;
        moveFocusCount++;
    }

    void handleStopFocus(const std::string& /*videoSourceToken*/) override
    {
        stopFocusCount++;
    }

    ImagingSettings settings { };
    int setSettingsCount { 0 };
    float lastFocusSpeed { 0.0f };
    int moveFocusCount { 0 };
    int stopFocusCount { 0 };
};

/// @class OnvifServerFixture
/// @brief Base GoogleTest fixture providing an isolated OnvifServer instance on an ephemeral port.
class OnvifServerFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    /// @brief Sends a SOAP POST request to the server and loads the response into a pugixml document.
    /// @param[in] path Request URL path.
    /// @param[in] soapRequest XML request string.
    /// @param[out] outDoc Parsed response XML document.
    /// @return HTTP status code, or -1 on network failure.
    [[nodiscard]] int sendSoapReq(std::string_view path, std::string_view soapRequest, pugi::xml_document& outDoc);

    std::uint16_t m_port { 0U };
    OnvifServerConfig m_config { };
    std::shared_ptr<MockPtzHandler> m_ptzHandler;
    std::shared_ptr<MockImagingHandler> m_imagingHandler;
    std::shared_ptr<OnvifServer> m_server;
    std::unique_ptr<OnvifTest::OnvifTestClient> m_client;
};

} // namespace Onvif::Test
