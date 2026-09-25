#include "SimulatedPayload.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace PayloadHal {

// =============================================================================
// Simulated Pan-Tilt Unit
// =============================================================================

class SimulatedPayload::SimPtu : public IPanTiltUnit {
public:
    SimPtu() = default;

    bool connect() override
    {
        m_connected = true;
        return true;
    }
    void disconnect() override
    {
        m_connected = false;
    }
    bool isConnected() const noexcept override
    {
        return m_connected;
    }
    DeviceState state() const noexcept override
    {
        return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
    }
    DeviceInfo info() const noexcept override
    {
        DeviceInfo d {};
        d.manufacturer = "Simulated";
        d.model = "Virtual Gyro-Stabilized Gimbal";
        d.firmwareVersion = "1.0.0-sim";
        return d;
    }
    void registerStateCallback(StateCallback cb) override
    {
        m_stateCb = std::move(cb);
    }

    bool setRate(double panDegPerSec, double tiltDegPerSec) override
    {
        return setNormalizedVelocity(static_cast<float>(panDegPerSec / 60.0), static_cast<float>(tiltDegPerSec / 30.0));
    }

    bool setNormalizedVelocity(float panVel, float tiltVel) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.panRateDegPerSec = static_cast<double>(std::clamp(panVel, -1.0f, 1.0f)) * 60.0;
        m_telemetry.tiltRateDegPerSec = static_cast<double>(std::clamp(tiltVel, -1.0f, 1.0f)) * 30.0;
        m_telemetry.isMoving = (std::abs(panVel) > 0.01f || std::abs(tiltVel) > 0.01f);
        dispatchTelemetry();
        return true;
    }

    bool setAbsoluteAngles(double panDeg, double tiltDeg) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        double normPan = std::fmod(panDeg, 360.0);
        if (normPan < 0.0)
            normPan += 360.0;
        m_telemetry.panAngleDeg = normPan;
        m_telemetry.tiltAngleDeg = std::clamp(tiltDeg, -90.0, 90.0);
        m_telemetry.isMoving = false;
        dispatchTelemetry();
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
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.panRateDegPerSec = 0.0;
        m_telemetry.tiltRateDegPerSec = 0.0;
        m_telemetry.isMoving = false;
        dispatchTelemetry();
        return true;
    }

    bool supportsStabilization() const noexcept override
    {
        return true;
    }
    bool setStabilizationMode(StabilizationMode mode) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stabMode = mode;
        m_telemetry.isStabilized = (mode != StabilizationMode::Disabled);
        dispatchTelemetry();
        return true;
    }
    StabilizationMode stabilizationMode() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stabMode;
    }
    bool zeroGyroDrift() override
    {
        return true;
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
        std::lock_guard<std::mutex> lock(m_mutex);
        m_presets[presetId] = { m_telemetry.panAngleDeg, m_telemetry.tiltAngleDeg };
        return true;
    }

    bool recallPreset(uint8_t presetId) override
    {
        std::pair<double, double> targetAngles {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_presets.find(presetId);
            if (it == m_presets.end())
                return false;
            targetAngles = it->second;
        }
        return setAbsoluteAngles(targetAngles.first, targetAngles.second);
    }

    void registerTelemetryCallback(TelemetryCallback cb) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetryCb = std::move(cb);
    }

    [[nodiscard]] GimbalTelemetry currentTelemetry() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    void dispatchTelemetry()
    {
        m_telemetry.timestamp = std::chrono::system_clock::now();
        if (m_telemetryCb) {
            m_telemetryCb(m_telemetry);
        }
    }

    mutable std::mutex m_mutex;
    bool m_connected { true };
    StabilizationMode m_stabMode { StabilizationMode::RateStabilized };
    GimbalTelemetry m_telemetry {};
    TelemetryCallback m_telemetryCb {};
    StateCallback m_stateCb {};
    std::unordered_map<uint8_t, std::pair<double, double>> m_presets {};
};

