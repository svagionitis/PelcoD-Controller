#pragma once

/// @file TrafficView.h
/// @brief Real-time colorized Pelco-D hex packet traffic monitor and protocol decoder.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoDTui {

/// @struct PacketRecord
/// @brief Captured inbound or outbound Pelco-D packet frame with timestamp and decode.
struct PacketRecord {
    std::chrono::system_clock::time_point timestamp {};
    bool isTx { true };
    std::vector<std::uint8_t> frame {};
    std::string hexStr {};
    std::string decoded {};
};

/// @class TrafficView
/// @brief Live scrollable packet inspector rendering hex streams with protocol opcode translation.
class TrafficView {
public:
    TrafficView() = default;
    ~TrafficView() = default;

    void addPacket(bool isTx, const std::vector<std::uint8_t>& frame);
    void render(Canvas& canvas, int startY, int width, int height);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

private:
    [[nodiscard]] static std::string decodeFrame(bool isTx, const std::vector<std::uint8_t>& frame);

    mutable std::mutex m_mutex;
    std::deque<PacketRecord> m_packets;
    bool m_paused { false };
    int m_scrollOffset { 0 };
    static constexpr std::size_t kMaxPackets { 250U };
};

} // namespace PelcoDTui
