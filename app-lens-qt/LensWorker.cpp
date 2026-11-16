/// @file LensWorker.cpp
/// @brief Dedicated background worker managing video ingestion, optical simulation, and LensController.
#include "LensWorker.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>
#include <QVariantMap>

#include "AtomicTripleBuffer.h"
#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "IVideoDecoder.h"
#include "LensController.h"
#include "LensFramePacket.h"
#include "LensTypes.h"
#include "MockLensDriver.h"
#include "PelcoDLensDriver.h"
#include "SerialTransport.h"
#include "SyntheticDefocusGenerator.h"
#include "ViscaLensDriver.h"

namespace LensApp {

namespace {

    constexpr double kDefaultFps = 30.0;

    void transmitBridge(const std::uint8_t* data, std::size_t len, void* userCtx) noexcept
    {
        if (userCtx != nullptr && data != nullptr && len > 0U) {
            auto* transport = static_cast<Transport::SerialTransport*>(userCtx);
            if (transport->isOpen()) {
                std::vector<std::uint8_t> frame(data, data + len);
                static_cast<void>(transport->sendData(frame));
            }
        }
    }

} // namespace

LensWorker::LensWorker(Video::AtomicTripleBuffer<LensFramePacket>& tripleBuffer, QObject* parent)
    : QObject(parent)
    , m_tripleBuffer(tripleBuffer)
{
}

LensWorker::~LensWorker()
{
    stopWorker();
}

bool LensWorker::isRunning() const noexcept
{
    return m_isRunning;
}

int LensWorker::activeChannel() const noexcept
{
    return m_activeChannel;
}

void LensWorker::initWorker()
{
    initDrivers();

    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout, this, &LensWorker::processLoopTick);
    }

    // Default to synthetic optical simulation on startup
    static_cast<void>(initBackend(m_currentUri));
    play();

    emit workerInitialized();
}

void LensWorker::stopWorker()
{
    m_isRunning = false;
    m_isPaused = false;

    if (m_timer != nullptr) {
        m_timer->stop();
    }

    if (m_decoder != nullptr) {
        m_decoder->close();
        m_decoder.reset();
    }

    for (std::uint8_t i = 0U; i < 4U; ++i) {
        m_controller.setDriver(LensControl::CameraIndex(i), nullptr);
        m_driverSlots[i].serialTransport.reset();
    }
}

void LensWorker::initDrivers()
{
    for (std::uint8_t i = 0U; i < 4U; ++i) {
        onBindMock(static_cast<int>(i));
        const LensControl::CameraIndex cam(i);
        m_controller.setLensRanges(cam, LensControl::LensRanges { 0U, 65535U, 0U, 65535U });

        auto* engine = m_controller.zoomTrackEngine(cam);
        if (engine && engine->curveCount() == 0U) {
            LensControl::TrackCurve defaultCurve { };
            defaultCurve.distanceMeters = 0.0F; // Infinity
            defaultCurve.knotCount = 6U;
            defaultCurve.knots[0]
                = { LensControl::MotorPosition { 1000U }, LensControl::MotorPosition { 3900U }, 0.0F };
            defaultCurve.knots[1]
                = { LensControl::MotorPosition { 12000U }, LensControl::MotorPosition { 4480U }, 0.0F };
            defaultCurve.knots[2]
                = { LensControl::MotorPosition { 24000U }, LensControl::MotorPosition { 5190U }, 0.0F };
            defaultCurve.knots[3]
                = { LensControl::MotorPosition { 36000U }, LensControl::MotorPosition { 5980U }, 0.0F };
            defaultCurve.knots[4]
                = { LensControl::MotorPosition { 48000U }, LensControl::MotorPosition { 6840U }, 0.0F };
            defaultCurve.knots[5]
                = { LensControl::MotorPosition { 60000U }, LensControl::MotorPosition { 7820U }, 0.0F };
            LensControl::ZoomTrackFocusEngine::computePchip(defaultCurve);
            (void)engine->addCurve(defaultCurve);
        }
    }
    onRequestKnots(m_activeChannel);
}

void LensWorker::openSource(const QString& uri)
{
    const std::string u = uri.trimmed().toStdString();
    if (u.empty()) {
        return;
    }

    stop();
    m_currentUri = u;

    if (!initBackend(u)) {
        emit errorOccurred(QStringLiteral("Failed to open video source: ") + uri);
        return;
    }

    play();
}

bool LensWorker::initBackend(const std::string& uri)
{
    if ((uri.rfind("synthetic://", 0U) == 0U) || (uri.rfind("sim://", 0U) == 0U) || (uri == "synthetic")) {
        m_isSynthetic = true;

        DefocusPatternType pat = DefocusPatternType::SiemensStar;
        if (uri.find("iso") != std::string::npos) {
            pat = DefocusPatternType::IsoChart;
        } else if (uri.find("natural") != std::string::npos) {
            pat = DefocusPatternType::NaturalTexture;
        }

        SyntheticDefocusConfig cfg { };
        cfg.width = 1280U;
        cfg.height = 720U;
        cfg.frameRate = 30.0;
        cfg.pattern = pat;
        m_defocusGen.setConfig(cfg);

        m_frameWidth = cfg.width;
        m_frameHeight = cfg.height;
        m_nominalFps = cfg.frameRate;

        auto& writeSlot = m_tripleBuffer.getWriteBuffer();
        writeSlot.preallocate(m_frameWidth, m_frameHeight);

        emit sourceOpened(static_cast<int>(m_frameWidth), static_cast<int>(m_frameHeight), m_nominalFps);
        return true;
    }

    m_isSynthetic = false;
    m_decoder = Video::DecoderFactory::create(Video::BackendType::FFmpeg);
    if ((m_decoder == nullptr) || !m_decoder->initialize(uri, Video::PixelFormat::RGB24)) {
        return false;
    }

    const Video::VideoMetadata meta = m_decoder->getVideoMetadata();
    m_frameWidth = static_cast<std::uint32_t>((meta.width > 0) ? meta.width : 1280);
    m_frameHeight = static_cast<std::uint32_t>((meta.height > 0) ? meta.height : 720);
    m_nominalFps = (meta.frameRate > 0.0) ? meta.frameRate : kDefaultFps;

    auto& writeSlot = m_tripleBuffer.getWriteBuffer();
    writeSlot.preallocate(m_frameWidth, m_frameHeight);

    emit sourceOpened(static_cast<int>(m_frameWidth), static_cast<int>(m_frameHeight), m_nominalFps);
    return true;
}

