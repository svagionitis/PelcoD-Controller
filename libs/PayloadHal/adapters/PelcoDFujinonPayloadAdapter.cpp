#include "PelcoDFujinonPayloadAdapter.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace PayloadHal {

// =============================================================================
// Internal Subsystem Units
// =============================================================================

class PelcoDFujinonPayloadAdapter::FujinonPtuUnit : public IPanTiltUnit {
public:
    explicit FujinonPtuUnit(std::shared_ptr<PelcoD::FujinonSX800Device> device)
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
    bool isConnected() const noexcept override
    {
        return m_device && m_device->isConnected();
    }
    DeviceState state() const noexcept override
    {
        return (m_device && m_device->isConnected()) ? DeviceState::Ready : DeviceState::Disconnected;
    }
    DeviceInfo info() const noexcept override
    {
        DeviceInfo d {};
        d.manufacturer = "Fujinon / Pelco";
        d.model = "SX800 Integrated PTZ Head";
        d.firmwareVersion = "2.4.0";
        return d;
    }
    void registerStateCallback(StateCallback cb) override
    {
        m_stateCb = std::move(cb);
    }

    bool setRate(double panDegPerSec, double tiltDegPerSec) override
    {
        const float normPan = static_cast<float>(std::clamp(panDegPerSec / 60.0, -1.0, 1.0));
        const float normTilt = static_cast<float>(std::clamp(tiltDegPerSec / 30.0, -1.0, 1.0));
        return setNormalizedVelocity(normPan, normTilt);
    }

    bool setNormalizedVelocity(float panVel, float tiltVel) override
    {
        if (!m_device)
            return false;
        const auto panSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(panVel) * 63.0f, 0.0f, 63.0f));
        const auto tiltSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(tiltVel) * 63.0f, 0.0f, 63.0f));

        PelcoD::PanDirection panDir = (panVel > 0.02f)
            ? PelcoD::PanDirection::Right
            : ((panVel < -0.02f) ? PelcoD::PanDirection::Left : PelcoD::PanDirection::Stop);
        PelcoD::TiltDirection tiltDir = (tiltVel > 0.02f)
            ? PelcoD::TiltDirection::Up
            : ((tiltVel < -0.02f) ? PelcoD::TiltDirection::Down : PelcoD::TiltDirection::Stop);
        m_device->move(panDir, panSpeed, tiltDir, tiltSpeed);
        return true;
    }

    bool setAbsoluteAngles(double panDeg, double tiltDeg) override
    {
        if (!m_device)
            return false;
        double normPan = std::fmod(panDeg, 360.0);
        if (normPan < 0.0)
            normPan += 360.0;
        double pelcoTilt = (tiltDeg < 0.0) ? -tiltDeg : (360.0 - tiltDeg);
        pelcoTilt = std::fmod(pelcoTilt, 360.0);
        if (pelcoTilt < 0.0)
            pelcoTilt += 360.0;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.panAngleDeg = normPan;
            m_telemetry.tiltAngleDeg = std::clamp(tiltDeg, -90.0, 90.0);
            m_telemetry.timestamp = std::chrono::system_clock::now();
        }

        m_device->setPanAngle(static_cast<std::uint16_t>(normPan * 100.0));
        m_device->setTiltAngle(static_cast<std::uint16_t>(pelcoTilt * 100.0));
        return true;
    }

    bool setRelativeNudge(double deltaPanDeg, double deltaTiltDeg) override
    {
        double currentPan { 0.0 };
        double currentTilt { 0.0 };
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            currentPan = m_telemetry.panAngleDeg;
            currentTilt = m_telemetry.tiltAngleDeg;
        }
        return setAbsoluteAngles(currentPan + deltaPanDeg, currentTilt + deltaTiltDeg);
    }

    bool stopMotion() override
    {
        if (!m_device)
            return false;
        m_device->stopMotion();
        return true;
    }

    bool supportsStabilization() const noexcept override
    {
        return false;
    }
    bool setStabilizationMode(StabilizationMode mode) override
    {
        return mode == StabilizationMode::Disabled;
    }
    StabilizationMode stabilizationMode() const noexcept override
    {
        return StabilizationMode::Disabled;
    }
    bool zeroGyroDrift() override
    {
        return false;
    }

    bool getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const override
    {
        minPan = 0.0;
        maxPan = 360.0;
        minTilt = -90.0;
        maxTilt = 90.0;
        return true;
    }

    bool savePreset(uint8_t presetId, const std::string& /*name*/) override
    {
        if (!m_device)
            return false;
        m_device->setPreset(presetId);
        return true;
    }

    bool recallPreset(uint8_t presetId) override
    {
        if (!m_device)
            return false;
        m_device->goToPreset(presetId);
        return true;
    }

    void registerTelemetryCallback(TelemetryCallback cb) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetryCb = std::move(cb);
    }

    void updateTelemetry(const PelcoD::DeviceStatus& status)
    {
        GimbalTelemetry telem {};
        telem.panAngleDeg = status.panDegrees();
        const double rawTilt = status.tiltDegrees();
        telem.tiltAngleDeg = (rawTilt <= 180.0) ? -rawTilt : (360.0 - rawTilt);
        telem.timestamp = std::chrono::system_clock::now();

        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry = telem;
            cbCopy = m_telemetryCb;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }

    [[nodiscard]] GimbalTelemetry currentTelemetry() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    std::shared_ptr<PelcoD::FujinonSX800Device> m_device;
    mutable std::mutex m_mutex;
    GimbalTelemetry m_telemetry {};
    TelemetryCallback m_telemetryCb {};
    StateCallback m_stateCb {};
};

