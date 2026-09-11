#pragma once

/// @file Terminal.h
/// @brief RAII POSIX termios management, signal handling, and non-blocking input parsing.

#include <atomic>
#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @enum Key
/// @brief Virtual key representations decoded from ANSI escape sequences.
enum class Key : std::uint16_t {
    None,
    Character,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    Tab,
    Backtab,
    Enter,
    Escape,
    Backspace,
    Delete,
    Space,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    MouseClick
};

/// @struct MouseEvent
/// @brief Terminal mouse click or motion coordinates.
struct MouseEvent {
    int x { 0 };
    int y { 0 };
    int button { 0 };
    bool isRelease { false };
};

/// @struct InputEvent
/// @brief Unified user input event parsed from standard input.
struct InputEvent {
    Key key { Key::None };
    char ch { '\0' };
    MouseEvent mouse {};
};

/// @struct TerminalSize
/// @brief Dimensions of the current terminal viewport.
struct TerminalSize {
    int width { 80 };
    int height { 24 };
};

/// @class Terminal
/// @brief RAII manager for POSIX termios raw mode, escape codes, and event reading.
class Terminal {
public:
    Terminal();
    ~Terminal();

    // Non-copyable, non-movable
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(Terminal&&) = delete;

    /// @brief Retrieve terminal dimensions.
    [[nodiscard]] TerminalSize getSize() const noexcept;

    /// @brief Signal that a terminal window resize event has occurred.
    static void triggerResize() noexcept;

    /// @brief Check if a terminal window resize event has occurred.
    [[nodiscard]] bool checkResize() noexcept;

    /// @brief Read a single non-blocking input event if available.
    /// @param[in] timeoutMs Maximum duration in milliseconds to poll stdin.
    /// @return Decoded InputEvent, or Key::None if no input was ready.
    [[nodiscard]] InputEvent pollEvent(int timeoutMs = 10);

    /// @brief Write raw buffer to stdout.
    void writeRaw(const std::string& buffer);

    /// @brief Flush stdout.
    void flush();

private:
    void enableRawMode();
    void disableRawMode() noexcept;
    void setupSignalHandlers();
    void restoreSignalHandlers() noexcept;

    bool m_rawEnabled { false };
    static std::atomic<bool> s_resized;
};

} // namespace PelcoDTui