void LensWorker::play()
{
    m_isRunning = true;
    m_isPaused = false;
    m_lastTickTime = std::chrono::steady_clock::now();

    if (m_timer != nullptr) {
        const double fps = (m_nominalFps > 0.0) ? m_nominalFps : kDefaultFps;
        const int intervalMs = std::max(1, static_cast<int>(1000.0 / (fps * m_playbackRate)));
        m_timer->start(intervalMs);
    }

    emit playbackStateChanged(2); // Playing
}

void LensWorker::pause()
{
    m_isRunning = false;
    m_isPaused = true;

    if (m_timer != nullptr) {
        m_timer->stop();
    }

    emit playbackStateChanged(3); // Paused
}

void LensWorker::stop()
{
    m_isRunning = false;
    m_isPaused = false;

    if (m_timer != nullptr) {
        m_timer->stop();
    }

    if (m_decoder != nullptr) {
        m_decoder->close();
        m_decoder.reset();
    }

    emit playbackStateChanged(0); // Idle
}

void LensWorker::stepFrame()
{
    if (m_isPaused || !m_isRunning) {
        processLoopTick();
    }
}

void LensWorker::seek(double timestampSec)
{
    if (m_decoder != nullptr) {
        m_decoder->seek(timestampSec);
        if (m_isPaused) {
            processLoopTick();
        }
    }
}

void LensWorker::setPlaybackRate(double rate)
{
    m_playbackRate = std::clamp(rate, 0.25, 4.0);
    if (m_isRunning && (m_timer != nullptr)) {
        const double fps = (m_nominalFps > 0.0) ? m_nominalFps : kDefaultFps;
        const int intervalMs = std::max(1, static_cast<int>(1000.0 / (fps * m_playbackRate)));
        m_timer->start(intervalMs);
    }
}

void LensWorker::setSyntheticTarget(int focusPlane)
{
    const auto plane = static_cast<std::uint16_t>(std::clamp(focusPlane, 0, 65535));
    m_defocusGen.setTargetPlane(plane);
}

void LensWorker::setSyntheticPattern(int pattern)
{
    auto cfg = m_defocusGen.config();
    switch (pattern) {
    case 0:
        cfg.pattern = DefocusPatternType::SiemensStar;
        break;
    case 1:
        cfg.pattern = DefocusPatternType::IsoChart;
        break;
    case 2:
        cfg.pattern = DefocusPatternType::NaturalTexture;
        break;
    default:
        cfg.pattern = DefocusPatternType::SiemensStar;
        break;
    }
    m_defocusGen.setConfig(cfg);
}

void LensWorker::setSyntheticNoise(double sigma)
{
    auto cfg = m_defocusGen.config();
    cfg.noiseSigma = static_cast<float>(std::clamp(sigma, 0.0, 25.0));
    m_defocusGen.setConfig(cfg);
}

void LensWorker::setRoi(double x, double y, double w, double h)
{
    const auto col = static_cast<std::uint8_t>(std::clamp(x * 255.0, 0.0, 255.0));
    const auto row = static_cast<std::uint8_t>(std::clamp(y * 255.0, 0.0, 255.0));
    const auto width = static_cast<std::uint8_t>(std::clamp(w * 255.0, 1.0, 256.0 - col));
    const auto height = static_cast<std::uint8_t>(std::clamp(h * 255.0, 1.0, 256.0 - row));

    m_activeRoi = LensControl::NormalizedRoi { col, row, width, height };

    const LensControl::CameraIndex cam(static_cast<std::uint8_t>(m_activeChannel));
    m_controller.setFocusRoi(cam, m_activeRoi);
}

void LensWorker::onTriggerAf(int channel, int algorithm, int metric)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);

    m_controller.setFocusMetric(cam, static_cast<LensControl::FocusMetricType>(metric));
    m_controller.triggerAutoFocus(cam, static_cast<LensControl::AutoFocusMethod>(algorithm));
    processLoopTick();
}

void LensWorker::onAbortAf(int channel)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);

    m_controller.stopAutoFocus(cam);
    processLoopTick();
}

void LensWorker::onSetCaf(int channel, bool enabled, double threshold)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);

    m_controller.setCafEnabled(cam, enabled, static_cast<float>(threshold));
}

void LensWorker::onSetTrackerRoi(int channel, double nx, double ny, double nw, double nh)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);

    m_controller.setTrackerRoi(
        cam, static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nw), static_cast<float>(nh));
}

void LensWorker::onSetFocusMetric(int channel, int metric)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);

    m_controller.setFocusMetric(cam, static_cast<LensControl::FocusMetricType>(metric));
}

void LensWorker::onSetActiveChannel(int channel)
{
    m_activeChannel = std::clamp(channel, 0, 3);
}

void LensWorker::onSetLensMode(int channel, int mode)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.setLensMode(LensControl::CameraIndex(c), static_cast<LensControl::LensMode>(mode));
}

