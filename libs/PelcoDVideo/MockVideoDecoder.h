#pragma once

/// @file MockVideoDecoder.h
/// @brief Synthetic test pattern and frame generator implementing IVideoDecoder.

#include "AtomicTripleBuffer.h"
#include "IVideoDecoder.h"

#include <chrono>
#include <string>
#include <vector>

namespace PelcoD::Video {

/// @class MockVideoDecoder
/// @brief Deterministic synthetic video decoder generating test patterns for headless testing and offline demos.
class MockVideoDecoder : public IVideoDecoder {
public:
    /// @brief Constructor.
    MockVideoDecoder();

    /// @brief Destructor.
    ~MockVideoDecoder() override = default;

    bool initialize(std::string_view source,
                    PixelFormat format = PixelFormat::RGB24,
                    int threadCount = 0,
                    DeviceType device = DeviceType::CPU) override;

    bool decodeNextFrame() override;

    [[nodiscard]] FrameInfo getRawFrameData() const override;

    [[nodiscard]] VideoMetadata getVideoMetadata() const override;

    [[nodiscard]] DecoderPerformanceStats getPerformanceStats() const override;

    bool seek(double timeInSeconds) override;

    void enableTripleBuffering(bool enable) override;

    [[nodiscard]] bool isTripleBufferingEnabled() const override;

    void close() override;

private:
    void renderTestPattern(std::uint8_t* buffer);

    int m_width { 640 };
    int m_height { 360 };
    double m_frameRate { 30.0 };
    PixelFormat m_format { PixelFormat::RGB24 };
    bool m_initialized { false };
    bool m_tripleBufferingEnabled { false };

    std::uint64_t m_frameIndex { 0U };
    double m_currentTimeSec { 0.0 };
    std::vector<std::uint8_t> m_currentFrameBuffer;

    AtomicTripleBuffer<FrameBufferSlot> m_tripleBuffer;

    std::chrono::steady_clock::time_point m_initTime {};
    double m_totalDecodeTimeMs { 0.0 };
    double m_lastDecodeTimeMs { 0.0 };
};

} // namespace PelcoD::Video