// =============================================================================
// Simulated Camera
// =============================================================================

class SimulatedPayload::SimCamera : public ICameraPayload {
public:
    explicit SimCamera(CameraSpectrum spec)
        : m_spectrum(spec)
    {
        updateFov();
    }

    bool connect() override
    {
        m_connected = true;
        return true;
    }
    void disconnect() override
    {
        m_connected = false;
    }
    bool isConnected() const noexcept override
    {
        return m_connected;
    }
    DeviceState state() const noexcept override
    {
        return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
    }
    DeviceInfo info() const noexcept override
    {
        DeviceInfo d {};
        d.manufacturer = "Simulated";
        d.model = (m_spectrum == CameraSpectrum::DaylightVisible) ? "Virtual EO Camera" : "Virtual Thermal LWIR";
        d.firmwareVersion = "1.0.0-sim";
        return d;
    }
    void registerStateCallback(StateCallback cb) override
    {
        m_stateCb = std::move(cb);
    }

    CameraSpectrum spectrum() const noexcept override
    {
        return m_spectrum;
    }

    bool setZoomNormalized(double zoom01) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.normalizedZoom = std::clamp(zoom01, 0.0, 1.0);
        m_telemetry.opticalZoomFactor = 1.0 + m_telemetry.normalizedZoom * 29.0;
        updateFov();
        dispatchTelemetry();
        return true;
    }

    bool zoomContinuous(float velocity) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const double delta = static_cast<double>(velocity) * 0.05;
        m_telemetry.normalizedZoom = std::clamp(m_telemetry.normalizedZoom + delta, 0.0, 1.0);
        m_telemetry.opticalZoomFactor = 1.0 + m_telemetry.normalizedZoom * 29.0;
        updateFov();
        dispatchTelemetry();
        return true;
    }

    bool zoomStop() override
    {
        return true;
    }

    bool setFocusAuto(bool enable) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.autoFocusActive = enable;
        dispatchTelemetry();
        return true;
    }

    bool setFocusNormalized(double focus01) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.focusDistanceNormalized = std::clamp(focus01, 0.0, 1.0);
        dispatchTelemetry();
        return true;
    }

    bool focusContinuous(float velocity) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const double delta = static_cast<double>(velocity) * 0.05;
        m_telemetry.focusDistanceNormalized = std::clamp(m_telemetry.focusDistanceNormalized + delta, 0.0, 1.0);
        m_telemetry.autoFocusActive = false;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool focusStop() override
    {
        return true;
    }

    bool triggerOnePushFocus() override
    {
        return true;
    }

    bool setIrisAuto(bool enable) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.autoIrisActive = enable;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool setIrisNormalized(double iris01) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.irisNormalized = std::clamp(iris01, 0.0, 1.0);
        m_telemetry.autoIrisActive = false;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool irisContinuous(float velocity) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const double delta = static_cast<double>(velocity) * 0.05;
        m_telemetry.irisNormalized = std::clamp(m_telemetry.irisNormalized + delta, 0.0, 1.0);
        m_telemetry.autoIrisActive = false;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool irisStop() override
    {
        return true;
    }

    bool setDayNightIcr(bool nightMode) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.dayNightIcrActive = nightMode;
        dispatchTelemetry();
        return true;
    }

    bool setDefog(bool enable) override
    {
        m_defog = enable;
        return true;
    }

    bool setStabilizer(bool enable) override
    {
        m_ois = enable;
        return true;
    }

    bool setThermalPolarity(ThermalPolarity polarity) override
    {
        m_polarity = polarity;
        return true;
    }

    bool triggerNucCalibration() override
    {
        return true;
    }

    void registerTelemetryCallback(TelemetryCallback cb) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetryCb = std::move(cb);
    }

    [[nodiscard]] CameraTelemetry currentTelemetry() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    void updateFov()
    {
        constexpr double kWideHfovRad { 60.0 * 3.14159265358979323846 / 180.0 };
        const double currentHfovRad = 2.0 * std::atan(std::tan(kWideHfovRad / 2.0) / m_telemetry.opticalZoomFactor);
        m_telemetry.horizontalFovDeg = currentHfovRad * (180.0 / 3.14159265358979323846);
        const double currentVfovRad = 2.0 * std::atan(std::tan(currentHfovRad / 2.0) * (9.0 / 16.0));
        m_telemetry.verticalFovDeg = currentVfovRad * (180.0 / 3.14159265358979323846);
    }

    void dispatchTelemetry()
    {
        m_telemetry.timestamp = std::chrono::system_clock::now();
        if (m_telemetryCb) {
            m_telemetryCb(m_telemetry);
        }
    }

    mutable std::mutex m_mutex;
    CameraSpectrum m_spectrum;
    bool m_connected { true };
    bool m_defog { false };
    bool m_ois { false };
    ThermalPolarity m_polarity { ThermalPolarity::WhiteHot };
    CameraTelemetry m_telemetry {};
    TelemetryCallback m_telemetryCb {};
    StateCallback m_stateCb {};
};