void LensWorker::onMoveFocusStep(int channel, int steps)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.moveFocusStep(LensControl::CameraIndex(c), static_cast<std::int16_t>(steps));
    processLoopTick();
}

void LensWorker::onMoveZoomStep(int channel, int steps)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.moveZoomStep(LensControl::CameraIndex(c), static_cast<std::int16_t>(steps));
    processLoopTick();
}

void LensWorker::onSetFocusTarget(int channel, int position)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.setFocusTarget(
        LensControl::CameraIndex(c), LensControl::MotorPosition(static_cast<std::uint16_t>(position)));
    processLoopTick();
}

void LensWorker::onSetZoomTarget(int channel, int position)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.setZoomTarget(
        LensControl::CameraIndex(c), LensControl::MotorPosition(static_cast<std::uint16_t>(position)));
    processLoopTick();
}

void LensWorker::onSetFocusSpeed(int channel, int speed)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.setFocusSpeed(LensControl::CameraIndex(c), LensControl::MotorSpeed(static_cast<std::uint8_t>(speed)));
}

void LensWorker::onSetZoomSpeed(int channel, int speed)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.setZoomSpeed(LensControl::CameraIndex(c), LensControl::MotorSpeed(static_cast<std::uint8_t>(speed)));
}

void LensWorker::onStopMotion(int channel)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    m_controller.stopMotion(LensControl::CameraIndex(c));
}

void LensWorker::onEmergencyStop()
{
    for (std::uint8_t i = 0U; i < 4U; ++i) {
        m_controller.stopMotion(LensControl::CameraIndex(i));
    }
    processLoopTick();
}

void LensWorker::onBindMock(int channel)
{
    const auto c = static_cast<std::size_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(static_cast<std::uint8_t>(c));

    m_controller.setDriver(cam, nullptr);
    m_driverSlots[c].serialTransport.reset();
    m_driverSlots[c].driverType = 0;
    m_driverSlots[c].portName = "IN-MEMORY";
    m_driverSlots[c].baudRate = 115200U;
    m_driverSlots[c].address = static_cast<std::uint8_t>(c + 1U);

    auto mock = std::make_unique<LensControl::MockLensDriver>();
    static_cast<void>(mock->initialize(static_cast<std::uint8_t>(c), 115200U));
    m_controller.setDriver(cam, std::move(mock));

    emit driverBound(
        static_cast<int>(c), 0, QStringLiteral("IN-MEMORY"), 115200, static_cast<int>(c + 1U), true, QString());
    processLoopTick();
}

void LensWorker::onBindPelco(int channel, int port, int baud, int address)
{
    const auto c = static_cast<std::size_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(static_cast<std::uint8_t>(c));

#ifdef _WIN32
    const std::string portName = "COM" + std::to_string(port);
#else
    const std::string portName = "/dev/ttyUSB" + std::to_string(port);
#endif

    // Port collision guard
    for (std::size_t i = 0U; i < 4U; ++i) {
        if (i != c && m_driverSlots[i].serialTransport && m_driverSlots[i].serialTransport->isOpen()
            && m_driverSlots[i].portName == portName) {
            emit driverBound(static_cast<int>(c), 1, QString::fromStdString(portName), baud, address, false,
                QStringLiteral("Port collision: %1 already bound to channel %2")
                    .arg(QString::fromStdString(portName))
                    .arg(i));
            return;
        }
    }

    m_controller.setDriver(cam, nullptr);
    m_driverSlots[c].serialTransport.reset();

    auto transport = std::make_unique<Transport::SerialTransport>(portName, static_cast<std::uint32_t>(baud));
    const bool opened = transport->open();

    auto pelco = std::make_unique<LensControl::PelcoDLensDriver>(static_cast<std::uint8_t>(address));
    static_cast<void>(pelco->initialize(static_cast<std::uint8_t>(port), static_cast<std::uint32_t>(baud)));

    pelco->setTransmitCallback(&transmitBridge, transport.get());
    auto* rawPelco = pelco.get();
    transport->setDataCallback([rawPelco](const std::vector<std::uint8_t>& bytes) {
        if (rawPelco && !bytes.empty()) {
            rawPelco->processIncomingBytes(bytes.data(), bytes.size());
        }
    });

    m_driverSlots[c].serialTransport = std::move(transport);
    m_driverSlots[c].driverType = 1;
    m_driverSlots[c].portName = portName;
    m_driverSlots[c].baudRate = static_cast<std::uint32_t>(baud);
    m_driverSlots[c].address = static_cast<std::uint8_t>(address);

    m_controller.setDriver(cam, std::move(pelco));

    emit driverBound(static_cast<int>(c), 1, QString::fromStdString(portName), baud, address, opened,
        opened ? QString()
               : QStringLiteral("Serial port %1 failed to open (offline mode)").arg(QString::fromStdString(portName)));
    processLoopTick();
}

