/// @file SerialTransport.cpp
/// @brief Implementation of cross-platform serial transport.

#include "SerialTransport.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <glog/logging.h>

namespace PelcoD {

namespace {

#ifdef _WIN32
    std::string getWin32ErrorString(DWORD errCode)
    {
        char* errText = nullptr;
        const DWORD len = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
            errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&errText), 0, nullptr);
        std::string msg = (len > 0 && errText) ? std::string(errText) : "Error code " + std::to_string(errCode);
        if (errText != nullptr) {
            LocalFree(errText);
        }
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) {
            msg.pop_back();
        }
        return msg;
    }
#else
    speed_t getTermiosSpeed(std::uint32_t baudRate) noexcept
    {
        switch (baudRate) {
        case 2400U:
            return B2400;
        case 4800U:
            return B4800;
        case 9600U:
            return B9600;
        case 19200U:
            return B19200;
        case 38400U:
            return B38400;
        case 57600U:
            return B57600;
        case 115200U:
            return B115200;
        default:
            return B9600;
        }
    }
#endif

    bool isValidBaudRate(std::uint32_t baudRate) noexcept
    {
        switch (baudRate) {
        case 2400U:
        case 4800U:
        case 9600U:
        case 19200U:
        case 38400U:
        case 57600U:
        case 115200U:
            return true;
        default:
            return false;
        }
    }

} // namespace

SerialTransport::SerialTransport(std::string portName, std::uint32_t baudRate)
    : m_portName { std::move(portName) }
    , m_baudRate { isValidBaudRate(baudRate) ? baudRate : 2400U }
{
}

SerialTransport::~SerialTransport()
{
    close();
}

void SerialTransport::setPortName(const std::string& portName)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    m_portName = portName;
}

std::string SerialTransport::getPortName() const
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    return m_portName;
}

void SerialTransport::setBaudRate(std::uint32_t baudRate)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if (!isValidBaudRate(baudRate)) {
        LOG(WARNING) << "Unsupported baud rate " << baudRate << ", defaulting to 2400 baud";
        m_baudRate = 2400U;
    } else {
        m_baudRate = baudRate;
    }
}

std::uint32_t SerialTransport::getBaudRate() const noexcept
{
    return m_baudRate;
}

bool SerialTransport::open()
{
    close();

    std::string port;
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        port = m_portName;
    }

    if (port.empty() || port.size() > 256U) {
        LOG(ERROR) << "Cannot open serial port: port name is empty or exceeds 256 characters";
        notifyState(TransportState::Error, "Serial port name is invalid");
        return false;
    }

    LOG(INFO) << "Opening serial port '" << port << "' at " << m_baudRate << " baud";
    notifyState(TransportState::Connecting, "Opening " + port);

#ifdef _WIN32
    std::string portPath = port;
    if (portPath.rfind("\\\\.\\", 0) != 0 && portPath.rfind("COM", 0) == 0) {
        portPath = "\\\\.\\" + portPath;
    }

    HANDLE hComm = ::CreateFileA(
        portPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hComm == INVALID_HANDLE_VALUE) {
        const std::string errStr = getWin32ErrorString(::GetLastError());
        notifyState(TransportState::Error, "Failed to open " + port + ": " + errStr);
        return false;
    }

    m_handle = hComm;
#else
    const int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        const std::string errStr = std::strerror(errno);
        LOG(ERROR) << "Failed to open serial port '" << port << "': " << errStr;
        notifyState(TransportState::Error, "Failed to open " + port + ": " + errStr);
        return false;
    }

    m_handle = fd;
#endif

    if (!configurePort()) {
#ifdef _WIN32
        ::CloseHandle(m_handle);
#else
        ::close(m_handle);
#endif
        m_handle = INVALID_SERIAL_HANDLE;
        return false;
    }

    m_running = true;
    m_readThread = std::thread(&SerialTransport::readWorker, this);

    LOG(INFO) << "Serial port '" << port << "' opened and configured successfully";
    notifyState(TransportState::Connected, "Connected to " + port);
    return true;
}

bool SerialTransport::configurePort()
{
    std::uint32_t baud { 9600U };
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        baud = m_baudRate;
    }

#ifdef _WIN32
    DCB dcbSerialParams {};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!::GetCommState(m_handle, &dcbSerialParams)) {
        const std::string errStr = getWin32ErrorString(::GetLastError());
        notifyState(TransportState::Error, "GetCommState failed: " + errStr);
        return false;
    }

    dcbSerialParams.BaudRate = baud;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fOutxCtsFlow = FALSE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
    dcbSerialParams.fOutX = FALSE;
    dcbSerialParams.fInX = FALSE;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

    if (!::SetCommState(m_handle, &dcbSerialParams)) {
        const std::string errStr = getWin32ErrorString(::GetLastError());
        notifyState(TransportState::Error, "SetCommState failed: " + errStr);
        return false;
    }

    COMMTIMEOUTS timeouts {};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!::SetCommTimeouts(m_handle, &timeouts)) {
        const std::string errStr = getWin32ErrorString(::GetLastError());
        notifyState(TransportState::Error, "SetCommTimeouts failed: " + errStr);
        return false;
    }

    ::PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;

