#include "Terminal.h"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace PelcoDTui {

std::atomic<bool> Terminal::s_resized { false };
static struct termios s_origTermios {
};
static bool s_hasOrigTermios { false };

static void handleSigwinch([[maybe_unused]] int sig)
{
    Terminal::triggerResize();
}

void Terminal::triggerResize() noexcept
{
    s_resized.store(true, std::memory_order_relaxed);
}

Terminal::Terminal()
{
    enableRawMode();
    setupSignalHandlers();

    // Switch to alternate screen, hide cursor, enable mouse reporting
    const std::string initSeq = "\033[?1049h\033[?25l\033[?1000h\033[?1002h\033[?1006h";
    writeRaw(initSeq);
    flush();
}

Terminal::~Terminal()
{
    // Disable mouse, show cursor, leave alternate screen
    const std::string exitSeq = "\033[?1006l\033[?1002l\033[?1000l\033[?25h\033[?1049l\033[0m";
    writeRaw(exitSeq);
    flush();

    restoreSignalHandlers();
    disableRawMode();
}

void Terminal::enableRawMode()
{
    if (m_rawEnabled || !isatty(STDIN_FILENO)) {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &s_origTermios) == 0) {
        s_hasOrigTermios = true;
        struct termios raw = s_origTermios;
        raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
        raw.c_oflag &= static_cast<tcflag_t>(~(OPOST));
        raw.c_cflag |= static_cast<tcflag_t>(CS8);
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
            m_rawEnabled = true;
        }
    }
}

void Terminal::disableRawMode() noexcept
{
    if (m_rawEnabled && s_hasOrigTermios) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_origTermios);
        m_rawEnabled = false;
    }
}

void Terminal::setupSignalHandlers()
{
    struct sigaction sa { };
    sa.sa_handler = handleSigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
}

void Terminal::restoreSignalHandlers() noexcept
{
    struct sigaction sa { };
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
}

TerminalSize Terminal::getSize() const noexcept
{
    struct winsize ws { };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return TerminalSize { static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row) };
    }
    return TerminalSize { 80, 24 };
}

bool Terminal::checkResize() noexcept
{
    bool expected = true;
    if (s_resized.compare_exchange_strong(expected, false)) {
        return true;
    }
    return false;
}

void Terminal::writeRaw(const std::string& buffer)
{
    if (!buffer.empty()) {
        const ssize_t ret = ::write(STDOUT_FILENO, buffer.data(), buffer.size());
        (void)ret;
    }
}

void Terminal::flush()
{
    ::fflush(stdout);
}

InputEvent Terminal::pollEvent(int timeoutMs)
{
    struct pollfd pfd { };
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    const int pollRet = ::poll(&pfd, 1, timeoutMs);
    if (pollRet <= 0 || !(pfd.revents & POLLIN)) {
        return InputEvent { Key::None, '\0', {} };
    }

    char c = '\0';
    const ssize_t bytesRead = ::read(STDIN_FILENO, &c, 1);
    if (bytesRead <= 0) {
        return InputEvent { Key::None, '\0', {} };
    }

    // Normal ASCII / control keys
    if (c == 0x1B) { // ESC sequence
        // Check if more characters follow immediately
        struct pollfd escPfd { };
        escPfd.fd = STDIN_FILENO;
        escPfd.events = POLLIN;
        if (::poll(&escPfd, 1, 15) <= 0) {
            return InputEvent { Key::Escape, '\0', {} };
        }

        char seq[64] = {};
        ssize_t seqLen = ::read(STDIN_FILENO, seq, sizeof(seq) - 1);
        if (seqLen <= 0) {
            return InputEvent { Key::Escape, '\0', {} };
        }
        seq[seqLen] = '\0';

        if (seq[0] == '[') {
            if (seq[1] == '<') {
                // SGR mouse mode: \033[<b;x;yM or m
                int btn = 0;
                int mx = 0;
                int my = 0;
                char termChar = 'M';
                if (std::sscanf(seq + 2, "%d;%d;%d%c", &btn, &mx, &my, &termChar) >= 3) {
                    MouseEvent me {};
                    me.x = mx;
                    me.y = my;
                    me.button = btn;
                    me.isRelease = (termChar == 'm');
                    return InputEvent { Key::MouseClick, '\0', me };
                }
            }
            switch (seq[1]) {
            case 'A':
                return InputEvent { Key::Up, '\0', {} };
            case 'B':
                return InputEvent { Key::Down, '\0', {} };
            case 'C':
                return InputEvent { Key::Right, '\0', {} };
            case 'D':
                return InputEvent { Key::Left, '\0', {} };
            case 'H':
                return InputEvent { Key::Home, '\0', {} };
            case 'F':
                return InputEvent { Key::End, '\0', {} };
            case 'Z':
                return InputEvent { Key::Backtab, '\0', {} };
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6': {
                if (seq[2] == '~') {
                    if (seq[1] == '1')
                        return InputEvent { Key::Home, '\0', {} };
                    if (seq[1] == '3')
                        return InputEvent { Key::Delete, '\0', {} };
                    if (seq[1] == '4')
                        return InputEvent { Key::End, '\0', {} };
                    if (seq[1] == '5')
                        return InputEvent { Key::PageUp, '\0', {} };
                    if (seq[1] == '6')
                        return InputEvent { Key::PageDown, '\0', {} };
                }
                // F keys: 11~ to 24~
                if (seq[1] == '1' && seq[2] == '1')
                    return InputEvent { Key::F1, '\0', {} };
                if (seq[1] == '1' && seq[2] == '2')
                    return InputEvent { Key::F2, '\0', {} };
                if (seq[1] == '1' && seq[2] == '3')
                    return InputEvent { Key::F3, '\0', {} };
                if (seq[1] == '1' && seq[2] == '4')
                    return InputEvent { Key::F4, '\0', {} };
                if (seq[1] == '1' && seq[2] == '5')
                    return InputEvent { Key::F5, '\0', {} };
                if (seq[1] == '1' && seq[2] == '7')
                    return InputEvent { Key::F6, '\0', {} };
                if (seq[1] == '1' && seq[2] == '8')
                    return InputEvent { Key::F7, '\0', {} };
                if (seq[1] == '1' && seq[2] == '9')
                    return InputEvent { Key::F8, '\0', {} };
                if (seq[1] == '2' && seq[2] == '0')
                    return InputEvent { Key::F9, '\0', {} };
                if (seq[1] == '2' && seq[2] == '1')
                    return InputEvent { Key::F10, '\0', {} };
                if (seq[1] == '2' && seq[2] == '3')
                    return InputEvent { Key::F11, '\0', {} };
                if (seq[1] == '2' && seq[2] == '4')
                    return InputEvent { Key::F12, '\0', {} };
                break;
            }
            default:
                break;
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'P':
                return InputEvent { Key::F1, '\0', {} };
            case 'Q':
                return InputEvent { Key::F2, '\0', {} };
            case 'R':
                return InputEvent { Key::F3, '\0', {} };
            case 'S':
                return InputEvent { Key::F4, '\0', {} };
            default:
                break;
            }
        }
        return InputEvent { Key::Escape, '\0', {} };
    }

    if (c == '\r' || c == '\n') {
        return InputEvent { Key::Enter, '\0', {} };
    }
    if (c == '\t') {
        return InputEvent { Key::Tab, '\0', {} };
    }
    if (c == 127 || c == '\b') {
        return InputEvent { Key::Backspace, '\0', {} };
    }
    if (c == ' ') {
        return InputEvent { Key::Space, ' ', {} };
    }

    return InputEvent { Key::Character, c, {} };
}

} // namespace PelcoDTui