class PelcoDFujinonPayloadAdapter::FujinonCameraUnit : public ICameraPayload {
public:
    explicit FujinonCameraUnit(std::shared_ptr<PelcoD::FujinonSX800Device> device)
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
    bool isConnected() const noexcept override
    {
        return m_device && m_device->isConnected();
    }
    DeviceState state() const noexcept override
    {
        return (m_device && m_device->isConnected()) ? DeviceState::Ready : DeviceState::Disconnected;
    }
    DeviceInfo info() const noexcept override
    {
        DeviceInfo d {};
        d.manufacturer = "Fujifilm / Fujinon";
        d.model = "SX800 Long-Range Surveillance Camera";
        d.firmwareVersion = "2.4.0";
        return d;
    }
    void registerStateCallback(StateCallback cb) override
    {
        m_stateCb = std::move(cb);
    }

    CameraSpectrum spectrum() const noexcept override
    {
        return CameraSpectrum::DaylightVisible;
    }

    bool setZoomNormalized(double zoom01) override
    {
        if (!m_device)
            return false;
        const double clampedZoom = std::clamp(zoom01, 0.0, 1.0);
        // Fujinon SX800 has 40x optical zoom. Centivalues range from 100 (1.0x) to 4000 (40.0x)
        const auto centiMag = static_cast<std::uint16_t>(100.0 + clampedZoom * 3900.0);
        m_device->setMagnification(centiMag);
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

    bool setIrisNormalized(double iris01) override
    {
        if (!m_device)
            return false;
        const double clamped = std::clamp(iris01, 0.0, 1.0);
        // Fujinon SX800 manual iris 0 (closed) to 15 (open)
        const auto val = static_cast<std::uint8_t>(clamped * 15.0);
        m_device->setManualIris(val);
        return true;
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

    bool setDayNightIcr(bool nightMode) override
    {
        if (!m_device)
            return false;
        m_device->setVLCFilter(nightMode);
        return true;
    }

    bool setDefog(bool enable) override
    {
        if (!m_device)
            return false;
        m_device->setDefog(enable ? PelcoD::FujinonDefogLevel::Level2 : PelcoD::FujinonDefogLevel::Off);
        return true;
    }

    bool setStabilizer(bool enable) override
    {
        if (!m_device)
            return false;
        m_device->setOISMode(enable ? PelcoD::FujinonOISMode::OisOn : PelcoD::FujinonOISMode::Off);
        return true;
    }

    void registerTelemetryCallback(TelemetryCallback cb) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetryCb = std::move(cb);
    }

    void updateTelemetry(const PelcoD::DeviceStatus& status, const PelcoD::FujinonStatus& fujinonStatus)
    {
        CameraTelemetry telem {};
        // SX800 magnification centivalue
        const double mag = (status.magnification > 0U) ? (static_cast<double>(status.magnification) / 100.0) : 1.0;
        telem.opticalZoomFactor = std::clamp(mag, 1.0, 40.0);
        telem.normalizedZoom = (telem.opticalZoomFactor - 1.0) / 39.0;
        telem.autoFocusActive = (status.autoFocus == PelcoD::AutoMode::Auto);
        telem.irisNormalized = std::clamp(static_cast<double>(fujinonStatus.manualIris) / 15.0, 0.0, 1.0);
        telem.autoIrisActive = (status.autoIris == PelcoD::AutoMode::Auto);
        telem.dayNightIcrActive = fujinonStatus.vlcFilter;

        // Calculate HFOV from magnification: SX800 has ~60° wide HFOV
        constexpr double kWideHfovRad { 60.0 * 3.14159265358979323846 / 180.0 };
        const double currentHfovRad = 2.0 * std::atan(std::tan(kWideHfovRad / 2.0) / telem.opticalZoomFactor);
        telem.horizontalFovDeg = currentHfovRad * (180.0 / 3.14159265358979323846);
        const double currentVfovRad = 2.0 * std::atan(std::tan(currentHfovRad / 2.0) * (9.0 / 16.0));
        telem.verticalFovDeg = currentVfovRad * (180.0 / 3.14159265358979323846);
        telem.timestamp = std::chrono::system_clock::now();

        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry = telem;
            cbCopy = m_telemetryCb;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }

    [[nodiscard]] CameraTelemetry currentTelemetry() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
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
                desc.framerateFps = 60.0;
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
    std::shared_ptr<PelcoD::FujinonSX800Device> m_device;
    mutable std::mutex m_mutex;
    CameraTelemetry m_telemetry {};
    TelemetryCallback m_telemetryCb {};
    StateCallback m_stateCb {};
    std::map<VideoStreamProfile, std::string> m_streamUris {};
};

// =============================================================================
// PelcoDFujinonPayloadAdapter Implementation
// =============================================================================

PelcoDFujinonPayloadAdapter::PelcoDFujinonPayloadAdapter(std::shared_ptr<PelcoD::FujinonSX800Device> device)
    : m_device(std::move(device))
    , m_ptu(std::make_shared<FujinonPtuUnit>(m_device))
    , m_camera(std::make_shared<FujinonCameraUnit>(m_device))
{
    if (m_device) {
        m_device->addStatusCallback([this](const PelcoD::DeviceStatus& status) {
            if (m_ptu) {
                m_ptu->updateTelemetry(status);
            }
        });
        m_device->addFujinonStatusCallback([this](const PelcoD::FujinonStatus& fStatus) {
            if (m_camera && m_device) {
                m_camera->updateTelemetry(m_device->getStatus(), fStatus);
            }
        });
    }
}

PelcoDFujinonPayloadAdapter::~PelcoDFujinonPayloadAdapter()
{
    disconnect();
}

bool PelcoDFujinonPayloadAdapter::connect()
{
    if (!m_device)
        return false;
    const bool ok = m_device->start();
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stateCallback) {
            m_stateCallback(DeviceState::Ready, "Fujinon SX800 Connected");
        }
    }
    return ok;
}