void LensWorker::onBindVisca(int channel, int port, int baud, int address)
{
    const auto c = static_cast<std::size_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(static_cast<std::uint8_t>(c));

#ifdef _WIN32
    const std::string portName = "COM" + std::to_string(port);
#else
    const std::string portName = "/dev/ttyUSB" + std::to_string(port);
#endif

    // Port collision guard
    for (std::size_t i = 0U; i < 4U; ++i) {
        if (i != c && m_driverSlots[i].serialTransport && m_driverSlots[i].serialTransport->isOpen()
            && m_driverSlots[i].portName == portName) {
            emit driverBound(static_cast<int>(c), 2, QString::fromStdString(portName), baud, address, false,
                QStringLiteral("Port collision: %1 already bound to channel %2")
                    .arg(QString::fromStdString(portName))
                    .arg(i));
            return;
        }
    }

    m_controller.setDriver(cam, nullptr);
    m_driverSlots[c].serialTransport.reset();

    auto transport = std::make_unique<Transport::SerialTransport>(portName, static_cast<std::uint32_t>(baud));
    const bool opened = transport->open();

    auto visca = std::make_unique<LensControl::ViscaLensDriver>(static_cast<std::uint8_t>(address));
    static_cast<void>(visca->initialize(static_cast<std::uint8_t>(port), static_cast<std::uint32_t>(baud)));

    visca->setTransmitCallback(&transmitBridge, transport.get());
    auto* rawVisca = visca.get();
    transport->setDataCallback([rawVisca](const std::vector<std::uint8_t>& bytes) {
        if (rawVisca && !bytes.empty()) {
            rawVisca->processIncomingBytes(bytes.data(), bytes.size());
        }
    });

    m_driverSlots[c].serialTransport = std::move(transport);
    m_driverSlots[c].driverType = 2;
    m_driverSlots[c].portName = portName;
    m_driverSlots[c].baudRate = static_cast<std::uint32_t>(baud);
    m_driverSlots[c].address = static_cast<std::uint8_t>(address);

    m_controller.setDriver(cam, std::move(visca));

    emit driverBound(static_cast<int>(c), 2, QString::fromStdString(portName), baud, address, opened,
        opened ? QString()
               : QStringLiteral("Serial port %1 failed to open (offline mode)").arg(QString::fromStdString(portName)));
    processLoopTick();
}

void LensWorker::onLoadDll(int channel, const QString& path, int port, int baud)
{
    const auto c = static_cast<std::size_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(static_cast<std::uint8_t>(c));

    m_controller.setDriver(cam, nullptr);
    m_driverSlots[c].serialTransport.reset();

    const std::string pathStd = path.toStdString();
    const auto result
        = m_controller.loadLensDll(cam, pathStd, static_cast<std::uint8_t>(port), static_cast<std::uint32_t>(baud));

    const bool success = (result == LensControl::LensLoadResult::Success);
    QString errorMsg;
    if (!success) {
        switch (result) {
        case LensControl::LensLoadResult::FileNotFound:
            errorMsg = QStringLiteral("DLL library not found at: ") + path;
            break;
        case LensControl::LensLoadResult::MissingCreateSymbol:
            errorMsg = QStringLiteral("Required factory symbol missing in DLL");
            break;
        case LensControl::LensLoadResult::PortCollision:
            errorMsg = QStringLiteral("Port conflict: port %1 already bound").arg(port);
            break;
        default:
            errorMsg = QStringLiteral("Failed to initialize lens driver DLL (code %1)").arg(static_cast<int>(result));
            break;
        }
    }

#ifdef _WIN32
    const std::string portName = "COM" + std::to_string(port);
#else
    const std::string portName = "/dev/ttyUSB" + std::to_string(port);
#endif

    m_driverSlots[c].driverType = 3;
    m_driverSlots[c].portName = portName;
    m_driverSlots[c].baudRate = static_cast<std::uint32_t>(baud);
    m_driverSlots[c].address = 1U;

    emit driverBound(static_cast<int>(c), 3, QString::fromStdString(portName), baud, 1, success, errorMsg);
    processLoopTick();
}

void LensWorker::extractLuma(const std::uint8_t* rgb, std::uint8_t* luma, std::size_t pixels) noexcept
{
    for (std::size_t i = 0U; i < pixels; ++i) {
        const std::size_t idx = i * 3U;
        const std::uint32_t r = rgb[idx];
        const std::uint32_t g = rgb[idx + 1U];
        const std::uint32_t b = rgb[idx + 2U];
        luma[i] = static_cast<std::uint8_t>((77U * r + 150U * g + 29U * b) >> 8U);
    }
}