// =============================================================================
// Simulated Laser Range Finder (LRF)
// =============================================================================

class SimulatedPayload::SimLrf : public ILaserRangeFinder {
public:
    SimLrf() = default;

    bool connect() override
    {
        m_connected = true;
        return true;
    }
    void disconnect() override
    {
        m_connected = false;
        disarmLaser();
    }
    bool isConnected() const noexcept override
    {
        return m_connected;
    }
    DeviceState state() const noexcept override
    {
        return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
    }
    DeviceInfo info() const noexcept override
    {
        DeviceInfo d {};
        d.manufacturer = "Simulated";
        d.model = "Virtual Eye-Safe 1550nm Tactical LRF";
        d.firmwareVersion = "1.0.0-sim";
        return d;
    }
    void registerStateCallback(StateCallback cb) override
    {
        m_stateCb = std::move(cb);
    }

    bool armLaser() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_armed = true;
        return true;
    }

    bool disarmLaser() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_armed = false;
        m_mode = LrfMode::Standby;
        return true;
    }

    bool isArmed() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_armed;
    }

    bool triggerSingleMeasurement() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_armed) {
            // Safety interlock: cannot fire laser if disarmed!
            return false;
        }
        m_pulseCount++;
        emitMeasurement(m_simRangeMeters);
        return true;
    }

    bool setContinuousMode(LrfMode mode) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_armed && mode != LrfMode::Standby) {
            return false;
        }
        m_mode = mode;
        if (mode != LrfMode::Standby) {
            m_pulseCount++;
            emitMeasurement(m_simRangeMeters);
        }
        return true;
    }

    bool stopRanging() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_mode = LrfMode::Standby;
        return true;
    }

    bool setRangeGating(double minRangeMeters, double maxRangeMeters) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gateMin = minRangeMeters;
        m_gateMax = maxRangeMeters;
        return true;
    }

    void registerMeasurementCallback(MeasurementCallback cb) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_measurementCb = std::move(cb);
    }

    void setSimRange(double rangeMeters)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_simRangeMeters = rangeMeters;
    }

    [[nodiscard]] std::optional<LrfTargetMeasurement> lastMeasurement() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hasMeasurement) {
            return std::nullopt;
        }
        return m_lastMeasurement;
    }

private:
    void emitMeasurement(double range)
    {
        LrfTargetMeasurement m {};
        m.valid = (range >= m_gateMin && range <= m_gateMax);
        m.slantRangeMeters = m.valid ? range : 0.0;
        m.signalQualityRatio = m.valid ? 0.98 : 0.1;
        m.diodeTemperatureC = 25.0 + (m_pulseCount % 100) * 0.1;
        m.pulseCounter = m_pulseCount;
        m.timestamp = std::chrono::system_clock::now();
        m_lastMeasurement = m;
        m_hasMeasurement = true;

        if (m_measurementCb) {
            m_measurementCb(m);
        }
    }

    mutable std::mutex m_mutex;
    bool m_connected { true };
    bool m_armed { false };
    LrfMode m_mode { LrfMode::Standby };
    double m_simRangeMeters { 1250.0 };
    double m_gateMin { 50.0 };
    double m_gateMax { 10000.0 };
    uint32_t m_pulseCount { 0U };
    bool m_hasMeasurement { false };
    LrfTargetMeasurement m_lastMeasurement {};
    MeasurementCallback m_measurementCb {};
    StateCallback m_stateCb {};
};

