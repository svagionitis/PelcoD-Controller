#include "Stanag4586Bridge.h"
#include "GimbalSectorBlanking.h"
#include "PayloadHealthMonitor.h"
#include "PayloadStowController.h"
#include "SensorFusionManager.h"

#include <chrono>
#include <cstring>

namespace PayloadHal {

Stanag4586Bridge::Stanag4586Bridge(std::shared_ptr<IPayload> payload,
                                   StanagBridgeConfig config) noexcept
    : m_payload(std::move(payload))
    , m_config(std::move(config))
    , m_navData(m_config.defaultNavData)
{
    if (m_config.enablePeriodicTelemetry) {
        startPeriodicTelemetry(m_config.telemetryInterval);
    }
}

Stanag4586Bridge::~Stanag4586Bridge()
{
    stopPeriodicTelemetry();
}

void Stanag4586Bridge::setConfig(const StanagBridgeConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool restartTelemetry = m_telemetryRunning && (config.telemetryInterval != m_config.telemetryInterval);
    m_config = config;
    if (restartTelemetry) {
        stopPeriodicTelemetry();
        startPeriodicTelemetry(m_config.telemetryInterval);
    }
}

StanagBridgeConfig Stanag4586Bridge::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void Stanag4586Bridge::updateNavData(const PlatformNavData& nav)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_navData = nav;
}

PlatformNavData Stanag4586Bridge::navData() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_navData;
}

std::uint32_t Stanag4586Bridge::nextSequenceNumber() noexcept
{
    return m_sequenceNumber.fetch_add(1U, std::memory_order_relaxed);
}

void Stanag4586Bridge::setPacketTxCallback(PacketTxCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetTxCallback = std::move(cb);
}

void Stanag4586Bridge::setMessage2000Callback(Message2000Callback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_msg2000Callback = std::move(cb);
}

void Stanag4586Bridge::setMessage2002Callback(Message2002Callback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_msg2002Callback = std::move(cb);
}

void Stanag4586Bridge::setMessage2003Callback(Message2003Callback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_msg2003Callback = std::move(cb);
}

void Stanag4586Bridge::setMessage2004Callback(Message2004Callback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_msg2004Callback = std::move(cb);
}

StanagParseResult Stanag4586Bridge::processInboundPacket(std::string_view packet)
{
    return processInboundBytes(reinterpret_cast<const std::uint8_t*>(packet.data()), packet.size());
}

