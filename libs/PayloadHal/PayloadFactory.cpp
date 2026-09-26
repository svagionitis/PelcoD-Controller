#include "PayloadFactory.h"
#include "GeoreferenceUtils.h"
#include "Transport/ITransport.h"
#include "Transport/SerialTransport.h"
#include "Transport/TcpTransport.h"
#include "Transport/UdpTransport.h"
#include "adapters/OnvifPayloadAdapter.h"
#include "adapters/PelcoDPtzAdapter.h"
#include "adapters/PelcoDFujinonPayloadAdapter.h"
#include "adapters/PelcoDViscaCompositePayload.h"
#include "adapters/ViscaSonyAdapter.h"
#include "adapters/LrfProtocols.h"
#include "adapters/SerialLrfAdapter.h"
#include "sim/SimulatedPayload.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <unordered_map>

namespace PayloadHal {

namespace {

    class PelcoDCameraUnit : public ICameraPayload {
    public:
        explicit PelcoDCameraUnit(std::shared_ptr<PelcoD::PelcoDDevice> device)
            : m_device(std::move(device))
        {
        }

        bool connect() override
        {
            return m_device && m_device->start();
        }
        void disconnect() override
        {
            if (m_device)
                m_device->stop();
        }
        [[nodiscard]] bool isConnected() const noexcept override
        {
            return m_device && m_device->isConnected();
        }
        [[nodiscard]] DeviceState state() const noexcept override
        {
            return (m_device && m_device->isConnected()) ? DeviceState::Ready : DeviceState::Disconnected;
        }
        [[nodiscard]] DeviceInfo info() const noexcept override
        {
            DeviceInfo d {};
            d.manufacturer = "Pelco";
            d.model = "Pelco-D Camera Optics";
            return d;
        }
        void registerStateCallback(StateCallback cb) override
        {
            m_stateCb = std::move(cb);
        }

        [[nodiscard]] CameraSpectrum spectrum() const noexcept override
        {
            return CameraSpectrum::DaylightVisible;
        }

        bool setZoomNormalized(double zoom01) override
        {
            if (!m_device)
                return false;
            const auto pos = static_cast<std::uint16_t>(std::clamp(zoom01, 0.0, 1.0) * 0xFFFF);
            m_device->setZoomPosition(pos);
            return true;
        }

        bool zoomContinuous(float velocity) override
        {
            if (!m_device)
                return false;
            if (velocity > 0.05f) {
                m_device->zoomTele();
            } else if (velocity < -0.05f) {
                m_device->zoomWide();
            } else {
                m_device->zoomStop();
            }
            return true;
        }

        bool zoomStop() override
        {
            if (!m_device)
                return false;
            m_device->zoomStop();
            return true;
        }

        bool setFocusAuto(bool enable) override
        {
            if (!m_device)
                return false;
            m_device->setAutoFocus(enable ? PelcoD::AutoMode::Auto : PelcoD::AutoMode::Off);
            return true;
        }

        bool setFocusNormalized(double /*focus01*/) override
        {
            return false;
        }

        bool focusContinuous(float velocity) override
        {
            if (!m_device)
                return false;
            if (velocity > 0.05f) {
                m_device->focusFar();
            } else if (velocity < -0.05f) {
                m_device->focusNear();
            } else {
                m_device->focusStop();
            }
            return true;
        }

        bool focusStop() override
        {
            if (!m_device)
                return false;
            m_device->focusStop();
            return true;
        }

        bool triggerOnePushFocus() override
        {
            if (!m_device)
                return false;
            m_device->focusNear();
            m_device->focusStop();
            return true;
        }

        bool setIrisAuto(bool enable) override
        {
            if (!m_device)
                return false;
            m_device->setAutoIris(enable ? PelcoD::AutoMode::Auto : PelcoD::AutoMode::Off);
            return true;
        }

        bool setIrisNormalized(double /*iris01*/) override
        {
            return false;
        }

