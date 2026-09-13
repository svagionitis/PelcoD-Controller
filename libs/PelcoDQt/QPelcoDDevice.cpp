/// @file QPelcoDDevice.cpp
/// @brief Implementation of Qt 6 QPelcoDDevice adapter.

#include "QPelcoDDevice.h"
#include "ProtocolParser.h"

#include <QMetaObject>
#include <QSignalBlocker>

namespace PelcoDQt {

QPelcoDDevice::QPelcoDDevice(std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address, QObject* parent)
    : QObject(parent)
    , m_transport { std::move(transport) }
    , m_address { address }
{
    if (m_transport) {
        m_device = std::make_unique<PelcoD::PelcoDDevice>(m_transport, m_address);
        initDeviceCallbacks();
    }
}

QPelcoDDevice::~QPelcoDDevice()
{
    // Join any in-flight async connect thread before tearing down.
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }
    // Block signals so connectionStateChanged(false) — emitted by
    // disconnectDevice() — cannot fire into slots whose receiver is
    // already mid-destruction (e.g. MainWindow::statusBar()).
    const QSignalBlocker blocker(this);
    disconnectDevice();
}

void QPelcoDDevice::setTransport(std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address)
{
    disconnectDevice();
    m_transport = std::move(transport);
    m_address = address;
    if (m_transport) {
        m_device = std::make_unique<PelcoD::PelcoDDevice>(m_transport, m_address);
        initDeviceCallbacks();
    } else {
        m_device.reset();
    }
}

void QPelcoDDevice::initDeviceCallbacks()
{
    if (!m_device) {
        return;
    }

    m_device->addStatusCallback([this](const PelcoD::DeviceStatus& status) {
        QMetaObject::invokeMethod(this, [this, status] {
            emit statusUpdated(status);
            if (!status.connected) {
                emit connectionStateChanged(false);
            }
        });
    });

    m_device->addTrafficCallback([this](bool isTx, const std::vector<std::uint8_t>& frame) {
        const QByteArray bytes(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
        const QString desc = describePacket(isTx, frame);
        QMetaObject::invokeMethod(this, [this, isTx, bytes, desc] { emit trafficLogged(isTx, bytes, desc); });
    });

    m_device->addTimeoutCallback([this](const std::string& queryTag) {
        const QString tag = QString::fromStdString(queryTag);
        QMetaObject::invokeMethod(this, [this, tag] { emit queryTimeoutOccurred(tag); });
    });
}

bool QPelcoDDevice::connectDevice()
{
    if (!m_device) {
        return false;
    }

    const bool ok = m_device->start();
    QMetaObject::invokeMethod(this, [this, ok] { emit connectionStateChanged(ok); });
    return ok;
}

void QPelcoDDevice::connectDeviceAsync()
{
    if (!m_device) {
        emit connectionStateChanged(false);
        return;
    }

    // If a previous connect thread is still running, do not spawn another.
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }

    emit connectingStateChanged(true);

    m_connectThread = std::thread([this] {
        const bool ok = m_device ? m_device->start() : false;
        // Post result back to the Qt main thread.
        QMetaObject::invokeMethod(this, [this, ok] {
            emit connectingStateChanged(false);
            emit connectionStateChanged(ok);
        });
    });
}

void QPelcoDDevice::disconnectDevice()
{
    // Cancel any in-flight async connect first.
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }
    if (m_device) {
        m_device->stop();
    }
    emit connectionStateChanged(false);
}

bool QPelcoDDevice::isConnected() const noexcept
{
    return m_device && m_device->isConnected();
}

PelcoD::DeviceStatus QPelcoDDevice::currentStatus() const
{
    if (m_device) {
        return m_device->getStatus();
    }
    return {};
}

PelcoD::DeviceInfo QPelcoDDevice::deviceInfo() const
{
    if (m_device) {
        return m_device->getInfo();
    }
    return {};
}

PelcoD::PelcoDDevice* QPelcoDDevice::coreDevice() const noexcept
{
    return m_device.get();
}

void QPelcoDDevice::setTelemetryPolling(bool enable, int intervalMs)
{
    if (m_device) {
        m_device->setTelemetryPolling(enable, static_cast<std::uint32_t>(intervalMs));
    }
}

void QPelcoDDevice::setQueryTimeoutMs(int timeoutMs)
{
    if (m_device) {
        m_device->setQueryTimeoutMs(static_cast<std::uint32_t>(timeoutMs));
    }
}

void QPelcoDDevice::panLeft(int speed)
{
    if (m_device) {
        m_device->panLeft(static_cast<std::uint8_t>(speed));
    }
}

void QPelcoDDevice::panRight(int speed)
{
    if (m_device) {
        m_device->panRight(static_cast<std::uint8_t>(speed));
    }
}