void LensWorker::processLoopTick()
{
    const auto now = std::chrono::steady_clock::now();
    double dt = 1.0 / ((m_nominalFps > 0.0) ? m_nominalFps : kDefaultFps);
    if (m_lastTickTime.time_since_epoch().count() > 0) {
        dt = std::chrono::duration<double>(now - m_lastTickTime).count();
    }
    m_lastTickTime = now;
    if ((dt <= 0.0) || (dt > 0.2)) {
        dt = 1.0 / ((m_nominalFps > 0.0) ? m_nominalFps : kDefaultFps);
    }

    // Step the optical controller state machine
    for (std::uint8_t i = 0U; i < 4U; ++i) {
        m_controller.update(LensControl::CameraIndex(i));
    }

    const LensControl::CameraIndex activeCam(static_cast<std::uint8_t>(m_activeChannel));
    const LensControl::LensStatus status = m_controller.getLensStatus(activeCam);

    // Acquire write slot in triple buffer
    auto& writeSlot = m_tripleBuffer.getWriteBuffer();
    if ((writeSlot.width != m_frameWidth) || (writeSlot.height != m_frameHeight)) {
        writeSlot.preallocate(m_frameWidth, m_frameHeight);
    }

    if (m_isSynthetic) {
        m_defocusGen.updateMotorState(status.focusPos.value(), status.zoomPos.value());
        writeSlot.timestampSec = m_defocusGen.generateNextFrame(writeSlot.rgbBuffer.data(), dt);
        writeSlot.isSynthetic = true;
    } else {
        if (m_decoder != nullptr) {
            if (!m_decoder->decodeNextFrame()) {
                // Loop video or wait
                m_decoder->seek(0.0);
            }
            const Video::FrameInfo info = m_decoder->getRawFrameData();
            if ((info.data != nullptr) && (info.size > 0U)) {
                const std::size_t copyBytes = std::min(writeSlot.rgbBuffer.size(), info.size);
                std::memcpy(writeSlot.rgbBuffer.data(), info.data, copyBytes);
            }
            writeSlot.timestampSec = info.timestamp;
            writeSlot.isSynthetic = false;
        }
    }

    writeSlot.frameId = ++m_frameCounter;

    // Fast integer extraction of grayscale luma
    const std::size_t pixelCount = static_cast<std::size_t>(m_frameWidth) * static_cast<std::size_t>(m_frameHeight);
    extractLuma(writeSlot.rgbBuffer.data(), writeSlot.lumaBuffer.data(), pixelCount);

    // Ingest luma frame into optical engine for focus evaluation
    const auto nowNs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
    m_controller.processFrame(activeCam, writeSlot.lumaBuffer.data(), static_cast<std::size_t>(m_frameWidth),
        static_cast<std::size_t>(m_frameHeight), nowNs, static_cast<std::uint32_t>(writeSlot.frameId));

    // Telemetry snapshot
    writeSlot.status = status;
    writeSlot.metricScore = m_controller.getLastMetricScore(activeCam);
    writeSlot.activeRoi = m_activeRoi;

    // Atomically publish finished frame to consumer
    m_tripleBuffer.publishWriteBuffer();

    emit frameReady();
    emit statusUpdated(status.focusPos.value(), status.zoomPos.value(),
        static_cast<int>(status.focusLimits | status.zoomLimits), static_cast<int>(status.mode), status.isAfActive);
    emit telemetryReady(static_cast<double>(writeSlot.metricScore.metric), m_activeChannel);

    const auto afState = m_controller.getAutoFocusState(activeCam);
    if ((afState != m_lastAfState) || (status.isAfActive != m_lastAfRunning)) {
        m_lastAfState = afState;
        m_lastAfRunning = status.isAfActive;
        emit afStateUpdated(static_cast<int>(afState), status.isAfActive);
    }

    if (afState == LensControl::AutoFocusState::PeakConfirmed || afState == LensControl::AutoFocusState::SlewToPeak) {
        const auto peak = m_controller.getPeakFitResult(activeCam);
        if (peak.isValid) {
            emit peakFitted(static_cast<double>(peak.vertex.value()), static_cast<double>(peak.peakScore),
                static_cast<double>(peak.curvature), static_cast<double>(peak.rSquared), peak.isValid);
        }
    }

    const std::size_t sampleCount = m_controller.getAfSampleCount(activeCam);
    if (sampleCount > 0U) {
        QVariantList sampleList;
        sampleList.reserve(static_cast<int>(sampleCount));
        for (std::size_t i = 0U; i < sampleCount; ++i) {
            const auto pt = m_controller.getAfSample(activeCam, i);
            sampleList.append(QPointF(static_cast<qreal>(pt.pos.value()), static_cast<qreal>(pt.score.metric)));
        }
        emit samplesReady(sampleList);
    }

    stepCalibWizard(status);
}

void LensWorker::stepCalibWizard(const LensControl::LensStatus& status)
{
    if (m_calibContext.state == CalibWizardState::Idle) {
        return;
    }

    const auto camIdx = static_cast<std::uint8_t>(m_calibContext.channel);
    const LensControl::CameraIndex cam(camIdx);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        m_calibContext.state = CalibWizardState::Idle;
        emit calibFinished(m_calibContext.channel, false, QStringLiteral("Invalid zoom-track engine"));
        return;
    }

    switch (m_calibContext.state) {
    case CalibWizardState::SlewingZoom: {
        const int curZ = static_cast<int>(status.zoomPos.value());
        const int targetZ = static_cast<int>(m_calibContext.currentTargetZoom);
        if (std::abs(curZ - targetZ) <= 30) {
            m_calibContext.state = CalibWizardState::SettlingOptics;
            m_calibContext.settleStartTime = std::chrono::steady_clock::now();
            const double prog
                = static_cast<double>(m_calibContext.currentKnotIdx) / static_cast<double>(m_calibContext.totalKnots);
            emit calibProgress(m_calibContext.channel, static_cast<int>(m_calibContext.currentKnotIdx + 1U),
                static_cast<int>(m_calibContext.totalKnots), prog, QStringLiteral("Settling optics..."));
        }
        break;
    }
    case CalibWizardState::SettlingOptics: {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_calibContext.settleStartTime >= m_calibContext.settleDelay) {
            m_calibContext.state = CalibWizardState::WaitingAfSweep;
            m_controller.triggerAutoFocus(cam, LensControl::AutoFocusMethod::ScanThreePhase);
            const double prog = (static_cast<double>(m_calibContext.currentKnotIdx) + 0.3)
                / static_cast<double>(m_calibContext.totalKnots);
            emit calibProgress(m_calibContext.channel, static_cast<int>(m_calibContext.currentKnotIdx + 1U),
                static_cast<int>(m_calibContext.totalKnots), prog, QStringLiteral("Focus sweep in progress..."));
        }
        break;
    }
    case CalibWizardState::WaitingAfSweep: {
        const auto afState = m_controller.getAutoFocusState(cam);
        if (afState == LensControl::AutoFocusState::PeakConfirmed || !status.isAfActive) {
            m_calibContext.state = CalibWizardState::RecordingKnot;
        }
        break;
    }
    case CalibWizardState::RecordingKnot: {
        const auto peak = m_controller.getPeakFitResult(cam);
        const LensControl::MotorPosition focusSummit = peak.isValid ? peak.vertex : status.focusPos;
        engine->recordCalibKnot(focusSummit);
        m_calibContext.currentKnotIdx++;

        if (m_calibContext.currentKnotIdx < m_calibContext.totalKnots) {
            m_calibContext.currentTargetZoom = engine->nextCalibZoom().value();
            m_controller.setZoomTarget(cam, LensControl::MotorPosition(m_calibContext.currentTargetZoom));
            m_calibContext.state = CalibWizardState::SlewingZoom;
            const double prog
                = static_cast<double>(m_calibContext.currentKnotIdx) / static_cast<double>(m_calibContext.totalKnots);
            emit calibProgress(m_calibContext.channel, static_cast<int>(m_calibContext.currentKnotIdx + 1U),
                static_cast<int>(m_calibContext.totalKnots), prog, QStringLiteral("Slewing to next zoom knot..."));
        } else {
            m_calibContext.state = CalibWizardState::Completed;
            emit calibProgress(m_calibContext.channel, static_cast<int>(m_calibContext.totalKnots),
                static_cast<int>(m_calibContext.totalKnots), 1.0, QStringLiteral("Calibration completed"));
            emit calibFinished(
                m_calibContext.channel, true, QStringLiteral("Parfocal calibration completed successfully."));
            m_calibContext.state = CalibWizardState::Idle;
            onRequestKnots(m_calibContext.channel);
        }
        break;
    }
    default:
        break;
    }
}

