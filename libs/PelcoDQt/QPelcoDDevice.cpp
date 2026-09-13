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

// --- Forwarding Macros for Core Device Passthrough ---
#define FORWARD_CORE_0(slotName) \
    void QPelcoDDevice::slotName() { if (m_device) { m_device->slotName(); } }

#define FORWARD_CORE_1(slotName, ArgType, CastType) \
    void QPelcoDDevice::slotName(ArgType val) { if (m_device) { m_device->slotName(static_cast<CastType>(val)); } }

#define FORWARD_CORE_SWITCH(slotName) \
    void QPelcoDDevice::slotName(bool enable) { \
        if (m_device) { m_device->slotName(enable ? PelcoD::SwitchState::On : PelcoD::SwitchState::Off); } \
    }

// Configuration
FORWARD_CORE_1(setQueryTimeoutMs, int, std::uint32_t)

// Motion
FORWARD_CORE_1(panLeft, int, std::uint8_t)
FORWARD_CORE_1(panRight, int, std::uint8_t)
FORWARD_CORE_1(tiltUp, int, std::uint8_t)
FORWARD_CORE_1(tiltDown, int, std::uint8_t)
FORWARD_CORE_0(stopMotion)

void QPelcoDDevice::move(int panDir, int panSpeed, int tiltDir, int tiltSpeed)
{
    if (m_device) {
        m_device->move(static_cast<PelcoD::PanDirection>(panDir), static_cast<std::uint8_t>(panSpeed),
            static_cast<PelcoD::TiltDirection>(tiltDir), static_cast<std::uint8_t>(tiltSpeed));
    }
}

// Zoom, Focus, Iris
FORWARD_CORE_0(zoomTele)
FORWARD_CORE_0(zoomWide)
FORWARD_CORE_0(zoomStop)
FORWARD_CORE_0(focusNear)
FORWARD_CORE_0(focusFar)
FORWARD_CORE_0(focusStop)
FORWARD_CORE_0(irisOpen)
FORWARD_CORE_0(irisClose)
FORWARD_CORE_0(irisStop)

// Absolute Positioning
FORWARD_CORE_1(setPanAngle, int, std::uint16_t)
FORWARD_CORE_1(setTiltAngle, int, std::uint16_t)
FORWARD_CORE_1(setZoomPosition, int, std::uint16_t)

// Presets
FORWARD_CORE_1(setPreset, int, std::uint8_t)
FORWARD_CORE_1(clearPreset, int, std::uint8_t)
FORWARD_CORE_1(goToPreset, int, std::uint8_t)
FORWARD_CORE_0(flip180)
FORWARD_CORE_0(zeroPan)

// Aux & Zones
FORWARD_CORE_1(setAuxiliary, int, std::uint8_t)
FORWARD_CORE_1(clearAuxiliary, int, std::uint8_t)
FORWARD_CORE_1(setZoneStart, int, std::uint8_t)
FORWARD_CORE_1(setZoneEnd, int, std::uint8_t)

void QPelcoDDevice::setZoneScan(bool enable)
{
    if (m_device) {
        m_device->setZoneScan(enable);
    }
}

// Patterns
FORWARD_CORE_1(recordPatternStart, int, std::uint8_t)
FORWARD_CORE_0(recordPatternStop)
FORWARD_CORE_1(runPattern, int, std::uint8_t)

// Speeds & Options
FORWARD_CORE_1(setZoomSpeed, int, std::uint8_t)
FORWARD_CORE_1(setFocusSpeed, int, std::uint8_t)
FORWARD_CORE_1(setAutoFocus, int, PelcoD::AutoMode)
FORWARD_CORE_1(setAutoIris, int, PelcoD::AutoMode)
FORWARD_CORE_1(setAgc, int, PelcoD::AutoMode)
FORWARD_CORE_SWITCH(setBacklightComp)
FORWARD_CORE_SWITCH(setAutoWhiteBalance)
FORWARD_CORE_1(setShutterSpeed, int, std::uint16_t)
FORWARD_CORE_1(setGain, int, std::uint16_t)
FORWARD_CORE_1(setAutoIrisLevel, int, std::uint8_t)
FORWARD_CORE_1(setAutoIrisPeak, int, std::uint8_t)
FORWARD_CORE_1(setPhaseDelayMode, int, PelcoD::SwitchState)
FORWARD_CORE_1(adjustWhiteBalanceRB, int, std::uint16_t)
FORWARD_CORE_1(adjustWhiteBalanceMG, int, std::uint16_t)

void QPelcoDDevice::setMagnification(int value, bool relative)
{
    if (m_device) {
        m_device->setMagnification(static_cast<std::uint16_t>(value), relative);
    }
}

FORWARD_CORE_1(setBaudRate, int, std::uint32_t)
FORWARD_CORE_0(setZeroPosition)

// System & Queries
FORWARD_CORE_0(resetDefaults)
FORWARD_CORE_0(remoteReset)
FORWARD_CORE_0(queryPan)
FORWARD_CORE_0(queryTilt)
FORWARD_CORE_0(queryZoom)
FORWARD_CORE_0(queryMagnification)
FORWARD_CORE_0(queryDeviceType)
FORWARD_CORE_0(queryGeneral)
FORWARD_CORE_0(queryDiagnostics)
FORWARD_CORE_0(queryAll)

#undef FORWARD_CORE_0
#undef FORWARD_CORE_1
#undef FORWARD_CORE_SWITCH

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