StanagParseResult Stanag4586Bridge::processInboundBytes(const std::uint8_t* data, std::size_t length)
{
    if (data == nullptr || length < STANAG4586_MIN_PACKET_SIZE) {
        return StanagParseResult::BufferTooShort;
    }

    // 1. Verify preamble sync bytes (0x45, 0x86)
    if (data[0] != STANAG4586_SYNC_BYTE_0 || data[1] != STANAG4586_SYNC_BYTE_1) {
        return StanagParseResult::InvalidSync;
    }

    const std::uint16_t msgIdRaw = StanagWire::readUint16Be(&data[2]);
    const std::uint16_t msgLen = StanagWire::readUint16Be(&data[4]);

    const std::size_t totalExpectedSize = STANAG4586_HEADER_SIZE + msgLen + 2U;
    if (length < totalExpectedSize) {
        return StanagParseResult::BufferTooShort;
    }

    // 2. Verify CRC-16 CCITT over header + payload
    const std::uint16_t expectedCrc = StanagWire::computeCrc16Ccitt(data, STANAG4586_HEADER_SIZE + msgLen);
    const std::uint16_t actualCrc = StanagWire::readUint16Be(&data[STANAG4586_HEADER_SIZE + msgLen]);
    if (expectedCrc != actualCrc) {
        return StanagParseResult::CrcMismatch;
    }

    // Read header fields
    Stanag4586Header hdr;
    hdr.syncMarker = StanagWire::readUint16Be(&data[0]);
    hdr.messageId = msgIdRaw;
    hdr.messageLength = msgLen;
    hdr.timestampUs = StanagWire::readUint64Be(&data[6]);
    hdr.sourceId = StanagWire::readUint32Be(&data[14]);
    hdr.destinationId = StanagWire::readUint32Be(&data[18]);
    hdr.sequenceNumber = StanagWire::readUint32Be(&data[22]);
    hdr.stationId = data[24];
    hdr.flags = data[25];

    std::uint8_t myStationId = 0U;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        myStationId = m_config.stationId;
    }

    // Station check (0xFF = broadcast to all stations)
    if (hdr.stationId != 0xFFU && hdr.stationId != myStationId) {
        return StanagParseResult::StationMismatch;
    }

    const std::uint8_t* payloadPtr = &data[STANAG4586_HEADER_SIZE];

    switch (static_cast<StanagMessageId>(msgIdRaw)) {
    case StanagMessageId::PayloadConfigurationReq: {
        if (msgLen < STANAG_PAYLOAD_SIZE_2001) {
            return StanagParseResult::InvalidPayloadSize;
        }
        Stanag4586Message2001 req;
        req.stationId = payloadPtr[0];
        return dispatchMessage2001(req) ? StanagParseResult::Success : StanagParseResult::DispatchFailed;
    }
    case StanagMessageId::PayloadOperatingCommand: {
        if (msgLen < STANAG_PAYLOAD_SIZE_2003) {
            return StanagParseResult::InvalidPayloadSize;
        }
        Stanag4586Message2003 cmd;
        cmd.stationId = payloadPtr[0];
        cmd.commandedMode = static_cast<StanagOperatingMode>(payloadPtr[1]);
        cmd.sensorSelect = static_cast<StanagSensorChannel>(payloadPtr[2]);
        cmd.focusMode = static_cast<StanagFocusMode>(payloadPtr[3]);
        cmd.manualFocusVelocity = StanagWire::readFloatBe(&payloadPtr[4]);
        cmd.irisMode = static_cast<StanagIrisMode>(payloadPtr[8]);
        cmd.lrfCommand = static_cast<StanagLaserCommand>(payloadPtr[9]);
        cmd.illuminatorCommand = static_cast<StanagIlluminatorCommand>(payloadPtr[10]);
        cmd.manualIrisNormalized = StanagWire::readFloatBe(&payloadPtr[12]);
        cmd.auxiliaryControls = StanagWire::readUint16Be(&payloadPtr[16]);
        return dispatchMessage2003(cmd) ? StanagParseResult::Success : StanagParseResult::DispatchFailed;
    }
    case StanagMessageId::PayloadSteeringCommand: {
        if (msgLen < STANAG_PAYLOAD_SIZE_2004) {
            return StanagParseResult::InvalidPayloadSize;
        }
        Stanag4586Message2004 cmd;
        cmd.stationId = payloadPtr[0];
        cmd.steeringMode = static_cast<StanagSteeringMode>(payloadPtr[1]);
        cmd.commandedAzimuthDeg = StanagWire::readFloatBe(&payloadPtr[4]);
        cmd.commandedElevationDeg = StanagWire::readFloatBe(&payloadPtr[8]);
        cmd.commandedRollDeg = StanagWire::readFloatBe(&payloadPtr[12]);
        cmd.maxSlewRateDegPerSec = StanagWire::readFloatBe(&payloadPtr[16]);
        cmd.commandedAzimuthRateDegPerSec = StanagWire::readFloatBe(&payloadPtr[20]);
        cmd.commandedElevationRateDegPerSec = StanagWire::readFloatBe(&payloadPtr[24]);
        cmd.targetLatitudeDeg = StanagWire::readDoubleBe(&payloadPtr[28]);
        cmd.targetLongitudeDeg = StanagWire::readDoubleBe(&payloadPtr[36]);
        cmd.targetAltitudeMslMeters = StanagWire::readFloatBe(&payloadPtr[44]);
        return dispatchMessage2004(cmd) ? StanagParseResult::Success : StanagParseResult::DispatchFailed;
    }
    case StanagMessageId::PayloadConfiguration:
    case StanagMessageId::PayloadOperatingState:
        // Recognized outbound reports received passively
        return StanagParseResult::Success;
    default:
        return StanagParseResult::UnsupportedMessageId;
    }
}