void LensWorker::onSetZoomTrack(int channel, bool enabled)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    m_controller.setZoomTrackFocus(cam, enabled);
    emit zoomTrackState(channel, enabled);
}

void LensWorker::onSetInterpMode(int channel, int mode)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (engine) {
        engine->setMode((mode == 0) ? LensControl::InterpolationMode::PchipSpline
                                    : LensControl::InterpolationMode::PiecewiseLinear);
    }
    onRequestKnots(channel);
}

void LensWorker::onSetActiveCurve(int channel, int index)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (engine && index >= 0 && static_cast<std::size_t>(index) < engine->curveCount()) {
        engine->setActiveCurve(static_cast<std::size_t>(index));
    }
    onRequestKnots(channel);
}

void LensWorker::onSetCurveDistance(int channel, int index, double distanceMeters)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (engine && index >= 0 && static_cast<std::size_t>(index) < engine->curveCount()) {
        const auto* cur = engine->getCurve(static_cast<std::size_t>(index));
        if (cur != nullptr) {
            LensControl::TrackCurve mod = *cur;
            mod.distanceMeters = static_cast<float>(distanceMeters);
            (void)engine->addCurve(mod);
        }
    }
    onRequestKnots(channel);
}

void LensWorker::onAddKnot(int channel, int zoom, int focus)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        return;
    }
    const std::size_t activeIdx = engine->activeCurve();
    const auto* curve = engine->getCurve(activeIdx);
    LensControl::TrackCurve modifiedCurve { };
    if (curve != nullptr) {
        modifiedCurve = *curve;
    }
    if (modifiedCurve.knotCount < LensControl::TrackCurve::kMaxKnots) {
        const std::size_t idx = modifiedCurve.knotCount;
        modifiedCurve.knots[idx].zoom
            = LensControl::MotorPosition(static_cast<std::uint16_t>(std::clamp(zoom, 0, 65535)));
        modifiedCurve.knots[idx].focus
            = LensControl::MotorPosition(static_cast<std::uint16_t>(std::clamp(focus, 0, 65535)));
        modifiedCurve.knots[idx].slope = 0.0F;
        modifiedCurve.knotCount++;

        std::sort(modifiedCurve.knots.begin(),
            modifiedCurve.knots.begin() + static_cast<std::ptrdiff_t>(modifiedCurve.knotCount),
            [](const LensControl::TrackKnot& a, const LensControl::TrackKnot& b) {
                return a.zoom.value() < b.zoom.value();
            });

        LensControl::ZoomTrackFocusEngine::computePchip(modifiedCurve);
        if (curve != nullptr) {
            (void)engine->setCurve(activeIdx, modifiedCurve);
        } else {
            (void)engine->addCurve(modifiedCurve);
        }
    }
    onRequestKnots(static_cast<int>(c));
}

void LensWorker::onUpdateKnot(int channel, int index, int zoom, int focus)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        return;
    }
    const std::size_t activeIdx = engine->activeCurve();
    const auto* curve = engine->getCurve(activeIdx);
    if (!curve || index < 0 || static_cast<std::size_t>(index) >= curve->knotCount) {
        return;
    }
    LensControl::TrackCurve modifiedCurve = *curve;
    const auto kIdx = static_cast<std::size_t>(index);
    modifiedCurve.knots[kIdx].zoom = LensControl::MotorPosition(static_cast<std::uint16_t>(std::clamp(zoom, 0, 65535)));
    modifiedCurve.knots[kIdx].focus
        = LensControl::MotorPosition(static_cast<std::uint16_t>(std::clamp(focus, 0, 65535)));

    std::sort(modifiedCurve.knots.begin(),
        modifiedCurve.knots.begin() + static_cast<std::ptrdiff_t>(modifiedCurve.knotCount),
        [](const LensControl::TrackKnot& a, const LensControl::TrackKnot& b) {
            return a.zoom.value() < b.zoom.value();
        });
    LensControl::ZoomTrackFocusEngine::computePchip(modifiedCurve);
    (void)engine->setCurve(activeIdx, modifiedCurve);
    onRequestKnots(static_cast<int>(c));
}

void LensWorker::onRemoveKnot(int channel, int index)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        return;
    }
    const std::size_t activeIdx = engine->activeCurve();
    const auto* curve = engine->getCurve(activeIdx);
    if (!curve || index < 0 || static_cast<std::size_t>(index) >= curve->knotCount) {
        return;
    }
    LensControl::TrackCurve modifiedCurve = *curve;
    const auto kIdx = static_cast<std::size_t>(index);
    for (std::size_t i = kIdx; i + 1U < modifiedCurve.knotCount; ++i) {
        modifiedCurve.knots[i] = modifiedCurve.knots[i + 1U];
    }
    modifiedCurve.knotCount--;
    if (modifiedCurve.knotCount >= 2U) {
        LensControl::ZoomTrackFocusEngine::computePchip(modifiedCurve);
    }
    (void)engine->setCurve(activeIdx, modifiedCurve);
    onRequestKnots(static_cast<int>(c));
}

