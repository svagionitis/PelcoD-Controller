#include "PelcoDViscaCompositePayload.h"
#include "GeoreferenceUtils.h"
#include "PelcoDPtzAdapter.h"
#include "ViscaSonyAdapter.h"

namespace PayloadHal {

PelcoDViscaCompositePayload::PelcoDViscaCompositePayload(
    std::shared_ptr<PelcoD::PelcoDDevice> ptzDevice, std::shared_ptr<Visca::Sony::SonyFCBDevice> cameraDevice)
    : m_ptu(ptzDevice ? std::make_shared<PelcoDPtzAdapter>(ptzDevice) : nullptr)
    , m_camera(cameraDevice ? std::make_shared<ViscaSonyAdapter>(cameraDevice) : nullptr)
    , m_pelcoDevice(std::move(ptzDevice))
    , m_sonyDevice(std::move(cameraDevice))
{
    if (m_ptu) {
        m_ptuState.store(m_ptu->isConnected() ? DeviceState::Ready : DeviceState::Disconnected);
        m_ptu->registerStateCallback([this](DeviceState st, const std::string& /*reason*/) {
            m_ptuState.store(st);
            handleSubsystemStateChange();
        });
    }
    if (m_camera) {
        m_cameraState.store(m_camera->isConnected() ? DeviceState::Ready : DeviceState::Disconnected);
        m_camera->registerStateCallback([this](DeviceState st, const std::string& /*reason*/) {
            m_cameraState.store(st);
            handleSubsystemStateChange();
        });
    }
}

PelcoDViscaCompositePayload::PelcoDViscaCompositePayload(
    std::shared_ptr<IPanTiltUnit> ptu, std::shared_ptr<ICameraPayload> camera)
    : m_ptu(std::move(ptu))
    , m_camera(std::move(camera))
{
    if (m_ptu) {
        m_ptuState.store(m_ptu->isConnected() ? DeviceState::Ready : DeviceState::Disconnected);
        m_ptu->registerStateCallback([this](DeviceState st, const std::string& /*reason*/) {
            m_ptuState.store(st);
            handleSubsystemStateChange();
        });
    }
    if (m_camera) {
        m_cameraState.store(m_camera->isConnected() ? DeviceState::Ready : DeviceState::Disconnected);
        m_camera->registerStateCallback([this](DeviceState st, const std::string& /*reason*/) {
            m_cameraState.store(st);
            handleSubsystemStateChange();
        });
    }
}

PelcoDViscaCompositePayload::~PelcoDViscaCompositePayload()
{
    disconnect();
}

bool PelcoDViscaCompositePayload::connect()
{
    const bool ptuOk = m_ptu ? m_ptu->connect() : false;
    const bool camOk = m_camera ? m_camera->connect() : false;
    m_ptuState.store(ptuOk ? DeviceState::Ready : DeviceState::Disconnected);
    m_cameraState.store(camOk ? DeviceState::Ready : DeviceState::Disconnected);
    handleSubsystemStateChange();
    return ptuOk && camOk;
}

void PelcoDViscaCompositePayload::disconnect()
{
    if (m_ptu) {
        m_ptu->disconnect();
    }
    if (m_camera) {
        m_camera->disconnect();
    }
    m_ptuState.store(DeviceState::Disconnected);
    m_cameraState.store(DeviceState::Disconnected);
    handleSubsystemStateChange();
}

[[nodiscard]] bool PelcoDViscaCompositePayload::isConnected() const noexcept
{
    const bool ptuConn = m_ptu ? m_ptu->isConnected() : false;
    const bool camConn = m_camera ? m_camera->isConnected() : false;
    return ptuConn && camConn;
}

[[nodiscard]] DeviceState PelcoDViscaCompositePayload::state() const noexcept
{
    const auto ptuSt = m_ptu ? m_ptuState.load() : DeviceState::Disconnected;
    const auto camSt = m_camera ? m_cameraState.load() : DeviceState::Disconnected;

    if (ptuSt == DeviceState::Ready && camSt == DeviceState::Ready) {
        return DeviceState::Ready;
    }
    if (ptuSt == DeviceState::Fault || camSt == DeviceState::Fault) {
        return (ptuSt == DeviceState::Ready || camSt == DeviceState::Ready) ? DeviceState::Degraded
                                                                            : DeviceState::Fault;
    }
    if (ptuSt == DeviceState::Ready || camSt == DeviceState::Ready) {
        return DeviceState::Degraded;
    }
    if (ptuSt == DeviceState::Connecting || camSt == DeviceState::Connecting) {
        return DeviceState::Connecting;
    }
    if (ptuSt == DeviceState::Standby && camSt == DeviceState::Standby) {
        return DeviceState::Standby;
    }
    return DeviceState::Disconnected;
}

[[nodiscard]] DeviceInfo PelcoDViscaCompositePayload::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "Pelco / Sony";
    d.model = "Pelco-D PT + Sony FCB VISCA Composite";
    if (m_camera) {
        const auto camInfo = m_camera->info();
        if (!camInfo.model.empty()) {
            d.model += " (" + camInfo.model + ")";
        }
        d.firmwareVersion = camInfo.firmwareVersion;
    }
    return d;
}

void PelcoDViscaCompositePayload::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

[[nodiscard]] std::shared_ptr<IPanTiltUnit> PelcoDViscaCompositePayload::panTilt() const noexcept
{
    return m_ptu;
}

[[nodiscard]] std::shared_ptr<ICameraPayload> PelcoDViscaCompositePayload::primaryCamera() const noexcept
{
    return m_camera;
}

[[nodiscard]] std::shared_ptr<ICameraPayload> PelcoDViscaCompositePayload::secondaryCamera() const noexcept
{
    return nullptr;
}

[[nodiscard]] std::shared_ptr<ILaserRangeFinder> PelcoDViscaCompositePayload::lrf() const noexcept
{
    return nullptr;
}

[[nodiscard]] std::optional<Klv::GeoPoint2D> PelcoDViscaCompositePayload::calculateTargetCoordinates(
    const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const
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

[[nodiscard]] std::shared_ptr<PelcoD::PelcoDDevice> PelcoDViscaCompositePayload::underlyingPelcoDevice() const noexcept
{
    return m_pelcoDevice;
}

[[nodiscard]] std::shared_ptr<Visca::Sony::SonyFCBDevice>
PelcoDViscaCompositePayload::underlyingSonyDevice() const noexcept
{
    return m_sonyDevice;
}

void PelcoDViscaCompositePayload::handleSubsystemStateChange()
{
    StateCallback cbCopy {};
    DeviceState currentState {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentState = state();
        if (currentState != m_lastAggregatedState) {
            m_lastAggregatedState = currentState;
            cbCopy = m_stateCallback;
        }
    }
    if (cbCopy) {
        std::string reason;
        switch (currentState) {
        case DeviceState::Ready:
            reason = "All subsystems operational";
            break;
        case DeviceState::Degraded:
            reason = "Subsystem degraded";
            break;
        case DeviceState::Fault:
            reason = "Subsystem fault detected";
            break;
        case DeviceState::Connecting:
            reason = "Connecting subsystems";
            break;
        case DeviceState::Standby:
            reason = "Subsystems in standby";
            break;
        case DeviceState::Disconnected:
        default:
            reason = "Subsystems disconnected";
            break;
        }
        cbCopy(currentState, reason);
    }
}

} // namespace PayloadHal
