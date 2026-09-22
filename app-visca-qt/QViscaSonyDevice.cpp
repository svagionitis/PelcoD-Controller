/// @file QViscaSonyDevice.cpp
/// @brief Implementation of Qt adapter for Sony FCB cameras.

#include "QViscaSonyDevice.h"
#include <SonyViscaBuilder.h>
#include <ViscaBuilder.h>
#include <ViscaParser.h>

#include <QDateTime>
#include <QMetaObject>
#include <QThreadPool>
#include <glog/logging.h>

namespace ViscaApp {

namespace {

    QString describeViscaFrame(const Visca::ViscaFrame& frame, bool isTx)
    {
        if (frame.empty()) {
            return QStringLiteral("Empty frame");
        }

        const auto len = frame.size();
        const auto* bytes = frame.data();

        if (!isTx) {
            // RX Response packet
            if (len >= 3 && bytes[1] == 0x38) {
                return QStringLiteral("Network Change Notification");
            }
            if (len >= 3 && (bytes[1] & 0xF0) == 0x40) {
                const int socket = bytes[1] & 0x0F;
                return QStringLiteral("ACK (Socket %1)").arg(socket);
            }
            if (len >= 3 && (bytes[1] & 0xF0) == 0x50) {
                const int socket = bytes[1] & 0x0F;
                if (len > 3) {
                    return QStringLiteral("Inquiry Completion (Socket %1, %2 bytes)").arg(socket).arg(len);
                }
                return QStringLiteral("Command Completion (Socket %1)").arg(socket);
            }
            if (len >= 4 && (bytes[1] & 0xF0) == 0x60) {
                const int socket = bytes[1] & 0x0F;
                const auto err = static_cast<Visca::ViscaErrorCode>(bytes[2]);
                return QStringLiteral("Error (Socket %1): %2")
                    .arg(socket)
                    .arg(QString::fromUtf8(Visca::errorCodeToString(err).data()));
            }
            if (len >= 4 && bytes[0] == 0x88 && bytes[1] == 0x30) {
                return QStringLiteral("Address Set Response: %1 cameras").arg(bytes[2] - 1);
            }
            return QStringLiteral("RX Response (%1 bytes)").arg(len);
        }

        // TX Command packet
        if (len >= 4 && bytes[0] == 0x88 && bytes[1] == 0x30 && bytes[2] == 0x01) {
            return QStringLiteral("Address Set Broadcast (88 30 01 FF)");
        }
        if (len >= 5 && bytes[1] == 0x01 && bytes[2] == 0x00 && bytes[3] == 0x01) {
            return QStringLiteral("IF_Clear");
        }
        if (len >= 3 && (bytes[1] & 0xF0) == 0x20) {
            const int socket = bytes[1] & 0x0F;
            return QStringLiteral("Cancel Socket %1").arg(socket);
        }
        if (len >= 5 && bytes[1] == 0x09) {
            if (bytes[2] == 0x00 && bytes[3] == 0x02) {
                return QStringLiteral("Inquiry: CAM_VersionInq");
            }
            if (bytes[2] == 0x7E && bytes[3] == 0x7E) {
                const int sub = (len > 4) ? bytes[4] : -1;
                return QStringLiteral("Block Inquiry 0%1").arg(sub);
            }
            return QStringLiteral("Inquiry (%1 bytes)").arg(len);
        }
        if (len >= 6 && bytes[1] == 0x01 && bytes[2] == 0x04) {
            if (bytes[3] == 0x00) {
                return (bytes[4] == 0x02) ? QStringLiteral("Power ON") : QStringLiteral("Power OFF/Standby");
            }
            if (bytes[3] == 0x07) {
                return QStringLiteral("Zoom Direct/Speed");
            }
            if (bytes[3] == 0x08) {
                return QStringLiteral("Focus Control");
            }
            if (bytes[3] == 0x39) {
                return QStringLiteral("Exposure Mode");
            }
            if (bytes[3] == 0x35) {
                return QStringLiteral("White Balance Mode");
            }
        }

        return QStringLiteral("Command (%1 bytes)").arg(len);
    }

} // namespace

QViscaSonyDevice::QViscaSonyDevice(
    std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress, QObject* parent)
    : QObject(parent)
    , m_transport(std::move(transport))
    , m_cameraAddress(cameraAddress)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &QViscaSonyDevice::pollStatus);
}

