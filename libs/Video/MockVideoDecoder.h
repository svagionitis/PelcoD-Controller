#pragma once

/// @file MockVideoDecoder.h
/// @brief Synthetic test pattern and frame generator implementing IVideoDecoder.

#include "BaseVideoDecoder.h"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace Video {

/// @class MockVideoDecoder
/// @brief Deterministic synthetic video decoder generating test patterns for headless testing and offline demos.
/// @details Inherits shared state and non-backend-specific method implementations from BaseVideoDecoder.
class MockVideoDecoder : public BaseVideoDecoder {
public:
    /// @brief Constructor.
    MockVideoDecoder();

    /// @brief Destructor.
    ~MockVideoDecoder() override = default;

    bool initialize(std::string_view source, PixelFormat format = PixelFormat::RGB24, int threadCount = 0,
        DeviceType device = DeviceType::CPU) override;

    bool decodeNextFrame() override;

    bool seek(double timeInSeconds) override;

    void close() override;

    /// @brief Simulates network connection loss for outage testing.
    /// @param[in] loss True to simulate severed connection, false to simulate restored link.
    void setSimulatedConnectionLoss(bool loss);

    /// @brief Checks whether connection loss simulation is active.
    /// @return True if connection is simulated as lost.
    [[nodiscard]] bool isSimulatedConnectionLoss() const noexcept;

private:
    void renderTestPattern(std::uint8_t* buffer);

    std::uint64_t m_frameIndex { 0U };
    double m_currentTimeSec { 0.0 };

    std::chrono::steady_clock::time_point m_initTime {};
    bool m_simulatedConnectionLoss { false };
};

} // namespace Video