        bool irisContinuous(float velocity) override
        {
            if (!m_device)
                return false;
            if (velocity > 0.05f) {
                m_device->irisOpen();
            } else if (velocity < -0.05f) {
                m_device->irisClose();
            } else {
                m_device->irisStop();
            }
            return true;
        }

        bool irisStop() override
        {
            if (!m_device)
                return false;
            m_device->irisStop();
            return true;
        }

        bool setDayNightIcr(bool /*nightMode*/) override
        {
            return false;
        }
        bool setDefog(bool /*enable*/) override
        {
            return false;
        }
        bool setStabilizer(bool /*enable*/) override
        {
            return false;
        }

        void registerTelemetryCallback(TelemetryCallback cb) override
        {
            m_telemCb = std::move(cb);
        }

        [[nodiscard]] CameraTelemetry currentTelemetry() const override
        {
            CameraTelemetry telem {};
            if (m_device) {
                const auto st = m_device->getStatus();
                const double mag = (st.magnification > 0U) ? (static_cast<double>(st.magnification) / 100.0) : 1.0;
                telem.opticalZoomFactor = std::max(1.0, mag);
                telem.normalizedZoom = (telem.opticalZoomFactor - 1.0) / 29.0;
                telem.autoFocusActive = (st.autoFocus == PelcoD::AutoMode::Auto);
                constexpr double kWideHfovRad { 60.0 * 3.14159265358979323846 / 180.0 };
                const double currentHfovRad = 2.0 * std::atan(std::tan(kWideHfovRad / 2.0) / telem.opticalZoomFactor);
                telem.horizontalFovDeg = currentHfovRad * (180.0 / 3.14159265358979323846);
                const double currentVfovRad = 2.0 * std::atan(std::tan(currentHfovRad / 2.0) * (9.0 / 16.0));
                telem.verticalFovDeg = currentVfovRad * (180.0 / 3.14159265358979323846);
            }
            telem.timestamp = std::chrono::system_clock::now();
            return telem;
        }

        std::string videoStreamUri(VideoStreamProfile profile = VideoStreamProfile::Primary) const override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_streamUris.find(profile);
            if (it != m_streamUris.end() && !it->second.empty()) {
                return it->second;
            }
            if (profile == VideoStreamProfile::Secondary) {
                auto primIt = m_streamUris.find(VideoStreamProfile::Primary);
                if (primIt != m_streamUris.end()) {
                    return primIt->second;
                }
            }
            return {};
        }

        bool setVideoStreamUri(const std::string& uri, VideoStreamProfile profile = VideoStreamProfile::Primary) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_streamUris[profile] = uri;
            return true;
        }

