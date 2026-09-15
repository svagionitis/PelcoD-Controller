#include "DeviceEnumerator.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dshow.h>
#include <objbase.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <glog/logging.h>

namespace PelcoD::Video {

std::vector<VideoDeviceInfo> DeviceEnumerator::enumerateDevices()
{
    std::vector<VideoDeviceInfo> devices;

#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool needCoUninit = SUCCEEDED(hr);

    ICreateDevEnum* devEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ICreateDevEnum, reinterpret_cast<void**>(&devEnum));
    if (SUCCEEDED(hr) && devEnum != nullptr) {
        IEnumMoniker* enumMoniker = nullptr;
        hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
        if (hr == S_OK && enumMoniker != nullptr) {
            IMoniker* moniker = nullptr;
            while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
                IPropertyBag* propBag = nullptr;
                hr = moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                            reinterpret_cast<void**>(&propBag));
                if (SUCCEEDED(hr) && propBag != nullptr) {
                    VARIANT varName;
                    VariantInit(&varName);
                    hr = propBag->Read(L"FriendlyName", &varName, nullptr);
                    if (SUCCEEDED(hr) && varName.vt == VT_BSTR && varName.bstrVal != nullptr) {
                        const int len = WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                        if (len > 0) {
                            std::string name(static_cast<std::size_t>(len - 1), '\0');
                            WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, name.data(), len, nullptr, nullptr);
                            VideoDeviceInfo info;
                            info.name = name;
                            info.path = "video=" + name;
                            info.description = "DirectShow Video Capture Device";
                            devices.push_back(std::move(info));
                        }
                    }
                    VariantClear(&varName);
                    propBag->Release();
                }
                moniker->Release();
            }
            enumMoniker->Release();
        }
        devEnum->Release();
    }

    if (needCoUninit) {
        CoUninitialize();
    }
#else
    for (int i = 0; i < 64; ++i) {
        const std::string devPath = "/dev/video" + std::to_string(i);
        const int fd = open(devPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        struct v4l2_capability cap {};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            if (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
                VideoDeviceInfo info;
                info.name = reinterpret_cast<const char*>(cap.card);
                info.path = devPath;
                info.description = reinterpret_cast<const char*>(cap.driver);
                devices.push_back(std::move(info));
            }
        }
        close(fd);
    }
#endif

    return devices;
}

} // namespace PelcoD::Video
