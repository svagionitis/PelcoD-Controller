#pragma once

/// @file ConnectionModal.h
/// @brief Interactive connection configuration pop-up dialog (Mock, Serial, TCP).

#include "Canvas.h"
#include "ITransport.h"
#include "Terminal.h"

#include <cstdint>
#include <memory>
#include <string>

namespace PelcoDTui {

/// @enum TransportType
/// @brief Selected transport protocol mechanism.
enum class TransportType : std::uint8_t { Mock, Tcp, Serial };

/// @struct ConnectionConfig
/// @brief Configuration settings required to establish device communication.
struct ConnectionConfig {
    TransportType type { TransportType::Mock };
    std::string tcpHost { "127.0.0.1" };
    std::uint16_t tcpPort { 9000U };
    std::string serialPort { "/dev/ttyUSB0" };
    std::uint32_t serialBaud { 9600U };
    std::uint8_t address { 1U };
};

/// @class ConnectionModal
/// @brief Pop-up modal dialog for configuring and switching communication transports.
class ConnectionModal {
public:
    ConnectionModal();
    ~ConnectionModal() = default;

    void setOpen(bool open) noexcept;
    [[nodiscard]] bool isOpen() const noexcept
    {
        return m_isOpen;
    }

    void render(Canvas& canvas, int screenWidth, int screenHeight);

    /// @brief Handle keyboard input while modal is active.
    /// @return True if modal consumed the input event.
    bool handleInput(const InputEvent& event);

    /// @brief Check if user confirmed connection changes.
    [[nodiscard]] bool hasPendingConnect() noexcept;

    /// @brief Retrieve updated connection configuration.
    [[nodiscard]] ConnectionConfig getConfig() const noexcept
    {
        return m_config;
    }

    void setConfig(const ConnectionConfig& config);

    /// @brief Instantiate the configured transport instance.
    [[nodiscard]] std::shared_ptr<PelcoD::ITransport> createTransport() const;

private:
    [[nodiscard]] bool isTextEditingField() const noexcept;
    void resetCursor() noexcept;
    bool validateAndApply();
    void renderTextField(Canvas& canvas, int x, int y, std::string_view text, bool isSelected, const Style& textStyle,
        const Style& cursorStyle) const;

    bool m_isOpen { false };
    bool m_pendingConnect { false };
    int m_selectedField { 0 };
    int m_cursorPos { 0 };
    std::string m_tcpPortStr { "9000" };
    std::string m_errorMessage {};
    ConnectionConfig m_config {};
};

} // namespace PelcoDTui