        std::vector<VideoStreamDescriptor> availableStreams() const override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::vector<VideoStreamDescriptor> list;
            for (const auto& [prof, uri] : m_streamUris) {
                if (uri.empty()) {
                    continue;
                }
                VideoStreamDescriptor desc;
                desc.uri = uri;
                desc.profile = prof;
                desc.transport = deduceTransportProtocol(uri);
                if (prof == VideoStreamProfile::Primary) {
                    desc.width = 1920;
                    desc.height = 1080;
                    desc.framerateFps = 30.0;
                    desc.encoding = "H264";
                    desc.isDefault = true;
                } else if (prof == VideoStreamProfile::Secondary) {
                    desc.width = 1280;
                    desc.height = 720;
                    desc.framerateFps = 30.0;
                    desc.encoding = "H264";
                    desc.isDefault = false;
                } else if (prof == VideoStreamProfile::Snapshot) {
                    desc.width = 1920;
                    desc.height = 1080;
                    desc.framerateFps = 0.0;
                    desc.encoding = "JPEG";
                    desc.isDefault = false;
                }
                list.push_back(desc);
            }
            return list;
        }

    private:
        std::shared_ptr<PelcoD::PelcoDDevice> m_device;
        mutable std::mutex m_mutex;
        TelemetryCallback m_telemCb {};
        StateCallback m_stateCb {};
        std::map<VideoStreamProfile, std::string> m_streamUris {};
    };

    class PelcoDCompositePayload : public IPayload {
    public:
        explicit PelcoDCompositePayload(std::shared_ptr<PelcoD::PelcoDDevice> device)
            : m_device(device)
            , m_ptu(std::make_shared<PelcoDPtzAdapter>(device))
            , m_camera(std::make_shared<PelcoDCameraUnit>(device))
        {
        }

        bool connect() override
        {
            return m_device && m_device->start();
        }
        void disconnect() override
        {
            if (m_device)
                m_device->stop();
        }
        [[nodiscard]] bool isConnected() const noexcept override
        {
            return m_device && m_device->isConnected();
        }
        [[nodiscard]] DeviceState state() const noexcept override
        {
            return (m_device && m_device->isConnected()) ? DeviceState::Ready : DeviceState::Disconnected;
        }
        [[nodiscard]] DeviceInfo info() const noexcept override
        {
            DeviceInfo d {};
            d.manufacturer = "Pelco";
            d.model = "Pelco-D Composite Station";
            return d;
        }
        void registerStateCallback(StateCallback cb) override
        {
            if (m_ptu)
                m_ptu->registerStateCallback(cb);
        }

        [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept override
        {
            return m_ptu;
        }
        [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept override
        {
            return m_camera;
        }
        [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept override
        {
            return nullptr;
        }
        void setLrf(std::shared_ptr<ILaserRangeFinder> lrf) noexcept
        {
            m_lrf = std::move(lrf);
        }

        [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override
        {
            return m_lrf;
        }

        [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
            const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override
        {
            if (!m_ptu) {
                return std::nullopt;
            }
            const GimbalTelemetry telem = m_ptu->currentTelemetry();
            const Klv::GeoPoint3D platform3D { platformGps.latitudeDeg, platformGps.longitudeDeg, platformAltMeters };

            // 1. Prefer precise slant range calculation if LRF return exists
            if (m_lrf) {
                if (const auto meas = m_lrf->lastMeasurement(); meas && meas->valid && meas->slantRangeMeters > 0.0) {
                    const auto targetSlant = GeoreferenceUtils::computeTargetFromSlantRange(
                        platform3D, platformHeadingDeg, telem.panAngleDeg, telem.tiltAngleDeg, meas->slantRangeMeters);
                    if (targetSlant.has_value()) {
                        return Klv::GeoPoint2D { targetSlant->latitudeDeg, targetSlant->longitudeDeg };
                    }
                }
            }

            // 2. Fallback to ground plane intersection
            auto target3D = GeoreferenceUtils::computeTargetFromGroundIntersection(
                platform3D, platformHeadingDeg, telem.panAngleDeg, telem.tiltAngleDeg, 0.0);
            if (target3D.has_value()) {
                return Klv::GeoPoint2D { target3D->latitudeDeg, target3D->longitudeDeg };
            }
            return std::nullopt;
        }

    private:
        std::shared_ptr<PelcoD::PelcoDDevice> m_device;
        std::shared_ptr<PelcoDPtzAdapter> m_ptu;
        std::shared_ptr<PelcoDCameraUnit> m_camera;
        std::shared_ptr<ILaserRangeFinder> m_lrf {};
    };

    struct ParsedUri {
        std::string scheme;
        std::string username;
        std::string password;
        std::string host;
        int port { -1 };
        std::string path;
        std::unordered_map<std::string, std::string> query;

        [[nodiscard]] std::string getQuery(const std::string& key, const std::string& defaultVal = "") const
        {
            const auto it = query.find(key);
            return (it != query.end()) ? it->second : defaultVal;
        }

        [[nodiscard]] int getIntQuery(const std::string& key, int defaultVal) const
        {
            const auto it = query.find(key);
            if (it != query.end()) {
                try {
                    return std::stoi(it->second);
                } catch (...) {
                    return defaultVal;
                }
            }
            return defaultVal;
        }
    };

    ParsedUri parseUriString(const std::string& uriString)
    {
        ParsedUri result {};
        if (uriString.empty()) {
            return result;
        }
        if (uriString == "sim") {
            result.scheme = "sim";
            return result;
        }

        const auto schemeEnd = uriString.find("://");
        if (schemeEnd == std::string::npos) {
            return result;
        }

        result.scheme = uriString.substr(0, schemeEnd);
        std::transform(result.scheme.begin(), result.scheme.end(), result.scheme.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::string rest = uriString.substr(schemeEnd + 3);

        const auto qPos = rest.find('?');
        if (qPos != std::string::npos) {
            const std::string queryStr = rest.substr(qPos + 1);
            rest = rest.substr(0, qPos);

            std::size_t start = 0;
            while (start < queryStr.length()) {
                const auto ampPos = queryStr.find('&', start);
                const std::string param
                    = (ampPos == std::string::npos) ? queryStr.substr(start) : queryStr.substr(start, ampPos - start);
                const auto eqPos = param.find('=');
                if (eqPos != std::string::npos) {
                    result.query[param.substr(0, eqPos)] = param.substr(eqPos + 1);
                } else if (!param.empty()) {
                    result.query[param] = "";
                }
                if (ampPos == std::string::npos) {
                    break;
                }
                start = ampPos + 1;
            }
        }

        if (!rest.empty() && rest.front() == '/') {
            result.path = rest;
            return result;
        }

        const auto slashPos = rest.find('/');
        std::string authority {};
        if (slashPos != std::string::npos) {
            authority = rest.substr(0, slashPos);
            result.path = rest.substr(slashPos);
        } else {
            authority = rest;
        }

        const auto atPos = authority.find('@');
        std::string hostPort = authority;
        if (atPos != std::string::npos) {
            const std::string userInfo = authority.substr(0, atPos);
            hostPort = authority.substr(atPos + 1);
            const auto colonPos = userInfo.find(':');
            if (colonPos != std::string::npos) {
                result.username = userInfo.substr(0, colonPos);
                result.password = userInfo.substr(colonPos + 1);
            } else {
                result.username = userInfo;
            }
        }

        if (!hostPort.empty() && hostPort.front() == '[') {
            const auto closeBracket = hostPort.find(']');
            if (closeBracket != std::string::npos) {
                result.host = hostPort.substr(1, closeBracket - 1);
                if (closeBracket + 1 < hostPort.length() && hostPort[closeBracket + 1] == ':') {
                    try {
                        result.port = std::stoi(hostPort.substr(closeBracket + 2));
                    } catch (...) {
                        result.port = -1;
                    }
                }
                return result;
            }
        }

        const auto colonPos = hostPort.rfind(':');
        if (colonPos != std::string::npos) {
            result.host = hostPort.substr(0, colonPos);
            try {
                result.port = std::stoi(hostPort.substr(colonPos + 1));
            } catch (...) {
                result.port = -1;
            }
        } else {
            result.host = hostPort;
        }

        return result;
    }

    std::shared_ptr<::Transport::ITransport> createTransportFromEndpoint(
        const std::string& hostOrPath, int port, const std::string& proto, int baudRate)
    {
        if (hostOrPath.rfind("/dev/", 0) == 0 || hostOrPath.rfind("COM", 0) == 0
            || hostOrPath.rfind("\\\\.\\COM", 0) == 0 || baudRate > 0) {
            const auto baud = (baudRate > 0) ? static_cast<std::uint32_t>(baudRate) : 9600U;
            if (!::Transport::SerialTransport::isValidBaudRate(baud)) {
                return nullptr;
            }
            return std::make_shared<::Transport::SerialTransport>(hostOrPath, baud);
        }

        if (hostOrPath.empty()) {
            return nullptr;
        }

        const auto socketPort
            = (port > 0 && port <= 65535) ? static_cast<std::uint16_t>(port) : static_cast<std::uint16_t>(4001U);
        if (proto == "udp") {
            return std::make_shared<::Transport::UdpTransport>(hostOrPath, socketPort);
        }
        return std::make_shared<::Transport::TcpTransport>(hostOrPath, socketPort);
    }

} // namespace

std::shared_ptr<IPayload> PayloadFactory::createFromUri(const std::string& uri)
{
    const ParsedUri parsed = parseUriString(uri);

    auto applyStreamParams = [&parsed](std::shared_ptr<IPayload> payload) -> std::shared_ptr<IPayload> {
        if (!payload) {
            return nullptr;
        }
        const std::string videoUri = parsed.getQuery("video", parsed.getQuery("stream", ""));
        const std::string substreamUri = parsed.getQuery("substream", "");
        const std::string thermalUri = parsed.getQuery("thermal", "");
        const std::string snapshotUri = parsed.getQuery("snapshot", "");

        if (auto prim = payload->primaryCamera()) {
            if (!videoUri.empty()) {
                prim->setVideoStreamUri(videoUri, VideoStreamProfile::Primary);
            }
            if (!substreamUri.empty()) {
                prim->setVideoStreamUri(substreamUri, VideoStreamProfile::Secondary);
            }
            if (!snapshotUri.empty()) {
                prim->setVideoStreamUri(snapshotUri, VideoStreamProfile::Snapshot);
            }
        }
        if (auto sec = payload->secondaryCamera()) {
            if (!thermalUri.empty()) {
                sec->setVideoStreamUri(thermalUri, VideoStreamProfile::Thermal);
                sec->setVideoStreamUri(thermalUri, VideoStreamProfile::Primary);
            }
        }

        // Bind optional physical or virtual LRF from query parameters
        const std::string lrfUri = parsed.getQuery("lrf", "");
        if (!lrfUri.empty()) {
            std::shared_ptr<ILaserRangeFinder> lrf;
            if (lrfUri.find("://") != std::string::npos || lrfUri == "sim") {
                lrf = createLrfFromUri(lrfUri);
            } else {
                std::string fullUri = "lrf://" + lrfUri;
                const std::string lrfProto = parsed.getQuery("lrf_proto", "nmea");
                const int lrfBaud = parsed.getIntQuery("lrf_baud", 9600);
                fullUri += "?proto=" + lrfProto + "&baud=" + std::to_string(lrfBaud);
                lrf = createLrfFromUri(fullUri);
            }
            if (lrf) {
                if (auto comp = std::dynamic_pointer_cast<PelcoDCompositePayload>(payload)) {
                    comp->setLrf(lrf);
                } else if (auto fuj = std::dynamic_pointer_cast<PelcoDFujinonPayloadAdapter>(payload)) {
                    fuj->setLrf(lrf);
                } else if (auto vis = std::dynamic_pointer_cast<PelcoDViscaCompositePayload>(payload)) {
                    vis->setLrf(lrf);
                } else if (auto onv = std::dynamic_pointer_cast<OnvifPayloadAdapter>(payload)) {
                    onv->setLrf(lrf);
                }
            }
        }

        return payload;
    };

    if (parsed.scheme == "sim") {
        return applyStreamParams(createSimulatedPayload());
    }

    if (parsed.scheme == "onvif") {
        std::string hostPort = parsed.host;
        if (parsed.port > 0) {
            hostPort += ":" + std::to_string(parsed.port);
        }
        const std::string path = parsed.path.empty() ? "/onvif/device_service" : parsed.path;
        const std::string endpoint = "http://" + hostPort + path;
        Onvif::SecurityCredentials creds {};
        creds.username = parsed.username;
        creds.password = parsed.password;
        return applyStreamParams(createOnvifPayload(endpoint, creds));
    }

    if (parsed.scheme == "pelcod") {
        const int addr = parsed.getIntQuery("addr", 1);
        if (addr < 1 || addr > 255) {
            return nullptr;
        }

        if (!parsed.path.empty()) {
            const int baud = parsed.getIntQuery("baud", 9600);
            auto transport = createTransportFromEndpoint(parsed.path, -1, "", baud);
            if (!transport) {
                return nullptr;
            }
            auto dev = std::make_shared<PelcoD::PelcoDDevice>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createPelcoDPayload(std::move(dev)));
        }

        if (!parsed.host.empty()) {
            const int port = parsed.port > 0 ? parsed.port : 4001;
            const std::string proto = parsed.getQuery("proto", "tcp");
            auto transport = createTransportFromEndpoint(parsed.host, port, proto, 0);
            if (!transport) {
                return nullptr;
            }
            auto dev = std::make_shared<PelcoD::PelcoDDevice>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createPelcoDPayload(std::move(dev)));
        }
        return nullptr;
    }

    if (parsed.scheme == "serial") {
        const std::string devPath = !parsed.path.empty() ? parsed.path : parsed.host;
        if (devPath.empty()) {
            return nullptr;
        }
        const int baud = parsed.getIntQuery("baud", 9600);
        const int addr = parsed.getIntQuery("addr", 1);
        if (addr < 1 || addr > 255) {
            return nullptr;
        }
        const std::string protocol = parsed.getQuery("protocol", "pelcod");
        auto transport = createTransportFromEndpoint(devPath, -1, "", baud);
        if (!transport) {
            return nullptr;
        }
        if (protocol == "pelcod") {
            auto dev = std::make_shared<PelcoD::PelcoDDevice>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createPelcoDPayload(std::move(dev)));
        }
        if (protocol == "fujinon") {
            auto dev
                = std::make_shared<PelcoD::FujinonSX800Device>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createFujinonPayload(std::move(dev)));
        }
        return nullptr;
    }

    if (parsed.scheme == "tcp" || parsed.scheme == "udp") {
        if (parsed.host.empty()) {
            return nullptr;
        }
        const int port = parsed.port > 0 ? parsed.port : 4001;
        const int addr = parsed.getIntQuery("addr", 1);
        if (addr < 1 || addr > 255) {
            return nullptr;
        }
        const std::string protocol = parsed.getQuery("protocol", "pelcod");
        auto transport = createTransportFromEndpoint(parsed.host, port, parsed.scheme, 0);
        if (!transport) {
            return nullptr;
        }
        if (protocol == "pelcod") {
            auto dev = std::make_shared<PelcoD::PelcoDDevice>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createPelcoDPayload(std::move(dev)));
        }
        if (protocol == "fujinon") {
            auto dev
                = std::make_shared<PelcoD::FujinonSX800Device>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createFujinonPayload(std::move(dev)));
        }
        return nullptr;
    }

    if (parsed.scheme == "fujinon") {
        const int addr = parsed.getIntQuery("addr", 1);
        if (addr < 1 || addr > 31) {
            return nullptr;
        }
        if (!parsed.path.empty()) {
            const int baud = parsed.getIntQuery("baud", 9600);
            auto transport = createTransportFromEndpoint(parsed.path, -1, "", baud);
            if (!transport) {
                return nullptr;
            }
            auto dev
                = std::make_shared<PelcoD::FujinonSX800Device>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createFujinonPayload(std::move(dev)));
        }
        if (!parsed.host.empty()) {
            const int port = parsed.port > 0 ? parsed.port : 4001;
            const std::string proto = parsed.getQuery("proto", "tcp");
            auto transport = createTransportFromEndpoint(parsed.host, port, proto, 0);
            if (!transport) {
                return nullptr;
            }
            auto dev
                = std::make_shared<PelcoD::FujinonSX800Device>(std::move(transport), static_cast<std::uint8_t>(addr));
            return applyStreamParams(createFujinonPayload(std::move(dev)));
        }
        return nullptr;
    }

    if (parsed.scheme == "pelcod-visca") {
        std::string ptzDev = parsed.getQuery("ptz_dev", "");
        if (ptzDev.empty() && !parsed.path.empty()) {
            ptzDev = parsed.path;
        }
        if (ptzDev.empty() && parsed.getQuery("ptz_port", "").rfind("/dev/", 0) == 0) {
            ptzDev = parsed.getQuery("ptz_port");
        }
        std::string camDev = parsed.getQuery("cam_dev", "");
        if (camDev.empty() && parsed.getQuery("cam_port", "").rfind("/dev/", 0) == 0) {
            camDev = parsed.getQuery("cam_port");
        }

        if (!ptzDev.empty() || !camDev.empty()) {
            if (ptzDev.empty() || camDev.empty()) {
                return nullptr;
            }
            const int defaultBaud = parsed.getIntQuery("baud", 9600);
            const int ptzBaud = parsed.getIntQuery("ptz_baud", defaultBaud);
            const int camBaud = parsed.getIntQuery("cam_baud", defaultBaud);
            const int ptzAddr = parsed.getIntQuery("ptz_addr", parsed.getIntQuery("addr", 1));
            const int camAddr = parsed.getIntQuery("cam_addr", 1);
            if (ptzAddr < 1 || ptzAddr > 255 || camAddr < 1 || camAddr > 7) {
                return nullptr;
            }
            auto ptzTrans = createTransportFromEndpoint(ptzDev, -1, "", ptzBaud);
            auto camTrans = createTransportFromEndpoint(camDev, -1, "", camBaud);
            if (!ptzTrans || !camTrans) {
                return nullptr;
            }
            auto ptzDevice
                = std::make_shared<PelcoD::PelcoDDevice>(std::move(ptzTrans), static_cast<std::uint8_t>(ptzAddr));
            auto camDevice
                = std::make_shared<Visca::Sony::SonyFCBDevice>(std::move(camTrans), static_cast<std::uint8_t>(camAddr));
            return applyStreamParams(createPelcoDViscaPayload(std::move(ptzDevice), std::move(camDevice)));
        }

        std::string ptzHost = parsed.getQuery("ptz_host", parsed.host);
        if (ptzHost.empty()) {
            return nullptr;
        }
        const int ptzPort = parsed.getIntQuery("ptz_port", parsed.port > 0 ? parsed.port : 4001);
        std::string camHost = parsed.getQuery("cam_host", ptzHost);
        const int camPort = parsed.getIntQuery("cam_port", 4002);
        const int ptzAddr = parsed.getIntQuery("ptz_addr", parsed.getIntQuery("addr", 1));
        const int camAddr = parsed.getIntQuery("cam_addr", 1);
        if (ptzAddr < 1 || ptzAddr > 255 || camAddr < 1 || camAddr > 7) {
            return nullptr;
        }
        const std::string proto = parsed.getQuery("proto", "tcp");
        auto ptzTrans = createTransportFromEndpoint(ptzHost, ptzPort, proto, 0);
        auto camTrans = createTransportFromEndpoint(camHost, camPort, proto, 0);
        if (!ptzTrans || !camTrans) {
            return nullptr;
        }
        auto ptzDevice
            = std::make_shared<PelcoD::PelcoDDevice>(std::move(ptzTrans), static_cast<std::uint8_t>(ptzAddr));
        auto camDevice
            = std::make_shared<Visca::Sony::SonyFCBDevice>(std::move(camTrans), static_cast<std::uint8_t>(camAddr));
        return applyStreamParams(createPelcoDViscaPayload(std::move(ptzDevice), std::move(camDevice)));
    }

    return nullptr;
}

