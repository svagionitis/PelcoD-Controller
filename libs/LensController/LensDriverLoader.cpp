/// @file LensDriverLoader.cpp
/// @brief Implementation of dynamic shared library loader and port collision guard.

#include "LensDriverLoader.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace LensControl {

namespace {

    extern "C" {
    using CreateLensObjectFn = ILensDriver* (*)();
    }

} // namespace

LensDriverLoader::LensDriverLoader() noexcept = default;

LensDriverLoader::~LensDriverLoader() noexcept
{
    reset();
}

LensDriverLoader::LensDriverLoader(LensDriverLoader&& other) noexcept
{
    std::lock_guard<std::mutex> lock { other.m_mutex };
    m_channels = other.m_channels;
    for (auto& ch : other.m_channels) {
        ch.moduleHandle = nullptr;
        ch.port = kInvalidPort;
        ch.loadedPath.clear();
    }
}

LensDriverLoader& LensDriverLoader::operator=(LensDriverLoader&& other) noexcept
{
    if (this != &other) {
        std::scoped_lock lock { m_mutex, other.m_mutex };
        reset();
        m_channels = other.m_channels;
        for (auto& ch : other.m_channels) {
            ch.moduleHandle = nullptr;
            ch.port = kInvalidPort;
            ch.loadedPath.clear();
        }
    }
    return *this;
}

LensLoadResult LensDriverLoader::loadDriver(CameraIndex cam, std::string_view libraryPath, std::uint8_t port,
    std::uint32_t baud, std::unique_ptr<ILensDriver>& outDriver) noexcept
{
    if (cam.value() >= kMaxChannels) {
        return LensLoadResult::InitFailed;
    }
    if (libraryPath.empty()) {
        return LensLoadResult::FileNotFound;
    }

    std::lock_guard<std::mutex> lock { m_mutex };

    if (isPortBoundLocked(port, cam)) {
        return LensLoadResult::PortCollision;
    }

    // If channel had a previous driver loaded, unload previous module handle
    const std::size_t idx = static_cast<std::size_t>(cam.value());
    if (m_channels[idx].moduleHandle != nullptr) {
        closeLibrary(m_channels[idx].moduleHandle);
        m_channels[idx].moduleHandle = nullptr;
        m_channels[idx].port = kInvalidPort;
        m_channels[idx].loadedPath.clear();
    }

    void* handle = openLibrary(libraryPath);
    if (handle == nullptr) {
        return LensLoadResult::FileNotFound;
    }

    auto createFn = reinterpret_cast<CreateLensObjectFn>(resolveSymbol(handle, "create_lens_object"));
    if (createFn == nullptr) {
        createFn = reinterpret_cast<CreateLensObjectFn>(resolveSymbol(handle, "create_object"));
    }

    if (createFn == nullptr) {
        closeLibrary(handle);
        return LensLoadResult::MissingCreateSymbol;
    }

    ILensDriver* rawDriver = createFn();
    if (rawDriver == nullptr) {
        closeLibrary(handle);
        return LensLoadResult::InitFailed;
    }

    std::unique_ptr<ILensDriver> driver { rawDriver };
    if (!driver->initialize(port, baud)) {
        driver.reset();
        closeLibrary(handle);
        return LensLoadResult::InitFailed;
    }

    m_channels[idx].moduleHandle = handle;
    m_channels[idx].port = port;
    m_channels[idx].loadedPath = std::string { libraryPath };
    outDriver = std::move(driver);

    return LensLoadResult::Success;
}

void LensDriverLoader::unloadDriver(CameraIndex cam) noexcept
{
    if (cam.value() >= kMaxChannels) {
        return;
    }

    std::lock_guard<std::mutex> lock { m_mutex };
    const std::size_t idx = static_cast<std::size_t>(cam.value());
    if (m_channels[idx].moduleHandle != nullptr) {
        closeLibrary(m_channels[idx].moduleHandle);
        m_channels[idx].moduleHandle = nullptr;
    }
    m_channels[idx].port = kInvalidPort;
    m_channels[idx].loadedPath.clear();
}

bool LensDriverLoader::isPortBound(std::uint8_t port) const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    for (std::size_t i = 0U; i < kMaxChannels; ++i) {
        if (m_channels[i].port == port && port != kInvalidPort) {
            return true;
        }
    }
    return false;
}

std::uint8_t LensDriverLoader::boundPort(CameraIndex cam) const noexcept
{
    if (cam.value() >= kMaxChannels) {
        return kInvalidPort;
    }
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_channels[static_cast<std::size_t>(cam.value())].port;
}

void LensDriverLoader::reset() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    for (std::size_t i = 0U; i < kMaxChannels; ++i) {
        if (m_channels[i].moduleHandle != nullptr) {
            closeLibrary(m_channels[i].moduleHandle);
            m_channels[i].moduleHandle = nullptr;
        }
        m_channels[i].port = kInvalidPort;
        m_channels[i].loadedPath.clear();
    }
}

bool LensDriverLoader::isPortBoundLocked(std::uint8_t port, CameraIndex exemptCam) const noexcept
{
    if (port == kInvalidPort) {
        return false;
    }
    for (std::size_t i = 0U; i < kMaxChannels; ++i) {
        if (i == static_cast<std::size_t>(exemptCam.value())) {
            continue;
        }
        if (m_channels[i].port == port) {
            return true;
        }
    }
    return false;
}

void* LensDriverLoader::openLibrary(std::string_view path) noexcept
{
    const std::string pathStr { path };
    std::vector<std::string> candidates { };
    candidates.push_back(pathStr);

    const bool hasDirSep = (path.find('/') != std::string_view::npos) || (path.find('\\') != std::string_view::npos);

    if (!hasDirSep) {
        candidates.push_back("./plugins/lens/" + pathStr);
        candidates.push_back("./" + pathStr);
#if defined(_WIN32) || defined(_WIN64)
        if (pathStr.size() < 4U || pathStr.substr(pathStr.size() - 4U) != ".dll") {
            candidates.push_back(pathStr + ".dll");
            candidates.push_back("./plugins/lens/" + pathStr + ".dll");
        }
#else
        if (pathStr.size() < 3U || pathStr.substr(pathStr.size() - 3U) != ".so") {
            candidates.push_back(pathStr + ".so");
            candidates.push_back("./plugins/lens/" + pathStr + ".so");
        }
#endif
    }

    for (const auto& candidate : candidates) {
#if defined(_WIN32) || defined(_WIN64)
        HMODULE handle = LoadLibraryA(candidate.c_str());
        if (handle != nullptr) {
            return static_cast<void*>(handle);
        }
#else
        void* handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle != nullptr) {
            return handle;
        }
#endif
    }

    return nullptr;
}

void LensDriverLoader::closeLibrary(void* handle) noexcept
{
    if (handle == nullptr) {
        return;
    }
#if defined(_WIN32) || defined(_WIN64)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* LensDriverLoader::resolveSymbol(void* handle, const char* symbol) noexcept
{
    if (handle == nullptr || symbol == nullptr) {
        return nullptr;
    }
#if defined(_WIN32) || defined(_WIN64)
    FARPROC proc = GetProcAddress(static_cast<HMODULE>(handle), symbol);
    return reinterpret_cast<void*>(proc);
#else
    return dlsym(handle, symbol);
#endif
}

} // namespace LensControl
