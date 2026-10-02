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

std::optional<Sightline::MsgSetOverlayMode> QSightlineDevice::lastOverlayMode() const
{
    if (m_device) {
        return m_device->lastOverlayMode();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgCurrentOverlayObjectsIds> QSightlineDevice::lastOverlayObjectsIds() const
{
    if (m_device) {
        return m_device->lastOverlayObjectsIds();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgLogoParameters> QSightlineDevice::lastLogoParameters() const
{
    if (m_device) {
        return m_device->lastLogoParameters();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgSetDetectionParameters> QSightlineDevice::lastDetectionParams() const
{
    if (m_device) {
        return m_device->lastDetectionParams();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgAdvancedDetectionParameters> QSightlineDevice::lastAdvDetection() const
{
    if (m_device) {
        return m_device->lastAdvDetection();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgDetectionROI> QSightlineDevice::lastDetectionROI() const
{
    if (m_device) {
        return m_device->lastDetectionROI();
    }
    return std::nullopt;
}

std::optional<Sightline::MsgKlvMetricFilters> QSightlineDevice::lastKlvMetricFilters() const
{
    if (m_device) {
        return m_device->lastKlvMetricFilters();
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
            this,
            [this, pos]() {
                processTrackTelemetry(pos.cameraIndex, pos.tracks);
                emit trackingPositionsReceived(pos);
            },
            Qt::QueuedConnection);
    });

    m_device->setExtendedPositionsCallback([this](const Sightline::MsgTrackingPositionsExtended& ext) {
        QMetaObject::invokeMethod(
            this,
            [this, ext]() {
                processTrackTelemetry(ext.cameraIndex, ext.tracks);
                emit extendedPositionsReceived(ext);
            },
            Qt::QueuedConnection);
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

    m_device->setOverlayCallback([this](const Sightline::MsgSetOverlayMode& mode) {
        QMetaObject::invokeMethod(
            this, [this, mode]() { emit overlayModeReceived(mode); }, Qt::QueuedConnection);
    });

    m_device->setObjectsIdsCallback([this](const Sightline::MsgCurrentOverlayObjectsIds& ids) {
        QMetaObject::invokeMethod(
            this, [this, ids]() { emit overlayObjectsIdsReceived(ids); }, Qt::QueuedConnection);
    });

    m_device->setObjectParamsCallback([this](const Sightline::MsgCurrentOverlayObjectParameters& params) {
        QMetaObject::invokeMethod(
            this, [this, params]() { emit overlayObjectParamsReceived(params); }, Qt::QueuedConnection);
    });

    m_device->setLogoCallback([this](const Sightline::MsgLogoParameters& logo) {
        QMetaObject::invokeMethod(
            this, [this, logo]() { emit logoParametersReceived(logo); }, Qt::QueuedConnection);
    });

    m_device->setDetectionCallback([this](const Sightline::MsgSetDetectionParameters& det) {
        QMetaObject::invokeMethod(
            this, [this, det]() { emit detectionReceived(det); }, Qt::QueuedConnection);
    });

    m_device->setAdvDetectionCallback([this](const Sightline::MsgAdvancedDetectionParameters& adv) {
        QMetaObject::invokeMethod(
            this, [this, adv]() { emit advDetectionReceived(adv); }, Qt::QueuedConnection);
    });

    m_device->setDetectionRoiCallback([this](const Sightline::MsgDetectionROI& roi) {
        QMetaObject::invokeMethod(
            this, [this, roi]() { emit detectionRoiReceived(roi); }, Qt::QueuedConnection);
    });

    m_device->setKlvMetricFiltersCb([this](const Sightline::MsgKlvMetricFilters& filters) {
        QMetaObject::invokeMethod(
            this, [this, filters]() { emit klvMetricFiltersReceived(filters); }, Qt::QueuedConnection);
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
    m_knownCoastingState.clear();
    m_knownPrimaryTrack.clear();
    emit connectionStateChanged(false);
}

void QSightlineDevice::processTrackTelemetry(
    quint8 cameraIndex, const std::vector<Sightline::TrackCoordinate>& tracks)
{
    for (const auto& trk : tracks) {
        const auto key = std::make_pair(cameraIndex, trk.trackId);
        const auto it = m_knownCoastingState.find(key);
        if (it == m_knownCoastingState.end() || it->second != trk.isCoasting) {
            m_knownCoastingState[key] = trk.isCoasting;
            emit trackCoastingChanged(cameraIndex, trk.trackId, trk.isCoasting);
        }

        if (trk.isPrimary) {
            emit primaryTrackUpdated(cameraIndex, trk);
            const auto primIt = m_knownPrimaryTrack.find(cameraIndex);
            if (primIt == m_knownPrimaryTrack.end() || primIt->second != trk.trackId) {
                m_knownPrimaryTrack[cameraIndex] = trk.trackId;
                emit primaryTrackChanged(cameraIndex, trk.trackId);
            }
        }
    }
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

bool QSightlineDevice::startPrecisionTrack(
    quint8 cameraIndex, quint16 col, quint16 row, quint16 width, quint16 height, quint64 framePts)
{
    if (!m_device) {
        return false;
    }
    return m_device->startPrecisionTrack(cameraIndex, col, row, width, height, framePts);
}

bool QSightlineDevice::setForcedCoast(quint8 cameraIndex, quint8 trackId, Sightline::ForcedCoastingMode mode)
{
    if (!m_device) {
        return false;
    }
    return m_device->setForcedCoast(cameraIndex, trackId, mode);
}

bool QSightlineDevice::reinitTrack(quint8 cameraIndex, quint8 trackId)
{
    if (!m_device) {
        return false;
    }
    return m_device->reinitTrack(cameraIndex, trackId);
}

bool QSightlineDevice::resizeTrack(
    quint8 cameraIndex, quint8 trackId, quint16 width, quint16 height, bool assist)
{
    if (!m_device) {
        return false;
    }
    return m_device->resizeTrack(cameraIndex, trackId, width, height, assist);
}

bool QSightlineDevice::cueTrackAt(
    quint8 cameraIndex, quint16 col, quint16 row, Sightline::ModifyMode mode, quint8 trackId)
{
    if (!m_device) {
        return false;
    }
    return m_device->cueTrackAt(cameraIndex, col, row, mode, trackId);
}

bool QSightlineDevice::nudgeDisplayTrack(quint8 cameraIndex, qint16 deltaCol, qint16 deltaRow)
{
    if (!m_device) {
        return false;
    }
    return m_device->nudgeDisplayTrack(cameraIndex, deltaCol, deltaRow);
}

bool QSightlineDevice::setDetection(const Sightline::MsgSetDetectionParameters& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setDetection(msg);
}

bool QSightlineDevice::setAdvancedDetection(const Sightline::MsgAdvancedDetectionParameters& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setAdvancedDetection(msg);
}

bool QSightlineDevice::setDetectionROI(const Sightline::MsgDetectionROI& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setDetectionROI(msg);
}

bool QSightlineDevice::setVMTI(const Sightline::MsgSetVMTI& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setVMTI(msg);
}

bool QSightlineDevice::triggerDetectionSnapshot(quint8 cameraIndex, quint8 detectionIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->triggerDetectionSnapshot(cameraIndex, detectionIndex);
}

bool QSightlineDevice::setKlvMetricFilters(const Sightline::MsgKlvMetricFilters& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setKlvMetricFilters(msg);
}

bool QSightlineDevice::setClassifierConfig(const Sightline::MsgClassifierConfig& msg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setClassifierConfig(msg);
}

bool QSightlineDevice::setComputeResources(bool useNpu, bool asyncInferencing)
{
    if (!m_device) {
        return false;
    }
    return m_device->setComputeResources(useNpu, asyncInferencing);
}

bool QSightlineDevice::queryDetectionParams(quint8 cameraIndex, quint8 detIdx)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryDetectionParams(cameraIndex, detIdx);
}

bool QSightlineDevice::queryAdvDetection(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryAdvDetection(cameraIndex);
}

bool QSightlineDevice::queryDetectionROI(quint8 cameraIndex, quint8 roiIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryDetectionROI(cameraIndex, roiIndex);
}

bool QSightlineDevice::queryVMTI(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryVMTI(cameraIndex);
}

bool QSightlineDevice::queryTrackingPixelStats(quint8 cameraIndex, quint8 trackId)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryTrackingPixelStats(cameraIndex, trackId);
}

bool QSightlineDevice::queryKlvMetricFilters(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryKlvMetricFilters(cameraIndex);
}

bool QSightlineDevice::queryClassifierConfig(quint8 cameraIndex)
{
    if (!m_device) {
        return false;
    }
    return m_device->queryClassifierConfig(cameraIndex);
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

bool QSightlineDevice::setOverlayMode(const Sightline::MsgSetOverlayMode& msg)
{
    return m_device ? m_device->setOverlayMode(msg) : false;
}

bool QSightlineDevice::getOverlayMode(quint8 cameraIndex)
{
    return m_device ? m_device->getOverlayMode(cameraIndex) : false;
}

bool QSightlineDevice::drawOverlay(const Sightline::MsgDrawOverlay& msg)
{
    return m_device ? m_device->drawOverlay(msg) : false;
}

bool QSightlineDevice::drawOverlayBatch(const std::vector<Sightline::MsgDrawOverlay>& objects)
{
    return m_device ? m_device->drawOverlayBatch(objects) : false;
}

bool QSightlineDevice::drawCross(quint8 cameraIndex, quint8 objectId, qint16 centerX, qint16 centerY, quint16 size,
    Sightline::OverlayPaletteColor fgColor, quint16 thickness, bool originUpperLeft)
{
    return m_device
        ? m_device->drawCross(cameraIndex, objectId, centerX, centerY, size, fgColor, thickness, originUpperLeft)
        : false;
}

bool QSightlineDevice::drawRectangle(quint8 cameraIndex, quint8 objectId, qint16 x, qint16 y, quint16 width,
    quint16 height, bool filled, Sightline::OverlayPaletteColor fgColor, Sightline::OverlayPaletteColor bgColor,
    quint8 alpha, quint16 thickness, bool originUpperLeft)
{
    return m_device ? m_device->drawRectangle(
               cameraIndex, objectId, x, y, width, height, filled, fgColor, bgColor, alpha, thickness, originUpperLeft)
                    : false;
}

bool QSightlineDevice::drawText(quint8 cameraIndex, quint8 objectId, qint16 x, qint16 y, const QString& text,
    Sightline::OverlayFontId fontId, Sightline::OverlayPaletteColor fgColor, Sightline::OverlayPaletteColor bgColor,
    quint8 hScale, quint8 vScale, bool originUpperLeft)
{
    return m_device ? m_device->drawText(cameraIndex, objectId, x, y, text.toStdString(), fontId, fgColor, bgColor,
               hScale, vScale, originUpperLeft)
                    : false;
}

bool QSightlineDevice::drawKlvField(quint8 cameraIndex, quint8 objectId, qint16 x, qint16 y,
    Sightline::KlvFieldTag fieldTag, Sightline::KlvFormatType formatType, const QString& formatString,
    Sightline::OverlayFontId fontId, Sightline::OverlayPaletteColor fgColor, bool originUpperLeft)
{
    return m_device ? m_device->drawKlvField(cameraIndex, objectId, x, y, fieldTag, formatType,
               formatString.toStdString(), fontId, fgColor, originUpperLeft)
                    : false;
}

bool QSightlineDevice::drawBlackout(quint8 cameraIndex, quint8 objectId, quint16 width, quint16 height)
{
    return m_device ? m_device->drawBlackout(cameraIndex, objectId, width, height) : false;
}

bool QSightlineDevice::destroyOverlay(quint8 cameraIndex, quint8 objectId)
{
    return m_device ? m_device->destroyOverlay(cameraIndex, objectId) : false;
}

bool QSightlineDevice::destroyAllOverlays(quint8 cameraIndex)
{
    return m_device ? m_device->destroyAllOverlays(cameraIndex) : false;
}

bool QSightlineDevice::setLogoParameters(const Sightline::MsgLogoParameters& msg)
{
    return m_device ? m_device->setLogoParameters(msg) : false;
}

bool QSightlineDevice::getLogoParameters(quint8 cameraIndex)
{
    return m_device ? m_device->getLogoParameters(cameraIndex) : false;
}

bool QSightlineDevice::setUserFont(quint8 slotIndex, const QString& fontFileName)
{
    return m_device ? m_device->setUserFont(slotIndex, fontFileName.toStdString()) : false;
}

bool QSightlineDevice::getOverlayObjectsIds(quint8 cameraIndex)
{
    return m_device ? m_device->getOverlayObjectsIds(cameraIndex) : false;
}

bool QSightlineDevice::getOverlayObjectParams(quint8 objectId)
{
    return m_device ? m_device->getOverlayObjectParams(objectId) : false;
}