std::shared_ptr<IPayload> PayloadFactory::createSimulatedPayload()
{
    return std::make_shared<SimulatedPayload>();
}

std::shared_ptr<IPayload> PayloadFactory::createPelcoDPayload(std::shared_ptr<PelcoD::PelcoDDevice> device)
{
    if (!device)
        return nullptr;
    return std::make_shared<PelcoDCompositePayload>(std::move(device));
}

std::shared_ptr<IPayload> PayloadFactory::createFujinonPayload(std::shared_ptr<PelcoD::FujinonSX800Device> device)
{
    if (!device)
        return nullptr;
    return std::make_shared<PelcoDFujinonPayloadAdapter>(std::move(device));
}

std::shared_ptr<IPayload> PayloadFactory::createPelcoDViscaPayload(
    std::shared_ptr<PelcoD::PelcoDDevice> ptzDevice, std::shared_ptr<Visca::Sony::SonyFCBDevice> cameraDevice)
{
    if (!ptzDevice && !cameraDevice) {
        return nullptr;
    }
    return std::make_shared<PelcoDViscaCompositePayload>(std::move(ptzDevice), std::move(cameraDevice));
}

std::shared_ptr<IPayload> PayloadFactory::createPelcoDViscaPayload(
    std::shared_ptr<IPanTiltUnit> ptu, std::shared_ptr<ICameraPayload> camera)
{
    if (!ptu && !camera) {
        return nullptr;
    }
    return std::make_shared<PelcoDViscaCompositePayload>(std::move(ptu), std::move(camera));
}

