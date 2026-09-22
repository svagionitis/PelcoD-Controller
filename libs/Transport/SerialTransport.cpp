/// @file SerialTransport.cpp
/// @brief Implementation of cross-platform serial transport.

#include "SerialTransport.h"

#ifndef _WIN32
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

namespace Transport {

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
    std::scoped_lock lock(m_writeMutex);
    m_portName = portName;
}

std::string SerialTransport::getPortName() const
{
    std::scoped_lock lock(m_writeMutex);
    return m_portName;
}

bool SerialTransport::setBaudRate(std::uint32_t baudRate)
{
    if (!isValidBaudRate(baudRate)) {
        LOG(WARNING) << "Unsupported baud rate " << baudRate;
        return false;
    }

    {
        std::scoped_lock lock(m_writeMutex);
        m_baudRate.store(baudRate);
    }

    if (m_handle.load() != INVALID_SERIAL_HANDLE) {
        return configurePort();
    }
    return true;
}

std::uint32_t SerialTransport::getBaudRate() const noexcept
{
    return m_baudRate.load();
}

bool SerialTransport::open()
{
    close();

    std::string port;
    {
        std::scoped_lock lock(m_writeMutex);
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

    m_handle.store(hComm);
#else
    const int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        const std::string errStr = std::strerror(errno);
        LOG(ERROR) << "Failed to open serial port '" << port << "': " << errStr;
        notifyState(TransportState::Error, "Failed to open " + port + ": " + errStr);
        return false;
    }

    m_handle.store(fd);
#endif

    if (!configurePort()) {
        const SerialHandle h = m_handle.exchange(INVALID_SERIAL_HANDLE);
        if (h != INVALID_SERIAL_HANDLE) {
#ifdef _WIN32
            ::CloseHandle(h);
#else
            ::close(h);
#endif
        }
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
        std::scoped_lock lock(m_writeMutex);
        baud = m_baudRate;
    }

    const SerialHandle handle = m_handle.load();
    if (handle == INVALID_SERIAL_HANDLE) {
        return false;
    }

#ifdef _WIN32
    DCB dcbSerialParams {};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!::GetCommState(handle, &dcbSerialParams)) {
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

    if (!::SetCommState(handle, &dcbSerialParams)) {
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

    if (!::SetCommTimeouts(handle, &timeouts)) {
        const std::string errStr = getWin32ErrorString(::GetLastError());
        notifyState(TransportState::Error, "SetCommTimeouts failed: " + errStr);
        return false;
    }

    ::PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;

#else
    struct termios tty { };
    if (::tcgetattr(handle, &tty) != 0) {
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

    if (::tcsetattr(handle, TCSANOW, &tty) != 0) {
        const std::string errStr = std::strerror(errno);
        notifyState(TransportState::Error, "tcsetattr failed: " + errStr);
        return false;
    }

    ::tcflush(handle, TCIOFLUSH);
    return true;
#endif
}

void SerialTransport::close()
{
    stopReadThread();

    bool wasClosed { false };
    {
        std::scoped_lock lock(m_writeMutex);
        const SerialHandle handle = m_handle.exchange(INVALID_SERIAL_HANDLE);
        if (handle != INVALID_SERIAL_HANDLE) {
            LOG(INFO) << "Closing serial port";
#ifdef _WIN32
            ::CloseHandle(handle);
#else
            ::close(handle);
#endif
            wasClosed = true;
        }
    }

    if (wasClosed) {
        notifyState(TransportState::Disconnected, "Port closed");
    }
}

bool SerialTransport::isOpen() const noexcept
{
    return m_running.load() && (m_handle.load() != INVALID_SERIAL_HANDLE);
}

bool SerialTransport::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    std::scoped_lock lock(m_writeMutex);
    const SerialHandle handle = m_handle.load();
    if (handle == INVALID_SERIAL_HANDLE || !m_running.load()) {
        return false;
    }

#ifdef _WIN32
    DWORD bytesWritten = 0;
    const BOOL success = ::WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr);
    return success && (bytesWritten == static_cast<DWORD>(data.size()));

#else
    std::size_t totalWritten { 0U };
    const std::size_t toWrite { data.size() };

    while (totalWritten < toWrite && m_running.load()) {
        const ssize_t written = ::write(handle, data.data() + totalWritten, toWrite - totalWritten);

        if (written > 0) {
            totalWritten += static_cast<std::size_t>(written);
        } else if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd { };
                pfd.fd = handle;
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

void SerialTransport::readWorker()
{
    std::vector<std::uint8_t> buffer(512U, 0x00U);
    bool unrecoverableError { false };

#ifdef _WIN32
    while (m_running.load()) {
        const SerialHandle handle = m_handle.load();
        if (handle == INVALID_SERIAL_HANDLE) {
            break;
        }
        DWORD bytesRead = 0;
        const BOOL success = ::ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);

        if (success) {
            if (bytesRead > 0) {
                std::vector<std::uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);
                invokeDataCallback(chunk);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            if (m_running.exchange(false)) {
                unrecoverableError = true;
                const std::string errStr = getWin32ErrorString(::GetLastError());
                notifyState(TransportState::Error, "Serial read error: " + errStr);
            }
            break;
        }
    }

#else
    while (m_running.load()) {
        const SerialHandle handle = m_handle.load();
        if (handle == INVALID_SERIAL_HANDLE) {
            break;
        }
        struct pollfd pfd { };
        pfd.fd = handle;
        pfd.events = POLLIN;

        const int ret = ::poll(&pfd, 1, 50);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            const ssize_t bytesRead = ::read(handle, buffer.data(), buffer.size());
            if (bytesRead > 0) {
                std::vector<std::uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);

                invokeDataCallback(chunk);
            } else if (bytesRead < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
                if (m_running.exchange(false)) {
                    unrecoverableError = true;
                    notifyState(TransportState::Error, "Serial read error");
                }
                break;
            }
        } else if (ret < 0 && errno != EINTR) {
            if (m_running.exchange(false)) {
                unrecoverableError = true;
                notifyState(TransportState::Error, "Serial poll error");
            }
            break;
        }
    }
#endif

    if (unrecoverableError) {
        std::scoped_lock lock(m_writeMutex);
        const SerialHandle handle = m_handle.exchange(INVALID_SERIAL_HANDLE);
        if (handle != INVALID_SERIAL_HANDLE) {
#ifdef _WIN32
            ::CloseHandle(handle);
#else
            ::close(handle);
#endif
        }
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
                if (filename.rfind("ttyUSB", 0) == 0 || filename.rfind("ttyACM", 0) == 0
                    || filename.rfind("ttyS", 0) == 0 || filename.rfind("rfcomm", 0) == 0) {
                    const std::string fullPath = entry.path().string();
                    if (std::find(ports.begin(), ports.end(), fullPath) == ports.end()) {
                        ports.push_back(fullPath);
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG(WARNING) << "SerialTransport: failed to iterate /dev: " << ex.what();
    } catch (const std::exception& ex) {
        LOG(WARNING) << "SerialTransport: unexpected error during /dev scan: " << ex.what();
    } catch (...) {
        LOG(WARNING) << "SerialTransport: unknown exception during /dev scan";
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
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG(WARNING) << "SerialTransport: failed to iterate /dev: " << ex.what();
    } catch (const std::exception& ex) {
        LOG(WARNING) << "SerialTransport: unexpected error during /dev scan: " << ex.what();
    } catch (...) {
        LOG(WARNING) << "SerialTransport: unknown exception during /dev scan";
    }
#endif

    std::sort(ports.begin(), ports.end(), naturalLess);
    return ports;
}

} // namespace Transport
