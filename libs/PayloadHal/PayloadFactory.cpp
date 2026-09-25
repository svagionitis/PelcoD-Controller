#include "PayloadFactory.h"
#include "GeoreferenceUtils.h"
#include "adapters/OnvifPayloadAdapter.h"
#include "adapters/PelcoDFujinonPayloadAdapter.h"
#include "adapters/PelcoDPtzAdapter.h"
#include "sim/SimulatedPayload.h"

#include <algorithm>
#include <cmath>

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

        bool triggerOnePushFocus() override
        {
            if (!m_device)
                return false;
            m_device->focusNear();
            m_device->focusStop();
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
            }
            telem.timestamp = std::chrono::system_clock::now();
            return telem;
        }

    private:
        std::shared_ptr<PelcoD::PelcoDDevice> m_device;
        TelemetryCallback m_telemCb {};
        StateCallback m_stateCb {};
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
        [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
            const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override
        {
            if (!m_ptu) {
                return std::nullopt;
            }
            const GimbalTelemetry telem = m_ptu->currentTelemetry();
            const Klv::GeoPoint3D platform3D { platformGps.latitudeDeg, platformGps.longitudeDeg, platformAltMeters };
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
    };

} // namespace

std::shared_ptr<IPayload> PayloadFactory::createFromUri(const std::string& uri)
{
    if (uri.rfind("sim://", 0) == 0 || uri == "sim") {
        return createSimulatedPayload();
    }
    if (uri.rfind("onvif://", 0) == 0) {
        std::string remainder = uri.substr(8);
        Onvif::SecurityCredentials creds {};
        const auto atPos = remainder.find('@');
        if (atPos != std::string::npos) {
            const std::string auth = remainder.substr(0, atPos);
            remainder = remainder.substr(atPos + 1);
            const auto colonPos = auth.find(':');
            if (colonPos != std::string::npos) {
                creds.username = auth.substr(0, colonPos);
                creds.password = auth.substr(colonPos + 1);
            } else {
                creds.username = auth;
            }
        }
        std::string hostPort {};
        std::string path { "/onvif/device_service" };
        const auto slashPos = remainder.find('/');
        if (slashPos != std::string::npos) {
            hostPort = remainder.substr(0, slashPos);
            const std::string customPath = remainder.substr(slashPos);
            if (customPath.length() > 1) {
                path = customPath;
            }
        } else {
            hostPort = remainder;
        }

        const std::string endpoint = "http://" + hostPort + path;
        return createOnvifPayload(endpoint, creds);
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

} // namespace PayloadHal