// =============================================================================
// Simulated Laser Pointer / Illuminator
// =============================================================================

class SimulatedPayload::SimIlluminator : public ILaserIlluminator {
public:
    SimIlluminator()
    {
        m_telemetry.mode = IlluminatorMode::Standby;
        m_telemetry.powerNormalized = 0.5;
        m_telemetry.pulseFrequencyHz = 5.0;
        m_telemetry.beamDivergenceNormalized = 0.2;
        m_telemetry.diodeTemperatureC = 25.0;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }

    bool connect() override
    {
        m_connected = true;
        return true;
    }

    void disconnect() override
    {
        m_connected = false;
        disarmLaser();
    }

    bool isConnected() const noexcept override
    {
        return m_connected;
    }

    DeviceState state() const noexcept override
    {
        return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
    }

    DeviceInfo info() const noexcept override
    {
        DeviceInfo d {};
        d.manufacturer = "Simulated";
        d.model = "Virtual Tactical 850nm NIR Illuminator / Pointer";
        d.firmwareVersion = "1.0.0-sim";
        return d;
    }

    void registerStateCallback(StateCallback cb) override
    {
        m_stateCb = std::move(cb);
    }

    bool armLaser() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_armed = true;
        m_telemetry.isArmed = true;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool disarmLaser() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_armed = false;
        m_emitting = false;
        m_telemetry.isArmed = false;
        m_telemetry.isEmitting = false;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool isArmed() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_armed;
    }

    bool startEmission() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_armed) {
            // Safety interlock: cannot fire laser if disarmed!
            return false;
        }
        m_emitting = true;
        m_telemetry.isEmitting = true;
        m_telemetry.diodeTemperatureC = 25.0 + m_telemetry.powerNormalized * 15.0;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool stopEmission() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_emitting = false;
        m_telemetry.isEmitting = false;
        m_telemetry.diodeTemperatureC = 25.0;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool isEmitting() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_emitting;
    }

    bool setMode(IlluminatorMode mode) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_mode = mode;
        m_telemetry.mode = mode;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    IlluminatorMode mode() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_mode;
    }

    bool setPowerNormalized(double power01) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const double clamped = std::clamp(power01, 0.0, 1.0);
        m_telemetry.powerNormalized = clamped;
        if (m_emitting) {
            m_telemetry.diodeTemperatureC = 25.0 + clamped * 15.0;
        }
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool setPulseFrequency(double frequencyHz) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.pulseFrequencyHz = std::max(0.0, frequencyHz);
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    bool setBeamDivergenceNormalized(double divergence01) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.beamDivergenceNormalized = std::clamp(divergence01, 0.0, 1.0);
        m_telemetry.timestamp = std::chrono::system_clock::now();
        dispatchTelemetry();
        return true;
    }

    void registerTelemetryCallback(TelemetryCallback cb) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetryCb = std::move(cb);
    }

    IlluminatorTelemetry currentTelemetry() const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    void dispatchTelemetry()
    {
        if (m_telemetryCb) {
            m_telemetryCb(m_telemetry);
        }
    }

    mutable std::mutex m_mutex;
    bool m_connected { true };
    bool m_armed { false };
    bool m_emitting { false };
    IlluminatorMode m_mode { IlluminatorMode::Standby };
    IlluminatorTelemetry m_telemetry {};
    TelemetryCallback m_telemetryCb {};
    StateCallback m_stateCb {};
};

// =============================================================================
// SimulatedPayload Implementation
// =============================================================================