bool Stanag4586Bridge::dispatchMessage2001(const Stanag4586Message2001& req)
{
    bool autoReply = false;
    std::uint8_t myStation = 0U;
    PacketTxCallback packetCb {};
    Message2000Callback msgCb {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        autoReply = m_config.autoRespondToConfigRequests;
        myStation = m_config.stationId;
        packetCb = m_packetTxCallback;
        msgCb = m_msg2000Callback;
    }

    if (req.stationId != 0xFFU && req.stationId != myStation) {
        return false;
    }

    if (autoReply) {
        const auto packet = serializeMessage2000(myStation);
        if (packetCb) {
            packetCb(packet);
        }
        if (msgCb) {
            msgCb(buildMessage2000(myStation));
        }
    }

    return true;
}

bool Stanag4586Bridge::dispatchMessage2003(const Stanag4586Message2003& cmd)
{
    Message2003Callback cb {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_msg2003Callback;
    }
    if (cb) {
        cb(cmd);
    }

    if (!m_payload) {
        return false;
    }

    // 1. Operating Mode Commands
    const auto stow = m_payload->stowController();
    const auto ptu = m_payload->panTilt();

    switch (cmd.commandedMode) {
    case StanagOperatingMode::Standby:
        if (stow && stow->isStowed()) {
            stow->deploy();
        }
        if (m_payload->isGeoLocked()) {
            m_payload->disengageGeoLock();
        }
        break;
    case StanagOperatingMode::Active:
        if (stow && stow->isStowed()) {
            stow->deploy();
        }
        break;
    case StanagOperatingMode::Stowed:
        if (stow) {
            stow->stow();
        }
        break;
    case StanagOperatingMode::Caged:
        if (ptu) {
            ptu->setAbsoluteAngles(0.0, 0.0);
        }
        break;
    case StanagOperatingMode::EmergencyPark:
        if (stow) {
            stow->emergencyPark();
        }
        break;
    case StanagOperatingMode::Off:
    case StanagOperatingMode::Fault:
    default:
        break;
    }

    // 2. Sensor Channel Selection
    const auto fusion = m_payload->sensorFusion();
    if (fusion) {
        if (cmd.sensorSelect == StanagSensorChannel::ThermalIr) {
            fusion->setActiveChannel(OpticalChannel::Secondary);
        } else {
            fusion->setActiveChannel(OpticalChannel::Primary);
        }
    }

    // 3. Optics (Focus & Iris)
    auto activeCam = (cmd.sensorSelect == StanagSensorChannel::ThermalIr && m_payload->secondaryCamera())
        ? m_payload->secondaryCamera()
        : m_payload->primaryCamera();

    if (activeCam) {
        switch (cmd.focusMode) {
        case StanagFocusMode::Auto:
            activeCam->setFocusAuto(true);
            break;
        case StanagFocusMode::ManualNear:
            activeCam->focusContinuous(-std::abs(cmd.manualFocusVelocity));
            break;
        case StanagFocusMode::ManualFar:
            activeCam->focusContinuous(std::abs(cmd.manualFocusVelocity));
            break;
        case StanagFocusMode::OnePush:
            activeCam->triggerOnePushFocus();
            break;
        case StanagFocusMode::Hold:
            activeCam->focusStop();
            break;
        }

        switch (cmd.irisMode) {
        case StanagIrisMode::Auto:
            activeCam->setIrisAuto(true);
            break;
        case StanagIrisMode::ManualOpen:
        case StanagIrisMode::ManualClose:
            activeCam->setIrisNormalized(static_cast<double>(cmd.manualIrisNormalized));
            break;
        case StanagIrisMode::Hold:
            activeCam->irisStop();
            break;
        }
    }

    // 4. Laser Range Finder Commands
    const auto lrf = m_payload->lrf();
    if (lrf) {
        switch (cmd.lrfCommand) {
        case StanagLaserCommand::Disarm:
        case StanagLaserCommand::Inhibit:
            lrf->disarmLaser();
            break;
        case StanagLaserCommand::Arm:
            lrf->armLaser();
            break;
        case StanagLaserCommand::FireSinglePulse:
        case StanagLaserCommand::FireContinuous:
            if (!lrf->isArmed()) {
                lrf->armLaser();
            }
            lrf->triggerSingleMeasurement();
            break;
        }
    }

    // 5. Laser Pointer / Illuminator Commands
    const auto ill = m_payload->illuminator();
    if (ill) {
        switch (cmd.illuminatorCommand) {
        case StanagIlluminatorCommand::Off:
            ill->stopEmission();
            ill->disarmLaser();
            break;
        case StanagIlluminatorCommand::Continuous:
            ill->armLaser();
            ill->setMode(IlluminatorMode::Continuous);
            ill->startEmission();
            break;
        case StanagIlluminatorCommand::Strobe:
            ill->armLaser();
            ill->setMode(IlluminatorMode::Pulsed);
            ill->startEmission();
            break;
        }
    }

    // 6. Auxiliary Servicing Controls
    if (stow) {
        if ((cmd.auxiliaryControls & StanagAuxControls::DeIceHeaterOn) != 0U) {
            stow->setHeaterMode(HeaterMode::ManualOn);
        } else if ((cmd.auxiliaryControls & StanagAuxControls::DeIceHeaterOff) != 0U) {
            stow->setHeaterMode(HeaterMode::Off);
        }

        if ((cmd.auxiliaryControls & StanagAuxControls::WiperSingleCycle) != 0U) {
            stow->triggerSingleWipe();
        } else if ((cmd.auxiliaryControls & StanagAuxControls::WiperContinuous) != 0U) {
            stow->setWiperMode(WiperMode::Continuous);
        } else if ((cmd.auxiliaryControls & StanagAuxControls::WiperStop) != 0U) {
            stow->setWiperMode(WiperMode::Off);
        }

        if ((cmd.auxiliaryControls & StanagAuxControls::WashCycleTrigger) != 0U) {
            stow->startWasherRoutine();
        }

        if ((cmd.auxiliaryControls & StanagAuxControls::EmergencyPark) != 0U) {
            stow->emergencyPark();
        }

        if ((cmd.auxiliaryControls & StanagAuxControls::ZeroizePresets) != 0U) {
            stow->zeroize(ZeroizeReason::OperatorCommand);
        }
    }

    return true;
}