void PelcoDFujinonPayloadAdapter::disconnect()
{
    if (m_device) {
        m_device->stop();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Disconnected, "Fujinon SX800 Disconnected");
    }
}

bool PelcoDFujinonPayloadAdapter::isConnected() const noexcept
{
    return m_device && m_device->isConnected();
}

DeviceState PelcoDFujinonPayloadAdapter::state() const noexcept
{
    if (!m_device)
        return DeviceState::Fault;
    return m_device->isConnected() ? DeviceState::Ready : DeviceState::Disconnected;
}

DeviceInfo PelcoDFujinonPayloadAdapter::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "Fujifilm / Fujinon";
    d.model = "SX800 Composite Station";
    d.firmwareVersion = "2.4.0";
    return d;
}

void PelcoDFujinonPayloadAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

std::shared_ptr<IPanTiltUnit> PelcoDFujinonPayloadAdapter::panTilt() const noexcept
{
    return m_ptu;
}

std::shared_ptr<ICameraPayload> PelcoDFujinonPayloadAdapter::primaryCamera() const noexcept
{
    return m_camera;
}

std::shared_ptr<ICameraPayload> PelcoDFujinonPayloadAdapter::secondaryCamera() const noexcept
{
    return nullptr;
}

void PelcoDFujinonPayloadAdapter::setLrf(std::shared_ptr<ILaserRangeFinder> lrf) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lrf = std::move(lrf);
}