SimulatedPayload::SimulatedPayload()
    : m_ptu(std::make_shared<SimPtu>())
    , m_daylightCamera(std::make_shared<SimCamera>(CameraSpectrum::DaylightVisible))
    , m_thermalCamera(std::make_shared<SimCamera>(CameraSpectrum::ThermalLWIR))
    , m_lrf(std::make_shared<SimLrf>())
    , m_illuminator(std::make_shared<SimIlluminator>())
    , m_connected(true)
{
}

SimulatedPayload::~SimulatedPayload()
{
    disconnect();
}

bool SimulatedPayload::connect()
{
    m_connected = true;
    m_ptu->connect();
    m_daylightCamera->connect();
    m_thermalCamera->connect();
    m_lrf->connect();
    m_illuminator->connect();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Ready, "Simulated Payload Online");
    }
    return true;
}

void SimulatedPayload::disconnect()
{
    m_connected = false;
    m_ptu->disconnect();
    m_daylightCamera->disconnect();
    m_thermalCamera->disconnect();
    m_lrf->disconnect();
    m_illuminator->disconnect();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Disconnected, "Simulated Payload Offline");
    }
}

bool SimulatedPayload::isConnected() const noexcept
{
    return m_connected;
}

DeviceState SimulatedPayload::state() const noexcept
{
    return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
}

DeviceInfo SimulatedPayload::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "Simulated Systems";
    d.model = "Multi-Sensor EO/IR/LRF Payload";
    d.firmwareVersion = "1.0.0-sim";
    return d;
}

void SimulatedPayload::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

std::shared_ptr<IPanTiltUnit> SimulatedPayload::panTilt() const noexcept
{
    return m_ptu;
}

std::shared_ptr<ICameraPayload> SimulatedPayload::primaryCamera() const noexcept
{
    return m_daylightCamera;
}

std::shared_ptr<ICameraPayload> SimulatedPayload::secondaryCamera() const noexcept
{
    return m_thermalCamera;
}

std::shared_ptr<ILaserRangeFinder> SimulatedPayload::lrf() const noexcept
{
    return m_lrf;
}

std::shared_ptr<ILaserIlluminator> SimulatedPayload::illuminator() const noexcept
{
    return m_illuminator;
}

std::optional<Klv::GeoPoint2D> SimulatedPayload::calculateTargetCoordinates(
    const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const
{
    const GimbalTelemetry ptuTelem = m_ptu->currentTelemetry();
    const Klv::GeoPoint3D platform3D { platformGps.latitudeDeg, platformGps.longitudeDeg, platformAltMeters };

    const auto lrfMeas = m_lrf->lastMeasurement();
    if (lrfMeas.has_value() && lrfMeas->valid && lrfMeas->slantRangeMeters > 0.0) {
        auto target3D = GeoreferenceUtils::computeTargetFromSlantRange(
            platform3D, platformHeadingDeg, ptuTelem.panAngleDeg, ptuTelem.tiltAngleDeg, lrfMeas->slantRangeMeters);
        if (target3D.has_value()) {
            return Klv::GeoPoint2D { target3D->latitudeDeg, target3D->longitudeDeg };
        }
    }

    // Fallback to ground intersection
    auto groundTarget3D = GeoreferenceUtils::computeTargetFromGroundIntersection(
        platform3D, platformHeadingDeg, ptuTelem.panAngleDeg, ptuTelem.tiltAngleDeg, m_simGroundElevation);
    if (groundTarget3D.has_value()) {
        return Klv::GeoPoint2D { groundTarget3D->latitudeDeg, groundTarget3D->longitudeDeg };
    }

    return std::nullopt;
}

void SimulatedPayload::setSimulatedSlantRange(double rangeMeters)
{
    m_lrf->setSimRange(rangeMeters);
}

void SimulatedPayload::setSimulatedGroundElevation(double groundElevationM)
{
    m_simGroundElevation = groundElevationM;
}

} // namespace PayloadHal