bool Stanag4586Bridge::dispatchMessage2004(const Stanag4586Message2004& cmd)
{
    Message2004Callback cb {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_msg2004Callback;
    }
    if (cb) {
        cb(cmd);
    }

    if (!m_payload) {
        return false;
    }

    const auto ptu = m_payload->panTilt();

    switch (cmd.steeringMode) {
    case StanagSteeringMode::AnglePosition: {
        if (!ptu) {
            return false;
        }
        if (m_payload->isGeoLocked()) {
            m_payload->disengageGeoLock();
        }
        const bool panTiltOk = ptu->setAbsoluteAngles(cmd.commandedAzimuthDeg, cmd.commandedElevationDeg);
        if (std::abs(cmd.commandedRollDeg) > 1e-3f && ptu->hasRollAxis()) {
            ptu->setRollAngle(cmd.commandedRollDeg);
        }
        return panTiltOk;
    }
    case StanagSteeringMode::RateVelocity: {
        if (!ptu) {
            return false;
        }
        if (m_payload->isGeoLocked()) {
            m_payload->disengageGeoLock();
        }
        return ptu->setRate(cmd.commandedAzimuthRateDegPerSec, cmd.commandedElevationRateDegPerSec);
    }
    case StanagSteeringMode::GeoPointing: {
        PlatformNavData nav;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            nav = m_navData;
        }
        const Klv::GeoPoint3D targetGeo {
            cmd.targetLatitudeDeg,
            cmd.targetLongitudeDeg,
            static_cast<double>(cmd.targetAltitudeMslMeters)
        };
        m_payload->engageGeoLock(targetGeo);
        return m_payload->slewToGeoTarget(nav.position, nav.headingDeg, targetGeo);
    }
    case StanagSteeringMode::SlavedTrack: {
        // Slaved tracking mode engagement
        return true;
    }
    case StanagSteeringMode::StowCage: {
        if (const auto stow = m_payload->stowController()) {
            return stow->stow();
        }
        if (ptu) {
            return ptu->setAbsoluteAngles(0.0, 0.0);
        }
        return false;
    }
    default:
        return false;
    }
}