#else
    struct termios tty { };
    if (::tcgetattr(m_handle, &tty) != 0) {
        const std::string errStr = std::strerror(errno);
        notifyState(TransportState::Error, "tcgetattr failed: " + errStr);
        return false;
    }

    const speed_t spd = getTermiosSpeed(baud);
    ::cfsetospeed(&tty, spd);
    ::cfsetispeed(&tty, spd);

    tty.c_cflag &= static_cast<tcflag_t>(~PARENB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    tty.c_cflag |= CS8;
    tty.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);

#ifdef CRTSCTS
    tty.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
#endif

    tty.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ISIG));
    tty.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR));
    tty.c_oflag &= static_cast<tcflag_t>(~OPOST);

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (::tcsetattr(m_handle, TCSANOW, &tty) != 0) {
        const std::string errStr = std::strerror(errno);
        notifyState(TransportState::Error, "tcsetattr failed: " + errStr);
        return false;
    }

    ::tcflush(m_handle, TCIOFLUSH);
    return true;
#endif
}

void SerialTransport::close()
{
    m_running = false;

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    if (m_handle != INVALID_SERIAL_HANDLE) {
        LOG(INFO) << "Closing serial port";
#ifdef _WIN32
        ::CloseHandle(m_handle);
#else
        ::close(m_handle);
#endif
        m_handle = INVALID_SERIAL_HANDLE;
        notifyState(TransportState::Disconnected, "Port closed");
    }
}

bool SerialTransport::isOpen() const noexcept
{
    return (m_handle != INVALID_SERIAL_HANDLE) && m_running.load();
}

bool SerialTransport::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_writeMutex);

#ifdef _WIN32
    DWORD bytesWritten = 0;
    const BOOL success = ::WriteFile(m_handle, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr);
    return success && (bytesWritten == static_cast<DWORD>(data.size()));

#else
    std::size_t totalWritten { 0U };
    const std::size_t toWrite { data.size() };

    while (totalWritten < toWrite && m_running.load()) {
        const ssize_t written = ::write(m_handle, data.data() + totalWritten, toWrite - totalWritten);

        if (written > 0) {
            totalWritten += static_cast<std::size_t>(written);
        } else if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd { };
                pfd.fd = m_handle;
                pfd.events = POLLOUT;
                ::poll(&pfd, 1, 50);
                continue;
            }
            return false;
        }
    }

    if (totalWritten == toWrite) {
        VLOG(1) << "Serial TX: " << totalWritten << " bytes";
        return true;
    }
    LOG(WARNING) << "Serial TX incomplete: wrote " << totalWritten << " of " << toWrite << " bytes";
    return false;
#endif
}

void SerialTransport::setDataCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dataCallback = std::move(callback);
}

void SerialTransport::setStateCallback(StateChangedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

void SerialTransport::readWorker()
{
    std::vector<std::uint8_t> buffer(512U, 0x00U);

#ifdef _WIN32
    while (m_running.load()) {
        DWORD bytesRead = 0;
        const BOOL success
            = ::ReadFile(m_handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);

        if (success) {
            if (bytesRead > 0) {
                std::vector<std::uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);
                DataReceivedCallback cb;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    cb = m_dataCallback;
                }
                if (cb) {
                    cb(chunk);
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            if (m_running.load()) {
                const std::string errStr = getWin32ErrorString(::GetLastError());
                notifyState(TransportState::Error, "Serial read error: " + errStr);
            }
            break;
        }
    }

#else
    while (m_running.load()) {
        struct pollfd pfd { };
        pfd.fd = m_handle;
        pfd.events = POLLIN;

        const int ret = ::poll(&pfd, 1, 50);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            const ssize_t bytesRead = ::read(m_handle, buffer.data(), buffer.size());
            if (bytesRead > 0) {
                std::vector<std::uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);

                DataReceivedCallback cb;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    cb = m_dataCallback;
                }
                if (cb) {
                    cb(chunk);
                }
            } else if (bytesRead < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
                notifyState(TransportState::Error, "Serial read error");
                break;
            }
        } else if (ret < 0 && errno != EINTR) {
            notifyState(TransportState::Error, "Serial poll error");
            break;
        }
    }
#endif
}