void QPelcoDDevice::tiltUp(int speed)
{
    if (m_device) {
        m_device->tiltUp(static_cast<std::uint8_t>(speed));
    }
}

void QPelcoDDevice::tiltDown(int speed)
{
    if (m_device) {
        m_device->tiltDown(static_cast<std::uint8_t>(speed));
    }
}

void QPelcoDDevice::stopMotion()
{
    if (m_device) {
        m_device->stopMotion();
    }
}

void QPelcoDDevice::move(int panDir, int panSpeed, int tiltDir, int tiltSpeed)
{
    if (m_device) {
        m_device->move(static_cast<PelcoD::PanDirection>(panDir), static_cast<std::uint8_t>(panSpeed),
            static_cast<PelcoD::TiltDirection>(tiltDir), static_cast<std::uint8_t>(tiltSpeed));
    }
}

void QPelcoDDevice::zoomTele()
{
    if (m_device) {
        m_device->zoomTele();
    }
}

void QPelcoDDevice::zoomWide()
{
    if (m_device) {
        m_device->zoomWide();
    }
}

void QPelcoDDevice::zoomStop()
{
    if (m_device) {
        m_device->zoomStop();
    }
}

void QPelcoDDevice::focusNear()
{
    if (m_device) {
        m_device->focusNear();
    }
}

void QPelcoDDevice::focusFar()
{
    if (m_device) {
        m_device->focusFar();
    }
}

void QPelcoDDevice::focusStop()
{
    if (m_device) {
        m_device->focusStop();
    }
}

void QPelcoDDevice::irisOpen()
{
    if (m_device) {
        m_device->irisOpen();
    }
}

void QPelcoDDevice::irisClose()
{
    if (m_device) {
        m_device->irisClose();
    }
}

void QPelcoDDevice::irisStop()
{
    if (m_device) {
        m_device->irisStop();
    }
}

void QPelcoDDevice::setPanAngle(int centidegrees)
{
    if (m_device) {
        m_device->setPanAngle(static_cast<std::uint16_t>(centidegrees));
    }
}

void QPelcoDDevice::setTiltAngle(int centidegrees)
{
    if (m_device) {
        m_device->setTiltAngle(static_cast<std::uint16_t>(centidegrees));
    }
}

void QPelcoDDevice::setZoomPosition(int position)
{
    if (m_device) {
        m_device->setZoomPosition(static_cast<std::uint16_t>(position));
    }
}

void QPelcoDDevice::setPreset(int presetId)
{
    if (m_device) {
        m_device->setPreset(static_cast<std::uint8_t>(presetId));
    }
}

void QPelcoDDevice::clearPreset(int presetId)
{
    if (m_device) {
        m_device->clearPreset(static_cast<std::uint8_t>(presetId));
    }
}

void QPelcoDDevice::goToPreset(int presetId)
{
    if (m_device) {
        m_device->goToPreset(static_cast<std::uint8_t>(presetId));
    }
}

void QPelcoDDevice::flip180()
{
    if (m_device) {
        m_device->flip180();
    }
}

void QPelcoDDevice::zeroPan()
{
    if (m_device) {
        m_device->zeroPan();
    }
}

void QPelcoDDevice::setAuxiliary(int auxId)
{
    if (m_device) {
        m_device->setAuxiliary(static_cast<std::uint8_t>(auxId));
    }
}

void QPelcoDDevice::clearAuxiliary(int auxId)
{
    if (m_device) {
        m_device->clearAuxiliary(static_cast<std::uint8_t>(auxId));
    }
}

void QPelcoDDevice::setZoneStart(int zoneId)
{
    if (m_device) {
        m_device->setZoneStart(static_cast<std::uint8_t>(zoneId));
    }
}

void QPelcoDDevice::setZoneEnd(int zoneId)
{
    if (m_device) {
        m_device->setZoneEnd(static_cast<std::uint8_t>(zoneId));
    }
}

void QPelcoDDevice::setZoneScan(bool enable)
{
    if (m_device) {
        m_device->setZoneScan(enable);
    }
}

void QPelcoDDevice::recordPatternStart(int patternId)
{
    if (m_device) {
        m_device->recordPatternStart(static_cast<std::uint8_t>(patternId));
    }
}

void QPelcoDDevice::recordPatternStop()
{
    if (m_device) {
        m_device->recordPatternStop();
    }
}

void QPelcoDDevice::runPattern(int patternId)
{
    if (m_device) {
        m_device->runPattern(static_cast<std::uint8_t>(patternId));
    }
}

void QPelcoDDevice::setZoomSpeed(int speed)
{
    if (m_device) {
        m_device->setZoomSpeed(static_cast<std::uint8_t>(speed));
    }
}

void QPelcoDDevice::setFocusSpeed(int speed)
{
    if (m_device) {
        m_device->setFocusSpeed(static_cast<std::uint8_t>(speed));
    }
}