Stanag4586Message2000 Stanag4586Bridge::buildMessage2000(std::optional<std::uint8_t> stationId) const
{
    Stanag4586Message2000 msg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        msg.stationId = stationId.value_or(m_config.stationId);
    }

    if (!m_payload) {
        return msg;
    }

    msg.payloadType = StanagPayloadType::MultiSensorEoIr;
    msg.capabilities = 0U;

    const auto ptu = m_payload->panTilt();
    if (ptu) {
        msg.capabilities |= StanagCapabilities::PanTiltGimbal;
        if (ptu->hasRollAxis()) {
            msg.capabilities |= StanagCapabilities::RollStabilization;
            double minRoll = -60.0;
            double maxRoll = 60.0;
            if (ptu->getRollLimits(minRoll, maxRoll)) {
                msg.minRollDeg = static_cast<float>(minRoll);
                msg.maxRollDeg = static_cast<float>(maxRoll);
            }
        }

        double minPan = -180.0;
        double maxPan = 180.0;
        double minTilt = -90.0;
        double maxTilt = 20.0;
        if (ptu->getLimits(minPan, maxPan, minTilt, maxTilt)) {
            msg.minAzimuthDeg = static_cast<float>(minPan);
            msg.maxAzimuthDeg = static_cast<float>(maxPan);
            msg.minElevationDeg = static_cast<float>(minTilt);
            msg.maxElevationDeg = static_cast<float>(maxTilt);
        }

        msg.maxPanRateDegPerSec = 60.0f;
        msg.maxTiltRateDegPerSec = 30.0f;
        msg.maxRollRateDegPerSec = 30.0f;
    }

    const auto primCam = m_payload->primaryCamera();
    if (primCam) {
        msg.capabilities |= StanagCapabilities::DaylightEoCamera;
        msg.capabilities |= StanagCapabilities::OpticalZoom;
        msg.capabilities |= StanagCapabilities::MotorizedFocus;
        msg.capabilities |= StanagCapabilities::MotorizedIris;

        const auto camTelem = primCam->currentTelemetry();
        msg.minHorizontalFovDeg = 2.0f;
        msg.maxHorizontalFovDeg = static_cast<float>(camTelem.horizontalFovDeg > 0.0 ? camTelem.horizontalFovDeg : 60.0);
    }

    if (m_payload->secondaryCamera()) {
        msg.capabilities |= StanagCapabilities::ThermalIrCamera;
    }

    if (m_payload->lrf()) {
        msg.capabilities |= StanagCapabilities::LaserRangeFinder;
    }

    if (m_payload->illuminator()) {
        msg.capabilities |= StanagCapabilities::LaserIlluminator;
    }

    if (m_payload->stowController()) {
        msg.capabilities |= StanagCapabilities::WindowDeIceWiper;
    }

    if (m_payload->sensorFusion()) {
        msg.capabilities |= StanagCapabilities::MultiSensorFusion;
    }

    msg.capabilities |= StanagCapabilities::GeoLockTracking;

    return msg;
}