QViscaSonyDevice::~QViscaSonyDevice()
{
    disconnectDevice();
}

void QViscaSonyDevice::setTransport(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress)
{
    const bool wasConnected = m_connected;
    if (wasConnected) {
        disconnectDevice();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_transport = std::move(transport);
    m_cameraAddress = cameraAddress;
}

std::shared_ptr<::Transport::ITransport> QViscaSonyDevice::transport() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transport;
}

uint8_t QViscaSonyDevice::cameraAddress() const noexcept
{
    return m_cameraAddress;
}

void QViscaSonyDevice::setCameraAddress(uint8_t address)
{
    m_cameraAddress = address;
    if (m_fcbDevice) {
        m_fcbDevice->device().setCameraAddress(address);
    }
}

bool QViscaSonyDevice::isConnected() const noexcept
{
    return m_connected;
}

Visca::Sony::SonyCameraModelType QViscaSonyDevice::modelType() const noexcept
{
    if (m_fcbDevice) {
        return m_fcbDevice->modelType();
    }
    return Visca::Sony::SonyCameraModelType::Unknown;
}

Visca::Sony::CameraCapabilities QViscaSonyDevice::capabilities() const noexcept
{
    if (m_fcbDevice) {
        return m_fcbDevice->capabilities();
    }
    return {};
}

Visca::Sony::SonyFCBStatus QViscaSonyDevice::currentStatus() const noexcept
{
    if (m_fcbDevice) {
        return m_fcbDevice->status();
    }
    return {};
}

void QViscaSonyDevice::setPollingInterval(int intervalMs)
{
    if (m_pollTimer) {
        m_pollTimer->setInterval(intervalMs > 100 ? intervalMs : 100);
    }
}

void QViscaSonyDevice::setupTrafficHook()
{
    if (!m_fcbDevice) {
        return;
    }

    m_fcbDevice->device().setTrafficCallback([this](const Visca::ViscaFrame& frame, bool outgoing) {
        const QByteArray packet(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
        const QString desc = describeViscaFrame(frame, outgoing);

        QMetaObject::invokeMethod(
            this, [this, outgoing, packet, desc]() { emit trafficLogged(outgoing, packet, desc); },
            Qt::QueuedConnection);
    });
}

bool QViscaSonyDevice::connectDevice()
{
    std::shared_ptr<::Transport::ITransport> trans;
    uint8_t addr = 1;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        trans = m_transport;
        addr = m_cameraAddress;
    }

    if (!trans) {
        emit commandFailed(tr("No transport configured for device connection."));
        return false;
    }

    if (!trans->isOpen()) {
        if (!trans->open()) {
            emit commandFailed(tr("Failed to open transport interface."));
            return false;
        }
    }

    auto fcb = std::make_shared<Visca::Sony::SonyFCBDevice>(trans, addr);
    m_fcbDevice = fcb;
    setupTrafficHook();

    const bool identified = m_fcbDevice->initialize();
    m_connected = true;

    if (identified) {
        emit modelDiscovered(m_fcbDevice->modelType(), m_fcbDevice->capabilities());
    } else {
        LOG(WARNING) << "Sony camera at address " << static_cast<int>(addr)
                     << " connected but did not answer Version Inquiry";
    }

    emit connectionStateChanged(true);

    if (m_pollTimer && !m_pollTimer->isActive()) {
        m_pollTimer->start();
    }

    pollStatus();
    return true;
}

void QViscaSonyDevice::connectDeviceAsync()
{
    emit connectingStateChanged(true);

    QThreadPool::globalInstance()->start([this]() {
        const bool success = connectDevice();
        QMetaObject::invokeMethod(
            this,
            [this, success]() {
                emit connectingStateChanged(false);
                if (!success) {
                    emit connectionStateChanged(false);
                }
            },
            Qt::QueuedConnection);
    });
}