std::shared_ptr<IPayload> PayloadFactory::createOnvifPayload(
    std::shared_ptr<Onvif::OnvifClient> client, const std::string& profileToken)
{
    if (!client)
        return nullptr;
    return std::make_shared<OnvifPayloadAdapter>(std::move(client), profileToken);
}

std::shared_ptr<IPayload> PayloadFactory::createOnvifPayload(
    const std::string& deviceEndpoint, const Onvif::SecurityCredentials& credentials, const std::string& profileToken)
{
    if (deviceEndpoint.empty())
        return nullptr;
    auto client = std::make_shared<Onvif::OnvifClient>(deviceEndpoint, credentials);
    return std::make_shared<OnvifPayloadAdapter>(std::move(client), profileToken);
}

std::shared_ptr<ILaserRangeFinder> PayloadFactory::createLrfFromUri(const std::string& uri)
{
    if (uri.empty()) {
        return nullptr;
    }
    if (uri == "sim" || uri == "lrf://sim") {
        auto sim = createSimulatedPayload();
        return sim ? sim->lrf() : nullptr;
    }

    const ParsedUri parsed = parseUriString(uri);
    if (parsed.scheme == "sim" || (parsed.scheme == "lrf" && (parsed.host == "sim" || parsed.path == "sim"))) {
        auto sim = createSimulatedPayload();
        return sim ? sim->lrf() : nullptr;
    }

    std::string hostOrPath = parsed.host;
    if (parsed.host == "serial" && !parsed.path.empty()) {
        hostOrPath = (parsed.path[0] == '/' && parsed.path.size() > 1 && parsed.path[1] != '/')
            ? parsed.path.substr(1)
            : parsed.path;
    } else if (parsed.host == "tcp" || parsed.host == "udp") {
        if (!parsed.path.empty()) {
            hostOrPath = (parsed.path[0] == '/') ? parsed.path.substr(1) : parsed.path;
        }
    } else if (!parsed.path.empty() && hostOrPath.empty()) {
        hostOrPath = (parsed.path[0] == '/' && parsed.path.size() > 1 && parsed.path[1] != '/')
            ? parsed.path.substr(1)
            : parsed.path;
    }

    const int port = parsed.port;
    const std::string proto = parsed.getQuery("proto", parsed.getQuery("transport", ""));
    const int baudRate = parsed.getIntQuery("baud", 9600);

    auto transport = createTransportFromEndpoint(hostOrPath, port, proto, baudRate);
    if (!transport) {
        return nullptr;
    }

    SerialLrfConfig cfg {};
    const std::string lrfProto = parsed.getQuery("proto", parsed.getQuery("lrf_proto", "nmea"));
    if (lrfProto == "ascii") {
        cfg.protocolType = LrfProtocolType::Ascii;
    } else if (lrfProto == "binary" || lrfProto == "bin") {
        cfg.protocolType = LrfProtocolType::Binary;
    } else {
        cfg.protocolType = LrfProtocolType::Nmea;
    }

    const int autoDisarmSec = parsed.getIntQuery("autodisarm", 30);
    cfg.autoDisarmTimeout = std::chrono::seconds(autoDisarmSec);

    return std::make_shared<SerialLrfAdapter>(std::move(transport), cfg);
}

} // namespace PayloadHal