std::vector<std::uint8_t> Stanag4586Bridge::serializeMessage2000(std::optional<std::uint8_t> stationId) const
{
    const auto msg = buildMessage2000(stationId);
    std::uint8_t payloadBytes[STANAG_PAYLOAD_SIZE_2000] = { 0 };

    payloadBytes[0] = msg.stationId;
    payloadBytes[1] = static_cast<std::uint8_t>(msg.payloadType);
    payloadBytes[2] = 0U; // Reserved
    payloadBytes[3] = 0U; // Reserved
    StanagWire::writeUint32Be(&payloadBytes[4], msg.capabilities);
    StanagWire::writeFloatBe(&payloadBytes[8], msg.minAzimuthDeg);
    StanagWire::writeFloatBe(&payloadBytes[12], msg.maxAzimuthDeg);
    StanagWire::writeFloatBe(&payloadBytes[16], msg.minElevationDeg);
    StanagWire::writeFloatBe(&payloadBytes[20], msg.maxElevationDeg);
    StanagWire::writeFloatBe(&payloadBytes[24], msg.minRollDeg);
    StanagWire::writeFloatBe(&payloadBytes[28], msg.maxRollDeg);
    StanagWire::writeFloatBe(&payloadBytes[32], msg.maxPanRateDegPerSec);
    StanagWire::writeFloatBe(&payloadBytes[36], msg.maxTiltRateDegPerSec);
    StanagWire::writeFloatBe(&payloadBytes[40], msg.maxRollRateDegPerSec);
    StanagWire::writeFloatBe(&payloadBytes[44], msg.minHorizontalFovDeg);
    StanagWire::writeFloatBe(&payloadBytes[48], msg.maxHorizontalFovDeg);

    return framePayload(StanagMessageId::PayloadConfiguration, msg.stationId, payloadBytes, sizeof(payloadBytes));
}

Stanag4586Message2002 Stanag4586Bridge::buildMessage2002(std::optional<PlatformNavData> navOpt) const
{
    Stanag4586Message2002 msg;
    PlatformNavData nav;
    double groundElevationM = 0.0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        msg.stationId = m_config.stationId;
        nav = navOpt.value_or(m_navData);
        groundElevationM = m_config.groundElevationM;
    }

    if (!m_payload) {
        return msg;
    }

    // 1. PTU Orientation & Rates
    const auto ptu = m_payload->panTilt();
    if (ptu) {
        const auto ptuTelem = ptu->currentTelemetry();
        msg.azimuthDeg = static_cast<float>(ptuTelem.panAngleDeg);
        msg.elevationDeg = static_cast<float>(ptuTelem.tiltAngleDeg);
        msg.rollDeg = static_cast<float>(ptuTelem.rollAngleDeg);
        msg.azimuthRateDegPerSec = static_cast<float>(ptuTelem.panRateDegPerSec);
        msg.elevationRateDegPerSec = static_cast<float>(ptuTelem.tiltRateDegPerSec);
        msg.rollRateDegPerSec = static_cast<float>(ptuTelem.rollRateDegPerSec);
    }

    // 2. Active Camera Optics
    const auto primCam = m_payload->primaryCamera();
    if (primCam) {
        const auto camTelem = primCam->currentTelemetry();
        msg.horizontalFovDeg = static_cast<float>(camTelem.horizontalFovDeg);
        msg.verticalFovDeg = static_cast<float>(camTelem.verticalFovDeg);
        msg.zoomMagnification = static_cast<float>(camTelem.opticalZoomFactor);
    }

    // 3. Active Sensor Channel
    const auto fusion = m_payload->sensorFusion();
    if (fusion && fusion->activeChannel() == OpticalChannel::Secondary) {
        msg.activeSensor = StanagSensorChannel::ThermalIr;
    } else {
        msg.activeSensor = StanagSensorChannel::DaylightEo;
    }

    // 4. LRF Echo & Laser Status
    const auto lrf = m_payload->lrf();
    if (lrf) {
        if (lrf->isArmed()) {
            msg.laserState = StanagLaserState::Armed;
        } else {
            msg.laserState = StanagLaserState::Disarmed;
        }

        const auto meas = lrf->lastMeasurement();
        if (meas.has_value() && meas->valid) {
            msg.slantRangeMeters = static_cast<float>(meas->slantRangeMeters);
        }
    }

    // 5. Illuminator State
    const auto ill = m_payload->illuminator();
    if (ill) {
        if (ill->isArmed()) {
            msg.illuminatorState = (ill->mode() == IlluminatorMode::Pulsed) ? 2U : 1U;
        } else {
            msg.illuminatorState = 0U;
        }
    }

    // 6. Operating Mode
    const auto stow = m_payload->stowController();
    if (stow && stow->state() == StowState::EmergencyParked) {
        msg.operatingMode = StanagOperatingMode::EmergencyPark;
    } else if (stow && (stow->isStowed() || stow->state() == StowState::Stowing)) {
        msg.operatingMode = StanagOperatingMode::Stowed;
    } else if (ptu && ptu->isConnected()) {
        msg.operatingMode = StanagOperatingMode::Active;
    } else {
        msg.operatingMode = StanagOperatingMode::Standby;
    }

    // 7. Built-In-Test State
    const auto health = m_payload->healthMonitor();
    if (health) {
        switch (health->healthReport().overallState) {
        case DeviceState::Ready:
            msg.bitStatus = StanagBitStatus::Ok;
            break;
        case DeviceState::Degraded:
            msg.bitStatus = StanagBitStatus::Degraded;
            break;
        case DeviceState::Fault:
        default:
            msg.bitStatus = StanagBitStatus::Fault;
            break;
        }
    }

    // 8. Geodetic Line-of-Sight Ground Intersection
    const Klv::GeoPoint2D platform2D { nav.position.latitudeDeg, nav.position.longitudeDeg };
    const auto tgtCoord = m_payload->calculateTargetCoordinates(
        platform2D, nav.headingDeg, nav.position.altitudeM);

    if (tgtCoord.has_value()) {
        msg.targetLocationValid = 1U;
        msg.targetLatitudeDeg = tgtCoord->latitudeDeg;
        msg.targetLongitudeDeg = tgtCoord->longitudeDeg;
        msg.targetAltitudeMslMeters = static_cast<float>(groundElevationM);
    } else {
        msg.targetLocationValid = 0U;
    }

    return msg;
}

