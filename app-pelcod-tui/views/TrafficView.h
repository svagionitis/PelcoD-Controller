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

/// @enum TrafficFilter
/// @brief Direction filter for traffic packet display and export.
enum class TrafficFilter : std::uint8_t { All, TxOnly, RxOnly };

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

    /// @brief Export current packet history to a formatted text log file.
    /// @param filename Destination path on the local filesystem.
    /// @return true if packets were written successfully.
    [[nodiscard]] bool exportToFile(const std::string& filename) const;

    /// @brief Retrieve active traffic direction filter.
    [[nodiscard]] TrafficFilter getFilter() const noexcept
    {
        return m_filter;
    }

    /// @brief Set active traffic direction filter.
    void setFilter(TrafficFilter filter) noexcept
    {
        m_filter = filter;
        m_scrollOffset = 0;
    }

    /// @brief Total packets recorded in buffer.
    [[nodiscard]] std::size_t getPacketCount() const noexcept;

    /// @brief Total packets matching the active direction filter.
    [[nodiscard]] std::size_t getFilteredPacketCount() const noexcept;

    /// @brief Text representation of traffic filter mode.
    [[nodiscard]] static std::string_view filterToString(TrafficFilter filter) noexcept;

private:
    [[nodiscard]] static std::string decodeFrame(bool isTx, const std::vector<std::uint8_t>& frame);

    mutable std::mutex m_mutex;
    std::deque<PacketRecord> m_packets;
    bool m_paused { false };
    int m_scrollOffset { 0 };
    TrafficFilter m_filter { TrafficFilter::All };
    std::string m_statusMessage {};
    std::chrono::steady_clock::time_point m_statusTime {};
    static constexpr std::size_t kMaxPackets { 250U };
};

} // namespace PelcoDTui
