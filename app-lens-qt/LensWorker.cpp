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

#include <QMetaObject>
#include <QTimer>

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
    }
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
}

} // namespace LensApp