std::vector<std::uint8_t> Stanag4586Bridge::serializeMessage2002(std::optional<PlatformNavData> navOpt) const
{
    const auto msg = buildMessage2002(navOpt);
    std::uint8_t payloadBytes[STANAG_PAYLOAD_SIZE_2002] = { 0 };

    payloadBytes[0] = msg.stationId;
    payloadBytes[1] = static_cast<std::uint8_t>(msg.operatingMode);
    payloadBytes[2] = static_cast<std::uint8_t>(msg.activeSensor);
    payloadBytes[3] = static_cast<std::uint8_t>(msg.laserState);
    StanagWire::writeFloatBe(&payloadBytes[4], msg.azimuthDeg);
    StanagWire::writeFloatBe(&payloadBytes[8], msg.elevationDeg);
    StanagWire::writeFloatBe(&payloadBytes[12], msg.rollDeg);
    StanagWire::writeFloatBe(&payloadBytes[16], msg.azimuthRateDegPerSec);
    StanagWire::writeFloatBe(&payloadBytes[20], msg.elevationRateDegPerSec);
    StanagWire::writeFloatBe(&payloadBytes[24], msg.rollRateDegPerSec);
    StanagWire::writeFloatBe(&payloadBytes[28], msg.horizontalFovDeg);
    StanagWire::writeFloatBe(&payloadBytes[32], msg.verticalFovDeg);
    StanagWire::writeFloatBe(&payloadBytes[36], msg.zoomMagnification);
    StanagWire::writeFloatBe(&payloadBytes[40], msg.slantRangeMeters);
    payloadBytes[44] = msg.illuminatorState;
    payloadBytes[45] = static_cast<std::uint8_t>(msg.bitStatus);
    payloadBytes[46] = msg.targetLocationValid;
    payloadBytes[47] = 0U; // Reserved
    StanagWire::writeDoubleBe(&payloadBytes[48], msg.targetLatitudeDeg);
    StanagWire::writeDoubleBe(&payloadBytes[56], msg.targetLongitudeDeg);
    StanagWire::writeFloatBe(&payloadBytes[64], msg.targetAltitudeMslMeters);
    payloadBytes[68] = 0U; // Reserved
    payloadBytes[69] = 0U;
    payloadBytes[70] = 0U;
    payloadBytes[71] = 0U;

    return framePayload(StanagMessageId::PayloadOperatingState, msg.stationId, payloadBytes, sizeof(payloadBytes));
}