void LensWorker::onClearKnots(int channel)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        return;
    }
    const std::size_t activeIdx = engine->activeCurve();
    const auto* curve = engine->getCurve(activeIdx);
    LensControl::TrackCurve emptyCurve { };
    if (curve != nullptr) {
        emptyCurve.distanceMeters = curve->distanceMeters;
    }
    emptyCurve.knotCount = 0U;
    if (curve != nullptr) {
        (void)engine->setCurve(activeIdx, emptyCurve);
    } else {
        (void)engine->addCurve(emptyCurve);
    }
    onRequestKnots(static_cast<int>(c));
}

void LensWorker::onSortKnots(int channel)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        return;
    }
    const std::size_t activeIdx = engine->activeCurve();
    const auto* curve = engine->getCurve(activeIdx);
    if (!curve || curve->knotCount < 2U) {
        return;
    }
    LensControl::TrackCurve sortedCurve = *curve;
    std::sort(sortedCurve.knots.begin(), sortedCurve.knots.begin() + static_cast<std::ptrdiff_t>(sortedCurve.knotCount),
        [](const LensControl::TrackKnot& a, const LensControl::TrackKnot& b) {
            return a.zoom.value() < b.zoom.value();
        });
    LensControl::ZoomTrackFocusEngine::computePchip(sortedCurve);
    (void)engine->setCurve(activeIdx, sortedCurve);
    onRequestKnots(static_cast<int>(c));
}

void LensWorker::onRequestKnots(int channel)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        return;
    }

    const std::size_t activeIdx = engine->activeCurve();
    const std::size_t count = engine->curveCount();
    const auto* curve = engine->getCurve(activeIdx);

    QVariantList knotList;
    double dist = 0.0;
    bool valid = false;

    if (curve != nullptr) {
        dist = static_cast<double>(curve->distanceMeters);
        valid = curve->isValid();
        for (std::size_t k = 0U; k < curve->knotCount; ++k) {
            QVariantMap kMap;
            kMap[QStringLiteral("zoom")] = static_cast<int>(curve->knots[k].zoom.value());
            kMap[QStringLiteral("focus")] = static_cast<int>(curve->knots[k].focus.value());
            kMap[QStringLiteral("slope")] = static_cast<double>(curve->knots[k].slope);
            knotList.append(kMap);
        }
    }

    emit knotsReady(static_cast<int>(c), knotList);
    emit curveSetUpdated(static_cast<int>(c), static_cast<int>(count), static_cast<int>(activeIdx), dist, valid);
}

void LensWorker::onStartCalib(int channel, int knots, int zMin, int zMax, double dist)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        emit calibFinished(channel, false, QStringLiteral("No zoom track engine found"));
        return;
    }

    const std::size_t clampedKnots = static_cast<std::size_t>(std::clamp(knots, 4, 32));
    const auto zm = static_cast<std::uint16_t>(std::clamp(zMin, 0, 65535));
    const auto zx = static_cast<std::uint16_t>(std::clamp(zMax, static_cast<int>(zm) + 100, 65535));

    engine->startCalib(clampedKnots, LensControl::MotorPosition(zm), LensControl::MotorPosition(zx));

    m_calibContext.state = CalibWizardState::SlewingZoom;
    m_calibContext.channel = static_cast<int>(c);
    m_calibContext.totalKnots = clampedKnots;
    m_calibContext.currentKnotIdx = 0U;
    m_calibContext.zoomMin = zm;
    m_calibContext.zoomMax = zx;
    m_calibContext.targetDistanceMeters = static_cast<float>(dist);
    m_calibContext.currentTargetZoom = engine->nextCalibZoom().value();
    m_calibContext.settleDelay = std::chrono::milliseconds(200);

    // Disable real-time zoom tracking during calibration
    m_controller.setZoomTrackFocus(cam, false);

    // Slew zoom to initial knot
    m_controller.setZoomTarget(cam, LensControl::MotorPosition(m_calibContext.currentTargetZoom));

    emit calibProgress(
        channel, 1, static_cast<int>(clampedKnots), 0.0, QStringLiteral("Slewing to first zoom knot..."));
}

void LensWorker::onCancelCalib(int channel)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (engine && engine->isCalibrating()) {
        engine->cancelCalib();
    }
    m_controller.stopAutoFocus(cam);
    m_calibContext.state = CalibWizardState::Idle;
    emit calibFinished(channel, false, QStringLiteral("Calibration cancelled by operator."));
    onRequestKnots(channel);
}

void LensWorker::onLoadTrackFile(int channel, const QString& path)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    const bool success = m_controller.loadTrackCurves(cam, path.toStdString());
    const QString msg = success ? QStringLiteral("Track curves loaded successfully.")
                                : QStringLiteral("Failed to parse track curve file.");
    emit curveIoFinished(static_cast<int>(c), success, msg);
    onRequestKnots(static_cast<int>(c));
}

void LensWorker::onSaveTrackFile(int channel, const QString& path)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    const bool success = m_controller.saveTrackCurves(cam, path.toStdString());
    const QString msg = success ? QStringLiteral("Track curves saved successfully.")
                                : QStringLiteral("Failed to write track curve file.");
    emit curveIoFinished(static_cast<int>(c), success, msg);
}