void QViscaSonyDevice::disconnectDevice()
{
    if (m_pollTimer && m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }

    std::shared_ptr<::Transport::ITransport> trans;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        trans = m_transport;
        m_fcbDevice.reset();
        m_connected = false;
    }

    if (trans && trans->isOpen()) {
        trans->close();
    }

    emit connectionStateChanged(false);
}

void QViscaSonyDevice::pollStatus()
{
    if (!m_connected || !m_fcbDevice) {
        return;
    }

    m_fcbDevice->pollStatus();
    emit statusUpdated(m_fcbDevice->status());
}

// Lens & Optics
void QViscaSonyDevice::setZoomDirect(uint16_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setZoomDirect(position);
    }
}

void QViscaSonyDevice::zoomTele(uint8_t speed)
{
    if (m_fcbDevice) {
        m_fcbDevice->zoomTele(speed);
    }
}

void QViscaSonyDevice::zoomWide(uint8_t speed)
{
    if (m_fcbDevice) {
        m_fcbDevice->zoomWide(speed);
    }
}

void QViscaSonyDevice::zoomStop()
{
    if (m_fcbDevice) {
        m_fcbDevice->zoomStop();
    }
}

void QViscaSonyDevice::setFocusAuto(bool autoMode)
{
    if (m_fcbDevice) {
        m_fcbDevice->setFocusAuto(autoMode);
    }
}

void QViscaSonyDevice::setFocusDirect(uint16_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setFocusDirect(position);
    }
}

void QViscaSonyDevice::focusOnePush()
{
    if (m_fcbDevice) {
        m_fcbDevice->focusOnePush();
    }
}

void QViscaSonyDevice::setFocusNearLimit(uint16_t limit)
{
    if (m_fcbDevice) {
        m_fcbDevice->setFocusNearLimit(limit);
    }
}

// Exposure
void QViscaSonyDevice::setExposureMode(Visca::Sony::SonyExposureMode mode)
{
    if (m_fcbDevice) {
        m_fcbDevice->setExposureMode(mode);
    }
}

void QViscaSonyDevice::setShutter(uint8_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setShutter(position);
    }
}

void QViscaSonyDevice::setIris(uint8_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setIris(position);
    }
}

void QViscaSonyDevice::setGain(uint8_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setGain(position);
    }
}

void QViscaSonyDevice::setExposureCompensation(bool on, uint8_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setExposureCompensation(on, position);
    }
}

// White Balance
void QViscaSonyDevice::setWhiteBalance(Visca::Sony::SonyWhiteBalanceMode mode)
{
    if (m_fcbDevice) {
        m_fcbDevice->setWhiteBalance(mode);
    }
}

void QViscaSonyDevice::setRgainDirect(uint8_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setRgaiDirect(position);
    }
}

void QViscaSonyDevice::setBgainDirect(uint8_t position)
{
    if (m_fcbDevice) {
        m_fcbDevice->setBgainDirect(position);
    }
}

void QViscaSonyDevice::triggerOnePushWb()
{
    if (m_fcbDevice) {
        m_fcbDevice->triggerOnePushWb();
    }
}

// Image Enhancement
void QViscaSonyDevice::setStabilizer(Visca::Sony::SonyStabilizerMode mode)
{
    if (m_fcbDevice) {
        m_fcbDevice->setStabilizer(mode);
    }
}

void QViscaSonyDevice::setDefog(Visca::Sony::SonyDefogMode mode)
{
    if (m_fcbDevice) {
        m_fcbDevice->setDefog(mode);
    }
}

void QViscaSonyDevice::setIcr(bool on)
{
    if (m_fcbDevice) {
        m_fcbDevice->setIcr(on);
    }
}

void QViscaSonyDevice::setAutoIcr(bool on)
{
    if (m_fcbDevice) {
        m_fcbDevice->setAutoIcr(on);
    }
}

// Hardware Gated Options
void QViscaSonyDevice::setDistortionCompensation(bool on)
{
    if (m_fcbDevice) {
        if (!m_fcbDevice->setDistortionCompensation(on)) {
            emit commandFailed(tr("Distortion compensation rejected: not supported on this model."));
        }
    }
}

void QViscaSonyDevice::setOpticalAxisGapCompensation(bool on)
{
    if (m_fcbDevice) {
        if (!m_fcbDevice->setOpticalAxisGapCompensation(on)) {
            emit commandFailed(tr("Optical axis gap compensation rejected: not supported on this model."));
        }
    }
}