std::shared_ptr<ILaserRangeFinder> PelcoDFujinonPayloadAdapter::lrf() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lrf;
}

std::optional<Klv::GeoPoint2D> PelcoDFujinonPayloadAdapter::calculateTargetCoordinates(
    const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const
{
    if (!m_ptu) {
        return std::nullopt;
    }
    const GimbalTelemetry telem = m_ptu->currentTelemetry();
    const Klv::GeoPoint3D platform3D { platformGps.latitudeDeg, platformGps.longitudeDeg, platformAltMeters };

    // 1. Prefer precise slant range calculation if LRF return exists
    std::shared_ptr<ILaserRangeFinder> currentLrf;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentLrf = m_lrf;
    }
    if (currentLrf) {
        if (const auto meas = currentLrf->lastMeasurement(); meas && meas->valid && meas->slantRangeMeters > 0.0) {
            const auto targetSlant = GeoreferenceUtils::computeTargetFromSlantRange(
                platform3D, platformHeadingDeg, telem.panAngleDeg, telem.tiltAngleDeg, meas->slantRangeMeters);
            if (targetSlant.has_value()) {
                return Klv::GeoPoint2D { targetSlant->latitudeDeg, targetSlant->longitudeDeg };
            }
        }
    }

    // 2. Fallback to ground plane intersection
    const auto target3D = GeoreferenceUtils::computeTargetFromGroundIntersection(
        platform3D, platformHeadingDeg, telem.panAngleDeg, telem.tiltAngleDeg, 0.0);

    if (!target3D.has_value()) {
        return std::nullopt;
    }
    return Klv::GeoPoint2D { target3D->latitudeDeg, target3D->longitudeDeg };
}

bool PelcoDFujinonPayloadAdapter::setOISMode(PelcoD::FujinonOISMode mode)
{
    if (!m_device)
        return false;
    m_device->setOISMode(mode);
    return true;
}

bool PelcoDFujinonPayloadAdapter::setDefogLevel(PelcoD::FujinonDefogLevel level)
{
    if (!m_device)
        return false;
    m_device->setDefog(level);
    return true;
}

bool PelcoDFujinonPayloadAdapter::setHeatHaze(PelcoD::FujinonHeatHazeLevel level)
{
    if (!m_device)
        return false;
    m_device->setHeatHaze(level);
    return true;
}

bool PelcoDFujinonPayloadAdapter::setWDR(PelcoD::FujinonWDRLevel level)
{
    if (!m_device)
        return false;
    m_device->setWDR(level);
    return true;
}

bool PelcoDFujinonPayloadAdapter::setVLCFilter(bool enable)
{
    if (!m_device)
        return false;
    m_device->setVLCFilter(enable);
    return true;
}

} // namespace PayloadHal
