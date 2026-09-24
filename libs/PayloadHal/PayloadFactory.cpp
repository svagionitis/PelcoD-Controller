#include "PayloadFactory.h"
#include "adapters/PelcoDPtzAdapter.h"
#include "adapters/PelcoDFujinonPayloadAdapter.h"
#include "sim/SimulatedPayload.h"
#include "GeoreferenceUtils.h"

namespace PayloadHal {

namespace {

class PelcoDCameraUnit : public ICameraPayload {
public:
    explicit PelcoDCameraUnit(std::shared_ptr<PelcoD::PelcoDDevice> device)
        : m_device(std::move(device)) {}

    bool connect() override { return m_device && m_device->start(); }
    void disconnect() override { if (m_device) m_device->stop(); }
    [[nodiscard]] bool isConnected() const noexcept override { return m_device && m_device->isConnected(); }
    [[nodiscard]] DeviceState state() const noexcept override {
        return (m_device && m_device->isConnected()) ? DeviceState::Ready : DeviceState::Disconnected;
    }
    [[nodiscard]] DeviceInfo info() const noexcept override {
        DeviceInfo d {};
        d.manufacturer = "Pelco";
        d.model = "Pelco-D Camera Optics";
        return d;
    }
    void registerStateCallback(StateCallback cb) override { m_stateCb = std::move(cb); }

    [[nodiscard]] CameraSpectrum spectrum() const noexcept override {
        return CameraSpectrum::DaylightVisible;
    }

    bool setZoomNormalized(double zoom01) override {
        if (!m_device) return false;
        const auto pos = static_cast<std::uint16_t>(std::clamp(zoom01, 0.0, 1.0) * 0xFFFF);
        m_device->setZoomPosition(pos);
        return true;
    }

    bool zoomContinuous(float velocity) override {
        if (!m_device) return false;
        if (velocity > 0.05f) {
            m_device->zoomTele();
        } else if (velocity < -0.05f) {
            m_device->zoomWide();
        } else {
            m_device->zoomStop();
        }
        return true;
    }

    bool zoomStop() override {
        if (!m_device) return false;
        m_device->zoomStop();
        return true;
    }

    bool setFocusAuto(bool enable) override {
        if (!m_device) return false;
        m_device->setAutoFocus(enable ? PelcoD::AutoMode::Auto : PelcoD::AutoMode::Off);
        return true;
    }

    bool setFocusNormalized(double /*focus01*/) override { return false; }

    bool triggerOnePushFocus() override {
        if (!m_device) return false;
        m_device->focusNear();
        m_device->focusStop();
        return true;
    }

    bool setDayNightIcr(bool /*nightMode*/) override { return false; }
    bool setDefog(bool /*enable*/) override { return false; }
    bool setStabilizer(bool /*enable*/) override { return false; }

    void registerTelemetryCallback(TelemetryCallback cb) override {
        m_telemCb = std::move(cb);
    }

private:
    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    TelemetryCallback m_telemCb {};
    StateCallback m_stateCb {};
};

class PelcoDCompositePayload : public IPayload {
public:
    explicit PelcoDCompositePayload(std::shared_ptr<PelcoD::PelcoDDevice> device)
        : m_device(device),
          m_ptu(std::make_shared<PelcoDPtzAdapter>(device)),
          m_camera(std::make_shared<PelcoDCameraUnit>(device)) {}

    bool connect() override { return m_device && m_device->start(); }
    void disconnect() override { if (m_device) m_device->stop(); }
    [[nodiscard]] bool isConnected() const noexcept override { return m_device && m_device->isConnected(); }
    [[nodiscard]] DeviceState state() const noexcept override {
        return (m_device && m_device->isConnected()) ? DeviceState::Ready : DeviceState::Disconnected;
    }
    [[nodiscard]] DeviceInfo info() const noexcept override {
        DeviceInfo d {};
        d.manufacturer = "Pelco";
        d.model = "Pelco-D Composite Station";
        return d;
    }
    void registerStateCallback(StateCallback cb) override {
        if (m_ptu) m_ptu->registerStateCallback(cb);
    }

    [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept override { return m_ptu; }
    [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept override { return m_camera; }
    [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept override { return nullptr; }
    [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override { return nullptr; }

    [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
        const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override {
        double minP { 0.0 };
        double maxP { 0.0 };
        double minT { 0.0 };
        double maxT { 0.0 };
        m_ptu->getLimits(minP, maxP, minT, maxT);
        // Estimate ground intersection with level/tilt
        const Klv::GeoPoint3D platform3D { platformGps.latitudeDeg, platformGps.longitudeDeg, platformAltMeters };
        auto target3D = GeoreferenceUtils::computeTargetFromGroundIntersection(
            platform3D, platformHeadingDeg, 0.0, -10.0, 0.0);
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

std::shared_ptr<IPayload> PayloadFactory::createFromUri(const std::string& uri) {
    if (uri.rfind("sim://", 0) == 0 || uri == "sim") {
        return createSimulatedPayload();
    }
    return nullptr;
}

std::shared_ptr<IPayload> PayloadFactory::createSimulatedPayload() {
    return std::make_shared<SimulatedPayload>();
}

std::shared_ptr<IPayload> PayloadFactory::createPelcoDPayload(
    std::shared_ptr<PelcoD::PelcoDDevice> device) {
    if (!device) return nullptr;
    return std::make_shared<PelcoDCompositePayload>(std::move(device));
}

std::shared_ptr<IPayload> PayloadFactory::createFujinonPayload(
    std::shared_ptr<PelcoD::FujinonSX800Device> device) {
    if (!device) return nullptr;
    return std::make_shared<PelcoDFujinonPayloadAdapter>(std::move(device));
}

} // namespace PayloadHal