void SerialTransport::notifyState(TransportState state, const std::string& errorMsg)
{
    StateChangedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_stateCallback;
    }
    if (cb) {
        cb(state, errorMsg);
    }
}

namespace {
bool naturalLess(const std::string& a, const std::string& b)
{
    std::size_t i { 0U };
    std::size_t j { 0U };
    while (i < a.size() && j < b.size()) {
        if (std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j]))) {
            std::size_t endA { i };
            while (endA < a.size() && std::isdigit(static_cast<unsigned char>(a[endA]))) {
                ++endA;
            }
            std::size_t endB { j };
            while (endB < b.size() && std::isdigit(static_cast<unsigned char>(b[endB]))) {
                ++endB;
            }
            const unsigned long long numA = std::stoull(a.substr(i, endA - i));
            const unsigned long long numB = std::stoull(b.substr(j, endB - j));
            if (numA != numB) {
                return numA < numB;
            }
            i = endA;
            j = endB;
        } else {
            if (a[i] != b[j]) {
                return a[i] < b[j];
            }
            ++i;
            ++j;
        }
    }
    return a.size() < b.size();
}
} // namespace

std::vector<std::string> SerialTransport::enumeratePorts()
{
    std::vector<std::string> ports;

#if defined(_WIN32)
    // 1. Query Windows Registry: HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
    HKEY hKey = nullptr;
    if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char valueName[256];
        char data[256];
        DWORD index = 0;
        while (true) {
            DWORD valueNameLen = sizeof(valueName);
            DWORD dataLen = sizeof(data);
            DWORD type = 0;
            const LONG status = ::RegEnumValueA(
                hKey, index++, valueName, &valueNameLen, nullptr, &type, reinterpret_cast<LPBYTE>(data), &dataLen);
            if (status == ERROR_NO_MORE_ITEMS) {
                break;
            }
            if (status == ERROR_SUCCESS && (type == REG_SZ || type == REG_MULTI_SZ) && dataLen > 0) {
                const std::size_t safeLen = (dataLen < sizeof(data)) ? dataLen : (sizeof(data) - 1);
                data[safeLen] = '\0';
                std::string portName(data);
                while (!portName.empty() && std::isspace(static_cast<unsigned char>(portName.back()))) {
                    portName.pop_back();
                }
                if (!portName.empty() && std::find(ports.begin(), ports.end(), portName) == ports.end()) {
                    ports.push_back(portName);
                }
            }
        }
        ::RegCloseKey(hKey);
    }

    // 2. Query MS-DOS device map via QueryDosDeviceA for virtual or redirected COM ports
    char dosBuf[65536];
    const DWORD charsRead = ::QueryDosDeviceA(nullptr, dosBuf, sizeof(dosBuf));
    if (charsRead > 0) {
        const char* current = dosBuf;
        while (*current != '\0') {
            const std::string devName(current);
            // Must strictly match "COM" followed only by digits (e.g. COM1, COM12)
            if (devName.rfind("COM", 0) == 0 && devName.size() > 3) {
                bool onlyDigits = true;
                for (std::size_t i = 3; i < devName.size(); ++i) {
                    if (!std::isdigit(static_cast<unsigned char>(devName[i]))) {
                        onlyDigits = false;
                        break;
                    }
                }
                if (onlyDigits && std::find(ports.begin(), ports.end(), devName) == ports.end()) {
                    ports.push_back(devName);
                }
            }
            current += devName.size() + 1;
        }
    }

#elif defined(__linux__)
    // Scan /dev for standard Linux serial device nodes
    try {
        if (std::filesystem::exists("/dev")) {
            for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
                const std::string filename = entry.path().filename().string();
                if (filename.rfind("ttyUSB", 0) == 0 || filename.rfind("ttyACM", 0) == 0 ||
                    filename.rfind("ttyS", 0) == 0 || filename.rfind("rfcomm", 0) == 0) {
                    const std::string fullPath = entry.path().string();
                    if (std::find(ports.begin(), ports.end(), fullPath) == ports.end()) {
                        ports.push_back(fullPath);
                    }
                }
            }
        }
    } catch (...) {
    }

#elif defined(__APPLE__)
    // Scan /dev for standard macOS serial device nodes
    try {
        if (std::filesystem::exists("/dev")) {
            for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
                const std::string filename = entry.path().filename().string();
                if (filename.rfind("cu.", 0) == 0 || filename.rfind("tty.", 0) == 0) {
                    const std::string fullPath = entry.path().string();
                    if (std::find(ports.begin(), ports.end(), fullPath) == ports.end()) {
                        ports.push_back(fullPath);
                    }
                }
            }
        }
    } catch (...) {
    }
#endif

    std::sort(ports.begin(), ports.end(), naturalLess);
    return ports;
}

} // namespace PelcoD