std::vector<std::uint8_t> Stanag4586Bridge::framePayload(
    StanagMessageId msgId, std::uint8_t stationId, const std::uint8_t* payloadData, std::size_t payloadLen) const
{
    const std::size_t totalPacketSize = STANAG4586_HEADER_SIZE + payloadLen + 2U;
    std::vector<std::uint8_t> packet(totalPacketSize, 0U);

    // 1. Header (26 bytes)
    StanagWire::writeUint16Be(&packet[0], STANAG4586_SYNC_MARKER);
    StanagWire::writeUint16Be(&packet[2], static_cast<std::uint16_t>(msgId));
    StanagWire::writeUint16Be(&packet[4], static_cast<std::uint16_t>(payloadLen));

    const auto nowUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    StanagWire::writeUint64Be(&packet[6], nowUs);

    std::uint32_t srcId = 0U;
    std::uint32_t dstId = 0U;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        srcId = m_config.localNodeId;
        dstId = m_config.remoteC2NodeId;
    }
    StanagWire::writeUint32Be(&packet[14], srcId);
    StanagWire::writeUint32Be(&packet[18], dstId);
    StanagWire::writeUint32Be(&packet[22], const_cast<Stanag4586Bridge*>(this)->nextSequenceNumber());
    packet[24] = stationId;
    packet[25] = 0U; // Flags

    // 2. Payload
    if (payloadData != nullptr && payloadLen > 0U) {
        std::memcpy(&packet[STANAG4586_HEADER_SIZE], payloadData, payloadLen);
    }

    // 3. CRC-16 CCITT over header + payload
    const std::uint16_t crc = StanagWire::computeCrc16Ccitt(packet.data(), STANAG4586_HEADER_SIZE + payloadLen);
    StanagWire::writeUint16Be(&packet[STANAG4586_HEADER_SIZE + payloadLen], crc);

    return packet;
}

void Stanag4586Bridge::startPeriodicTelemetry(std::chrono::milliseconds interval)
{
    if (m_telemetryRunning.exchange(true)) {
        return; // Already running
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.telemetryInterval = interval;
    }
    m_telemetryThread = std::thread(&Stanag4586Bridge::periodicTelemetryLoop, this);
}

void Stanag4586Bridge::stopPeriodicTelemetry()
{
    if (!m_telemetryRunning.exchange(false)) {
        return;
    }
    if (m_telemetryThread.joinable()) {
        m_telemetryThread.join();
    }
}

bool Stanag4586Bridge::isPeriodicTelemetryRunning() const noexcept
{
    return m_telemetryRunning.load(std::memory_order_relaxed);
}

void Stanag4586Bridge::periodicTelemetryLoop()
{
    while (m_telemetryRunning.load(std::memory_order_relaxed)) {
        std::chrono::milliseconds sleepDuration = std::chrono::milliseconds(100);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sleepDuration = m_config.telemetryInterval;
        }

        std::this_thread::sleep_for(sleepDuration);

        if (!m_telemetryRunning.load(std::memory_order_relaxed)) {
            break;
        }

        const auto packet = serializeMessage2002();
        PacketTxCallback txCb {};
        Message2002Callback msgCb {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            txCb = m_packetTxCallback;
            msgCb = m_msg2002Callback;
        }

        if (txCb) {
            txCb(packet);
        }
        if (msgCb) {
            msgCb(buildMessage2002());
        }
    }
}

} // namespace PayloadHal
