/// @file QSightlineDevice.cpp
/// @brief Implementation of Qt wrapper adapter for Sightline SLA protocols.

#include "QSightlineDevice.h"

#include <QMetaObject>

QSightlineDevice::QSightlineDevice(std::shared_ptr<Transport::ITransport> transport, QObject* parent)
    : QObject(parent)
    , m_device(std::make_unique<Sightline::SightlineDevice>(std::move(transport)))
{
    wireCallbacks();
}

QSightlineDevice::QSightlineDevice(std::unique_ptr<Sightline::SightlineDevice> device, QObject* parent)
    : QObject(parent)
    , m_device(std::move(device))
{
    wireCallbacks();
}

QSightlineDevice::~QSightlineDevice()
{
    stop();
}

bool QSightlineDevice::isConnected() const noexcept
{
    return m_device && m_device->isConnected();
}

Sightline::SightlineDevice* QSightlineDevice::device() const noexcept
{
    return m_device.get();
}

std::optional<Sightline::MsgTrackingPositions> QSightlineDevice::lastTrackingPositions() const
{
    if (m_device) {
        return m_device->lastTrackingPositions();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgVersionNumber> QSightlineDevice::lastVersion() const
{
    if (m_device) {
        return m_device->lastVersion();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgSystemStatusMessage> QSightlineDevice::lastSystemStatus() const
{
    if (m_device) {
        return m_device->lastSystemStatus();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgSetStabilizationParameters> QSightlineDevice::lastStabilization() const
{
    if (m_device) {
        return m_device->lastStabilization();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgSetRegistrationParameters> QSightlineDevice::lastRegistration() const
{
    if (m_device) {
        return m_device->lastRegistration();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgSetStabilizationBias> QSightlineDevice::lastStabilizationBias() const
{
    if (m_device) {
        return m_device->lastStabilizationBias();
    }
    return std::nullopt;
}

Transport::TransportStatsSnapshot QSightlineDevice::getTransportStats() const
{
    if (m_device) {
        return m_device->getTransportStats();
    }
    return {};
}

void QSightlineDevice::wireCallbacks()
{
    if (!m_device) {
        return;
    }

    m_device->setTrackingCallback([this](const Sightline::MsgTrackingPositions& pos) {
        QMetaObject::invokeMethod(
            this, [this, pos]() { emit trackingPositionsReceived(pos); }, Qt::QueuedConnection);
    });

    m_device->setExtendedPositionsCallback([this](const Sightline::MsgTrackingPositionsExtended& ext) {
        QMetaObject::invokeMethod(
            this, [this, ext]() { emit extendedPositionsReceived(ext); }, Qt::QueuedConnection);
    });

    m_device->setWarningCallback([this](const Sightline::MsgUserWarningMessage& warn) {
        QMetaObject::invokeMethod(
            this, [this, warn]() { emit userWarningReceived(warn); }, Qt::QueuedConnection);
    });

    m_device->setVersionCallback([this](const Sightline::MsgVersionNumber& ver) {
        QMetaObject::invokeMethod(
            this, [this, ver]() { emit versionReceived(ver); }, Qt::QueuedConnection);
    });

    m_device->setSystemStatusCallback([this](const Sightline::MsgSystemStatusMessage& stat) {
        QMetaObject::invokeMethod(
            this, [this, stat]() { emit systemStatusReceived(stat); }, Qt::QueuedConnection);
    });

    m_device->setStabilizationCallback([this](const Sightline::MsgSetStabilizationParameters& params) {
        QMetaObject::invokeMethod(
            this, [this, params]() { emit stabilizationReceived(params); }, Qt::QueuedConnection);
    });

    m_device->setRegistrationCallback([this](const Sightline::MsgSetRegistrationParameters& params) {
        QMetaObject::invokeMethod(
            this, [this, params]() { emit registrationReceived(params); }, Qt::QueuedConnection);
    });

    m_device->setStabilizationBiasCallback([this](const Sightline::MsgSetStabilizationBias& bias) {
        QMetaObject::invokeMethod(
            this, [this, bias]() { emit stabilizationBiasReceived(bias); }, Qt::QueuedConnection);
    });

    m_device->setRawTrafficCallback([this](bool isTx, const std::vector<std::uint8_t>& frame) {
        const QByteArray bytes(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
        QMetaObject::invokeMethod(
            this, [this, isTx, bytes]() { emit rawFrameReceived(isTx, bytes); }, Qt::QueuedConnection);
    });
}

bool QSightlineDevice::start()
{
    if (!m_device) {
        return false;
    }
    const bool ok = m_device->start();
    emit connectionStateChanged(ok);
    return ok;
}

void QSightlineDevice::stop()
{
    if (m_device) {
        m_device->stop();
    }
    emit connectionStateChanged(false);
}

bool QSightlineDevice::startTracking(
    quint8 cameraIndex, quint16 col, quint16 row, quint16 width, quint16 height, quint8 flags)
{
    if (!m_device) {
        return false;
    }
    return m_device->startTracking(cameraIndex, col, row, width, height, flags);
}

bool QSightlineDevice::stopTracking(quint8 cameraIndex, quint8 trackId)
{
    if (!m_device) {
        return false;
    }
    return m_device->stopTracking(cameraIndex, trackId);
}

bool QSightlineDevice::modifyTracking(quint8 cameraIndex, quint8 trackId, quint8 mode, quint8 flags)
{
    if (!m_device) {
        return false;
    }
    return m_device->modifyTracking(cameraIndex, trackId, mode, flags);
}

bool QSightlineDevice::nudgeTracking(quint8 cameraIndex, qint16 deltaCol, qint16 deltaRow)
{
    if (!m_device) {
        return false;
    }
    return m_device->nudgeTracking(cameraIndex, deltaCol, deltaRow);
}

bool QSightlineDevice::designatePrimary(quint8 cameraIndex, quint8 trackId)
{
    if (!m_device) {
        return false;
    }
    return m_device->designatePrimary(cameraIndex, trackId);
}

bool QSightlineDevice::setStabilization(quint8 cameraIndex, quint8 mode, quint8 rate, quint8 maxShift)
{
    if (!m_device) {
        return false;
    }
    return m_device->setStabilization(cameraIndex, mode, rate, maxShift);
}

bool QSightlineDevice::setStabilization(const Sightline::MsgSetStabilizationParameters& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setStabilization(msg);
}

bool QSightlineDevice::resetStabilization(quint8 cameraIndex, quint8 resetType)
{
    if (!m_device) {
        return false;
    }
    return m_device->resetStabilization(cameraIndex, resetType);
}

bool QSightlineDevice::setStabilizationBias(const Sightline::MsgSetStabilizationBias& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setStabilizationBias(msg);
}

bool QSightlineDevice::setStabilizationBias(
    quint8 cameraIndex, qint16 biasCol, qint16 biasRow, quint8 autoBias, quint8 updateRate)
{
    if (!m_device) {
        return false;
    }
    return m_device->setStabilizationBias(cameraIndex, biasCol, biasRow, autoBias, updateRate);
}

bool QSightlineDevice::setRegistration(const Sightline::MsgSetRegistrationParameters& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setRegistration(msg);
}

bool QSightlineDevice::getStabilization(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->getStabilization(cameraIndex);
}

bool QSightlineDevice::getRegistration(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->getRegistration(cameraIndex);
}

bool QSightlineDevice::getStabilizationBias(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->getStabilizationBias(cameraIndex);
}

bool QSightlineDevice::sendLensCommand(quint8 cameraIndex, quint8 commandType, qint16 rateOrPosition)
{
    if (!m_device) {
        return false;
    }
    return m_device->sendLensCommand(cameraIndex, commandType, rateOrPosition);
}

bool QSightlineDevice::saveParameters(quint8 commitType)
{
    if (!m_device) {
        return false;
    }
    return m_device->saveParameters(commitType);
}

bool QSightlineDevice::resetParameters(quint8 resetType)
{
    if (!m_device) {
        return false;
    }
    return m_device->resetParameters(resetType);
}

bool QSightlineDevice::queryVersion()
{
    if (!m_device) {
        return false;
    }
    return m_device->queryVersion();
}

bool QSightlineDevice::enableSystemStatus(bool enable)
{
    if (!m_device) {
        return false;
    }
    return m_device->enableSystemStatus(enable);
}

bool QSightlineDevice::queryParameters(quint8 queryId)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryParameters(queryId);
}

bool QSightlineDevice::sendRawPacket(const QByteArray& rawPacket)
{
    if (!m_device || !m_device->transport() || !m_device->transport()->isOpen()) {
        return false;
    }
    const std::vector<std::uint8_t> data(reinterpret_cast<const std::uint8_t*>(rawPacket.constData()),
        reinterpret_cast<const std::uint8_t*>(rawPacket.constData()) + rawPacket.size());
    return m_device->transport()->sendData(data);
}