void LensWorker::onExportJsonProfile(int channel, const QString& path)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        emit curveIoFinished(channel, false, QStringLiteral("Engine unavailable"));
        return;
    }

    QJsonObject rootObj;
    rootObj[QStringLiteral("version")] = 1;
    rootObj[QStringLiteral("channel")] = static_cast<int>(c);
    rootObj[QStringLiteral("interpolationMode")] = (engine->mode() == LensControl::InterpolationMode::PchipSpline)
        ? QStringLiteral("PchipSpline")
        : QStringLiteral("PiecewiseLinear");
    rootObj[QStringLiteral("activeCurveIndex")] = static_cast<int>(engine->activeCurve());

    QJsonArray curvesArray;
    for (std::size_t ci = 0U; ci < engine->curveCount(); ++ci) {
        const auto* curve = engine->getCurve(ci);
        if (!curve)
            continue;
        QJsonObject cObj;
        cObj[QStringLiteral("index")] = static_cast<int>(ci);
        cObj[QStringLiteral("distanceMeters")] = static_cast<double>(curve->distanceMeters);
        cObj[QStringLiteral("label")] = (curve->distanceMeters > 0.0F)
            ? QString::asprintf("%.1fm", static_cast<double>(curve->distanceMeters))
            : QStringLiteral("Infinity");

        QJsonArray knotsArray;
        for (std::size_t ki = 0U; ki < curve->knotCount; ++ki) {
            QJsonObject kObj;
            kObj[QStringLiteral("zoom")] = static_cast<int>(curve->knots[ki].zoom.value());
            kObj[QStringLiteral("focus")] = static_cast<int>(curve->knots[ki].focus.value());
            kObj[QStringLiteral("slope")] = static_cast<double>(curve->knots[ki].slope);
            knotsArray.append(kObj);
        }
        cObj[QStringLiteral("knots")] = knotsArray;
        curvesArray.append(cObj);
    }
    rootObj[QStringLiteral("curves")] = curvesArray;

    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit curveIoFinished(channel, false, QStringLiteral("Failed to open file for writing: ") + path);
        return;
    }
    const QJsonDocument doc(rootObj);
    outFile.write(doc.toJson(QJsonDocument::Indented));
    outFile.close();
    emit curveIoFinished(channel, true, QStringLiteral("JSON profile exported successfully."));
}

void LensWorker::onImportJsonProfile(int channel, const QString& path)
{
    const auto c = static_cast<std::uint8_t>(std::clamp(channel, 0, 3));
    const LensControl::CameraIndex cam(c);
    auto* engine = m_controller.zoomTrackEngine(cam);
    if (!engine) {
        emit curveIoFinished(channel, false, QStringLiteral("Engine unavailable"));
        return;
    }

    QFile inFile(path);
    if (!inFile.open(QIODevice::ReadOnly)) {
        emit curveIoFinished(channel, false, QStringLiteral("Failed to open file for reading: ") + path);
        return;
    }
    const QByteArray data = inFile.readAll();
    inFile.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        emit curveIoFinished(channel, false, QStringLiteral("Malformed JSON: ") + parseErr.errorString());
        return;
    }

    const QJsonObject rootObj = doc.object();
    engine->clearCurves();

    if (rootObj.contains(QStringLiteral("interpolationMode"))) {
        const QString modeStr = rootObj[QStringLiteral("interpolationMode")].toString();
        engine->setMode((modeStr == QStringLiteral("PiecewiseLinear")) ? LensControl::InterpolationMode::PiecewiseLinear
                                                                       : LensControl::InterpolationMode::PchipSpline);
    }

    const QJsonArray curvesArray = rootObj[QStringLiteral("curves")].toArray();
    for (const auto& cVal : curvesArray) {
        if (!cVal.isObject())
            continue;
        const QJsonObject cObj = cVal.toObject();
        LensControl::TrackCurve curve { };
        curve.distanceMeters = static_cast<float>(cObj[QStringLiteral("distanceMeters")].toDouble(0.0));

        const QJsonArray knotsArray = cObj[QStringLiteral("knots")].toArray();
        for (const auto& kVal : knotsArray) {
            if (!kVal.isObject() || curve.knotCount >= LensControl::TrackCurve::kMaxKnots)
                continue;
            const QJsonObject kObj = kVal.toObject();
            const auto z = static_cast<std::uint16_t>(kObj[QStringLiteral("zoom")].toInt(0));
            const auto f = static_cast<std::uint16_t>(kObj[QStringLiteral("focus")].toInt(0));
            curve.knots[curve.knotCount].zoom = LensControl::MotorPosition(z);
            curve.knots[curve.knotCount].focus = LensControl::MotorPosition(f);
            curve.knots[curve.knotCount].slope = static_cast<float>(kObj[QStringLiteral("slope")].toDouble(0.0));
            curve.knotCount++;
        }
        if (curve.knotCount >= 2U) {
            std::sort(curve.knots.begin(), curve.knots.begin() + static_cast<std::ptrdiff_t>(curve.knotCount),
                [](const LensControl::TrackKnot& a, const LensControl::TrackKnot& b) {
                    return a.zoom.value() < b.zoom.value();
                });
            LensControl::ZoomTrackFocusEngine::computePchip(curve);
            (void)engine->addCurve(curve);
        }
    }

    if (rootObj.contains(QStringLiteral("activeCurveIndex"))) {
        const int actIdx = rootObj[QStringLiteral("activeCurveIndex")].toInt(0);
        if (actIdx >= 0 && static_cast<std::size_t>(actIdx) < engine->curveCount()) {
            engine->setActiveCurve(static_cast<std::size_t>(actIdx));
        }
    }

    emit curveIoFinished(channel, true, QStringLiteral("JSON profile imported successfully."));
    onRequestKnots(channel);
}

} // namespace LensApp