void QPelcoDDevice::setAutoFocus(int mode)
{
    if (m_device) {
        m_device->setAutoFocus(static_cast<PelcoD::AutoMode>(mode));
    }
}

void QPelcoDDevice::setAutoIris(int mode)
{
    if (m_device) {
        m_device->setAutoIris(static_cast<PelcoD::AutoMode>(mode));
    }
}

void QPelcoDDevice::setAgc(int mode)
{
    if (m_device) {
        m_device->setAgc(static_cast<PelcoD::AutoMode>(mode));
    }
}

void QPelcoDDevice::setBacklightComp(bool enable)
{
    if (m_device) {
        m_device->setBacklightComp(enable ? PelcoD::SwitchState::On : PelcoD::SwitchState::Off);
    }
}

void QPelcoDDevice::setAutoWhiteBalance(bool enable)
{
    if (m_device) {
        m_device->setAutoWhiteBalance(enable ? PelcoD::SwitchState::On : PelcoD::SwitchState::Off);
    }
}

void QPelcoDDevice::setShutterSpeed(int speed)
{
    if (m_device) {
        m_device->setShutterSpeed(static_cast<std::uint16_t>(speed));
    }
}

void QPelcoDDevice::setGain(int gain)
{
    if (m_device) {
        m_device->setGain(static_cast<std::uint16_t>(gain));
    }
}

void QPelcoDDevice::setAutoIrisLevel(int level)
{
    if (m_device) {
        m_device->setAutoIrisLevel(static_cast<std::uint8_t>(level));
    }
}

void QPelcoDDevice::setAutoIrisPeak(int peak)
{
    if (m_device) {
        m_device->setAutoIrisPeak(static_cast<std::uint8_t>(peak));
    }
}

void QPelcoDDevice::setPhaseDelayMode(int state)
{
    if (m_device) {
        m_device->setPhaseDelayMode(static_cast<PelcoD::SwitchState>(state));
    }
}

void QPelcoDDevice::adjustWhiteBalanceRB(int value)
{
    if (m_device) {
        m_device->adjustWhiteBalanceRB(static_cast<std::uint16_t>(value));
    }
}

void QPelcoDDevice::adjustWhiteBalanceMG(int value)
{
    if (m_device) {
        m_device->adjustWhiteBalanceMG(static_cast<std::uint16_t>(value));
    }
}

void QPelcoDDevice::setMagnification(int value, bool relative)
{
    if (m_device) {
        m_device->setMagnification(static_cast<std::uint16_t>(value), relative);
    }
}

void QPelcoDDevice::setBaudRate(int baud)
{
    if (m_device) {
        m_device->setBaudRate(static_cast<std::uint32_t>(baud));
    }
}

void QPelcoDDevice::setZeroPosition()
{
    if (m_device) {
        m_device->setZeroPosition();
    }
}

void QPelcoDDevice::resetDefaults()
{
    if (m_device) {
        m_device->resetDefaults();
    }
}

void QPelcoDDevice::remoteReset()
{
    if (m_device) {
        m_device->remoteReset();
    }
}

void QPelcoDDevice::queryPan()
{
    if (m_device) {
        m_device->queryPan();
    }
}

void QPelcoDDevice::queryTilt()
{
    if (m_device) {
        m_device->queryTilt();
    }
}

void QPelcoDDevice::queryZoom()
{
    if (m_device) {
        m_device->queryZoom();
    }
}

void QPelcoDDevice::queryMagnification()
{
    if (m_device) {
        m_device->queryMagnification();
    }
}

void QPelcoDDevice::queryDeviceType()
{
    if (m_device) {
        m_device->queryDeviceType();
    }
}

void QPelcoDDevice::queryGeneral()
{
    if (m_device) {
        m_device->queryGeneral();
    }
}

void QPelcoDDevice::queryDiagnostics()
{
    if (m_device) {
        m_device->queryDiagnostics();
    }
}

void QPelcoDDevice::queryAll()
{
    if (m_device) {
        m_device->queryAll();
    }
}

void QPelcoDDevice::sendRawHex(const QByteArray& hexData)
{
    if (m_device) {
        std::vector<std::uint8_t> frame(hexData.begin(), hexData.end());
        m_device->sendRawFrame(frame);
    }
}

void QPelcoDDevice::sendRawHexPacket(const QString& hex)
{
    const QByteArray bytes = QByteArray::fromHex(hex.toUtf8());
    sendRawHex(bytes);
}

QString QPelcoDDevice::describePacket(bool isTx, const std::vector<std::uint8_t>& frame)
{
    return QString::fromStdString(PelcoD::ProtocolParser::describeFrame(isTx, frame));
}


} // namespace PelcoDQt