void QViscaSonyDevice::setOperatingMode(uint8_t mode)
{
    if (m_fcbDevice) {
        if (!m_fcbDevice->setOperatingMode(mode)) {
            emit commandFailed(tr("4K / Video operating mode rejected: mode not supported on this model."));
        }
    }
}

void QViscaSonyDevice::setLvdsMode(uint8_t mode)
{
    if (m_fcbDevice) {
        if (!m_fcbDevice->setLvdsMode(mode)) {
            emit commandFailed(tr("LVDS output mode rejected: not supported on this model."));
        }
    }
}

void QViscaSonyDevice::setDigitalOutputMode(uint8_t mode)
{
    if (m_fcbDevice) {
        if (!m_fcbDevice->setDigitalOutputMode(mode)) {
            emit commandFailed(tr("TMDS output mode rejected: not supported on this model."));
        }
    }
}

// Custom Registers & Raw Dispatch
void QViscaSonyDevice::writeRegister(uint8_t reg, uint8_t val)
{
    if (m_fcbDevice) {
        const auto frame = Visca::Sony::SonyViscaBuilder::writeRegister(m_cameraAddress, reg, val);
        m_fcbDevice->device().sendCommandAsync(frame, [this, reg](const Visca::CommandResult& res) {
            if (!res.success) {
                emit commandFailed(tr("Failed to write register 0x%1: %2")
                                       .arg(reg, 2, 16, QLatin1Char('0'))
                                       .arg(QString::fromStdString(res.errorMessage)));
            }
        });
    }
}

void QViscaSonyDevice::readRegister(uint8_t reg)
{
    if (m_fcbDevice) {
        const auto frame = Visca::Sony::SonyViscaBuilder::registerInquiry(m_cameraAddress, reg);
        m_fcbDevice->device().sendInquiryAsync(frame, [this, reg](const Visca::InquiryResult& res) {
            if (!res.success) {
                emit commandFailed(tr("Failed to read register 0x%1: %2")
                                       .arg(reg, 2, 16, QLatin1Char('0'))
                                       .arg(QString::fromStdString(res.errorMessage)));
            }
        });
    }
}

void QViscaSonyDevice::sendRawHex(const QByteArray& hexData)
{
    if (!m_fcbDevice) {
        emit commandFailed(tr("Device not connected."));
        return;
    }

    const QByteArray cleaned = hexData.simplified().replace(" ", "");
    const QByteArray binary = QByteArray::fromHex(cleaned);
    if (binary.isEmpty()) {
        emit commandFailed(tr("Invalid hex format."));
        return;
    }

    std::vector<uint8_t> bytes(binary.cbegin(), binary.cend());
    Visca::ViscaFrame frame;
    try {
        frame = Visca::ViscaFrame(std::move(bytes));
    } catch (const std::exception& e) {
        emit commandFailed(tr("Invalid VISCA frame: %1").arg(e.what()));
        return;
    }

    const bool isInquiry = (frame.size() >= 2 && frame[1] == 0x09);
    if (isInquiry) {
        m_fcbDevice->device().sendInquiryAsync(frame, [](const Visca::InquiryResult&) {});
    } else {
        m_fcbDevice->device().sendCommandAsync(frame, [](const Visca::CommandResult&) {});
    }
}

void QViscaSonyDevice::setPower(bool on)
{
    if (m_fcbDevice) {
        const auto frame = Visca::ViscaBuilder::power(m_cameraAddress, on);
        m_fcbDevice->device().sendCommandAsync(frame, [](const Visca::CommandResult&) {});
    }
}

void QViscaSonyDevice::ifClear()
{
    if (m_fcbDevice) {
        (void)m_fcbDevice->device().ifClear();
    }
}

void QViscaSonyDevice::cancelSocket(int socketIndex)
{
    if (m_fcbDevice) {
        const auto sock = (socketIndex == 2) ? Visca::ViscaSocket::Socket2 : Visca::ViscaSocket::Socket1;
        m_fcbDevice->device().cancelSocket(sock);
    }
}

} // namespace ViscaApp
