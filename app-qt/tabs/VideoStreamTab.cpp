#include "VideoStreamTab.h"
#include "DecoderFactory.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>

namespace PelcoDApp {

VideoStreamTab::VideoStreamTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device(device)
    , m_worker(new PelcoD::Video::QVideoStreamWorker(this))
{
    setupUi();
    setupConnections();
}

VideoStreamTab::~VideoStreamTab()
{
    if (m_worker != nullptr) {
        m_worker->stopPlayback();
    }
}

void VideoStreamTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // =========================================================================
    // Stream Connection Controls Bar
    // =========================================================================
    auto* topControlsGroup = new QGroupBox(tr("Stream Source & Connection"), this);
    auto* topLayout = new QHBoxLayout(topControlsGroup);
    topLayout->setContentsMargins(8, 6, 8, 6);
    topLayout->setSpacing(8);

    topLayout->addWidget(new QLabel(tr("URL / Preset:"), this));

    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->setEditable(true);
    m_sourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_sourceCombo->addItem(QStringLiteral("mock://smpte-bars"), tr("Mock: SMPTE Color Bars"));
    m_sourceCombo->addItem(QStringLiteral("rtsp://admin:admin@192.168.1.108:554/cam/realmonitor?channel=1&subtype=0"), tr("Preset: Fujinon SX800 RTSP"));
    m_sourceCombo->addItem(QStringLiteral("rtsp://192.168.1.100:554/stream1"), tr("Preset: Camera 1 RTSP"));
    topLayout->addWidget(m_sourceCombo);

    topLayout->addWidget(new QLabel(tr("Backend:"), this));
    m_backendCombo = new QComboBox(this);
    const auto backends = PelcoD::Video::DecoderFactory::availableBackends();
    for (const auto b : backends) {
        switch (b) {
        case PelcoD::Video::BackendType::FFmpeg:
            m_backendCombo->addItem(tr("FFmpeg (RTSP/HW)"), static_cast<int>(b));
            break;
        case PelcoD::Video::BackendType::GStreamer:
            m_backendCombo->addItem(tr("GStreamer (Playbin)"), static_cast<int>(b));
            break;
        case PelcoD::Video::BackendType::Mock:
            m_backendCombo->addItem(tr("Mock Synthetic"), static_cast<int>(b));
            break;
        }
    }
    topLayout->addWidget(m_backendCombo);

    m_btnConnect = new QPushButton(tr("Connect"), this);
    m_btnConnect->setStyleSheet("QPushButton { font-weight: bold; background-color: #2E7D32; color: white; padding: 4px 12px; border-radius: 3px; }"
                                "QPushButton:hover { background-color: #388E3C; }");
    topLayout->addWidget(m_btnConnect);

    m_btnDisconnect = new QPushButton(tr("Disconnect"), this);
    m_btnDisconnect->setEnabled(false);
    topLayout->addWidget(m_btnDisconnect);

    m_btnSnapshot = new QPushButton(tr("Snapshot"), this);
    topLayout->addWidget(m_btnSnapshot);

    m_btnFullscreen = new QPushButton(tr("Fullscreen [F11]"), this);
    topLayout->addWidget(m_btnFullscreen);

    mainLayout->addWidget(topControlsGroup);

    // =========================================================================
    // Central Splitter: Video Overlay Widget + Right Settings/PTZ Panel
    // =========================================================================
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    m_overlayWidget = new VideoOverlayWidget(splitter);
    m_overlayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    splitter->addWidget(m_overlayWidget);

    // Right Side Panel
    auto* sidePanel = new QWidget(splitter);
    sidePanel->setMaximumWidth(280);
    sidePanel->setMinimumWidth(220);
    auto* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(4, 0, 4, 0);
    sideLayout->setSpacing(8);

    // Group 1: HUD Overlays
    auto* hudGroup = new QGroupBox(tr("Tactical HUD Layers"), sidePanel);
    auto* hudLayout = new QVBoxLayout(hudGroup);
    hudLayout->setSpacing(4);

    m_chkCrosshair = new QCheckBox(tr("Target Reticle / Crosshairs"), hudGroup);
    m_chkCrosshair->setChecked(true);
    hudLayout->addWidget(m_chkCrosshair);

    m_chkCompass = new QCheckBox(tr("Compass Heading Tape"), hudGroup);
    m_chkCompass->setChecked(true);
    hudLayout->addWidget(m_chkCompass);

    m_chkPitchLadder = new QCheckBox(tr("Elevation Pitch Ladder"), hudGroup);
    m_chkPitchLadder->setChecked(true);
    hudLayout->addWidget(m_chkPitchLadder);

    m_chkOpticsHud = new QCheckBox(tr("Optics & Zoom Telemetry"), hudGroup);
    m_chkOpticsHud->setChecked(true);
    hudLayout->addWidget(m_chkOpticsHud);

    m_chkDiagnostics = new QCheckBox(tr("Stream Diagnostics HUD"), hudGroup);
    m_chkDiagnostics->setChecked(true);
    hudLayout->addWidget(m_chkDiagnostics);

    m_chkInteractivePtz = new QCheckBox(tr("Click-to-PTZ Joystick"), hudGroup);
    m_chkInteractivePtz->setChecked(true);
    hudLayout->addWidget(m_chkInteractivePtz);

    hudLayout->addWidget(new QLabel(tr("HUD Color Theme:"), hudGroup));
    m_comboColorScheme = new QComboBox(hudGroup);
    m_comboColorScheme->addItem(tr("Tactical Cyan"), QColor(0, 229, 255));
    m_comboColorScheme->addItem(tr("Phosphor Green"), QColor(0, 230, 118));
    m_comboColorScheme->addItem(tr("Amber Night"), QColor(255, 179, 0));
    m_comboColorScheme->addItem(tr("Crisp White"), QColor(255, 255, 255));
    hudLayout->addWidget(m_comboColorScheme);

    sideLayout->addWidget(hudGroup);

    // Group 2: Quick PTZ Keypad
    auto* ptzGroup = new QGroupBox(tr("Quick Camera Control"), sidePanel);
    auto* ptzLayout = new QVBoxLayout(ptzGroup);
    ptzLayout->setSpacing(6);

    auto* gridPad = new QGridLayout();
    gridPad->setSpacing(4);

    auto* btnUpLeft = new QPushButton(QStringLiteral("↖"), ptzGroup);
    auto* btnUp = new QPushButton(QStringLiteral("▲"), ptzGroup);
    auto* btnUpRight = new QPushButton(QStringLiteral("↗"), ptzGroup);
    auto* btnLeft = new QPushButton(QStringLiteral("◀"), ptzGroup);
    auto* btnStop = new QPushButton(QStringLiteral("■"), ptzGroup);
    btnStop->setStyleSheet("font-weight: bold; background-color: #C62828; color: white;");
    auto* btnRight = new QPushButton(QStringLiteral("▶"), ptzGroup);
    auto* btnDownLeft = new QPushButton(QStringLiteral("↙"), ptzGroup);
    auto* btnDown = new QPushButton(QStringLiteral("▼"), ptzGroup);
    auto* btnDownRight = new QPushButton(QStringLiteral("↘"), ptzGroup);

    gridPad->addWidget(btnUpLeft, 0, 0);
    gridPad->addWidget(btnUp, 0, 1);
    gridPad->addWidget(btnUpRight, 0, 2);
    gridPad->addWidget(btnLeft, 1, 0);
    gridPad->addWidget(btnStop, 1, 1);
    gridPad->addWidget(btnRight, 1, 2);
    gridPad->addWidget(btnDownLeft, 2, 0);
    gridPad->addWidget(btnDown, 2, 1);
    gridPad->addWidget(btnDownRight, 2, 2);

    ptzLayout->addLayout(gridPad);

    // Zoom Rocker
    auto* zoomLayout = new QHBoxLayout();
    auto* btnZoomIn = new QPushButton(tr("Zoom +"), ptzGroup);
    auto* btnZoomOut = new QPushButton(tr("Zoom -"), ptzGroup);
    zoomLayout->addWidget(btnZoomIn);
    zoomLayout->addWidget(btnZoomOut);
    ptzLayout->addLayout(zoomLayout);

    // Speed Slider
    auto* speedHeader = new QHBoxLayout();
    speedHeader->addWidget(new QLabel(tr("PTZ Speed:"), ptzGroup));
    m_speedLabel = new QLabel(QStringLiteral("30"), ptzGroup);
    m_speedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    speedHeader->addWidget(m_speedLabel);
    ptzLayout->addLayout(speedHeader);

    m_speedSlider = new QSlider(Qt::Horizontal, ptzGroup);
    m_speedSlider->setRange(1, 63);
    m_speedSlider->setValue(30);
    ptzLayout->addWidget(m_speedSlider);

    sideLayout->addWidget(ptzGroup);
    sideLayout->addStretch();

    splitter->addWidget(sidePanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    // Bottom Status Strip
    m_statusLabel = new QLabel(tr("Status: Idle. Enter an RTSP URL or choose Mock Test Pattern and press Connect."), this);
    m_statusLabel->setStyleSheet("color: #90A4AE; font-size: 11px; padding: 2px 4px;");
    mainLayout->addWidget(m_statusLabel);

    // Connect D-Pad buttons
    auto getSpeed = [this]() -> std::uint8_t {
        return static_cast<std::uint8_t>(m_speedSlider->value());
    };

    connect(btnUp, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->tiltUp(getSpeed()); });
    connect(btnDown, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->tiltDown(getSpeed()); });
    connect(btnLeft, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->panLeft(getSpeed()); });
    connect(btnRight, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->panRight(getSpeed()); });

    connect(btnUpLeft, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->move(-1, getSpeed(), 1, getSpeed()); });
    connect(btnUpRight, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->move(1, getSpeed(), 1, getSpeed()); });
    connect(btnDownLeft, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->move(-1, getSpeed(), -1, getSpeed()); });
    connect(btnDownRight, &QPushButton::pressed, this, [this, getSpeed]() { if (m_device) m_device->move(1, getSpeed(), -1, getSpeed()); });

    connect(btnStop, &QPushButton::clicked, this, [this]() { if (m_device) m_device->stopMotion(); });
    connect(btnZoomIn, &QPushButton::pressed, this, [this]() { if (m_device) m_device->zoomTele(); });
    connect(btnZoomIn, &QPushButton::released, this, [this]() { if (m_device) m_device->zoomStop(); });
    connect(btnZoomOut, &QPushButton::pressed, this, [this]() { if (m_device) m_device->zoomWide(); });
    connect(btnZoomOut, &QPushButton::released, this, [this]() { if (m_device) m_device->zoomStop(); });

    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int val) {
        m_speedLabel->setText(QString::number(val));
    });

    // F11 Shortcut for fullscreen toggle
    auto* f11Shortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(f11Shortcut, &QShortcut::activated, this, &VideoStreamTab::toggleFullscreen);
}

void VideoStreamTab::setupConnections()
{
    // Toolbar buttons
    connect(m_btnConnect, &QPushButton::clicked, this, &VideoStreamTab::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &VideoStreamTab::onDisconnectClicked);
    connect(m_btnSnapshot, &QPushButton::clicked, this, &VideoStreamTab::onSnapshotClicked);
    connect(m_btnFullscreen, &QPushButton::clicked, this, &VideoStreamTab::toggleFullscreen);

    // Layer visibility checkboxes
    connect(m_chkCrosshair, &QCheckBox::toggled, m_overlayWidget, &VideoOverlayWidget::setShowCrosshair);
    connect(m_chkCompass, &QCheckBox::toggled, m_overlayWidget, &VideoOverlayWidget::setShowCompass);
    connect(m_chkPitchLadder, &QCheckBox::toggled, m_overlayWidget, &VideoOverlayWidget::setShowPitchLadder);
    connect(m_chkOpticsHud, &QCheckBox::toggled, m_overlayWidget, &VideoOverlayWidget::setShowOpticsHud);
    connect(m_chkDiagnostics, &QCheckBox::toggled, m_overlayWidget, &VideoOverlayWidget::setShowDiagnostics);
    connect(m_chkInteractivePtz, &QCheckBox::toggled, m_overlayWidget, &VideoOverlayWidget::setInteractivePtzEnabled);
    connect(m_comboColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoStreamTab::onColorSchemeChanged);

    // Worker signals to Overlay Widget
    connect(m_worker, &PelcoD::Video::QVideoStreamWorker::frameReady, m_overlayWidget, &VideoOverlayWidget::updateFrame);
    connect(m_worker, &PelcoD::Video::QVideoStreamWorker::streamStatusChanged, this, &VideoStreamTab::onWorkerStatusChanged);
    connect(m_worker, &PelcoD::Video::QVideoStreamWorker::statsUpdated, this, &VideoStreamTab::onWorkerStatsUpdated);

    // Interactive Joystick signals from Overlay to Device
    connect(m_overlayWidget, &VideoOverlayWidget::panTiltRequested, this, &VideoStreamTab::handleOverlayPanTiltRequested);
    connect(m_overlayWidget, &VideoOverlayWidget::stopPtzRequested, this, &VideoStreamTab::handleOverlayStopRequested);
    connect(m_overlayWidget, &VideoOverlayWidget::zoomRequested, this, &VideoStreamTab::handleOverlayZoomRequested);
}

void VideoStreamTab::onConnectClicked()
{
    const QString source = m_sourceCombo->currentText().trimmed();
    if (source.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Source"), tr("Please enter a valid RTSP stream URL or select a preset."));
        return;
    }

    const auto backend = static_cast<PelcoD::Video::BackendType>(m_backendCombo->currentData().toInt());

    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    m_sourceCombo->setEnabled(false);
    m_backendCombo->setEnabled(false);

    m_worker->openStream(source, backend, PelcoD::Video::DeviceType::CPU);
}

void VideoStreamTab::onDisconnectClicked()
{
    m_worker->stopPlayback();
    m_overlayWidget->clearFrame();

    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    m_sourceCombo->setEnabled(true);
    m_backendCombo->setEnabled(true);
    m_statusLabel->setText(tr("Status: Disconnected"));
}

void VideoStreamTab::onSnapshotClicked()
{
    const QImage snapshot = m_overlayWidget->captureSnapshot(true);
    if (snapshot.isNull()) {
        QMessageBox::information(this, tr("Snapshot"), tr("No active video frame to capture."));
        return;
    }

    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
        QStringLiteral("/PelcoD_Snapshot_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    const QString filePath = QFileDialog::getSaveFileName(this, tr("Save Video Snapshot"), defaultPath, tr("PNG Image (*.png);;JPEG Image (*.jpg)"));
    if (!filePath.isEmpty()) {
        if (snapshot.save(filePath)) {
            m_statusLabel->setText(tr("Snapshot saved to: %1").arg(filePath));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to save snapshot file."));
        }
    }
}

void VideoStreamTab::onColorSchemeChanged(int index)
{
    const QColor color = m_comboColorScheme->itemData(index).value<QColor>();
    m_overlayWidget->setHudColor(color);
}

void VideoStreamTab::onWorkerStatusChanged(PelcoD::Video::StreamState state, const QString& message)
{
    m_statusLabel->setText(tr("Stream: %1").arg(message));

    if (state == PelcoD::Video::StreamState::Disconnected || state == PelcoD::Video::StreamState::Error) {
        m_btnConnect->setEnabled(true);
        m_btnDisconnect->setEnabled(false);
        m_sourceCombo->setEnabled(true);
        m_backendCombo->setEnabled(true);
    } else if (state == PelcoD::Video::StreamState::Streaming) {
        m_btnConnect->setEnabled(false);
        m_btnDisconnect->setEnabled(true);
    }
}

void VideoStreamTab::onWorkerStatsUpdated(double fps, double avgDecodeMs)
{
    const QString backendName = m_backendCombo->currentText();
    m_overlayWidget->setStreamDiagnostics(backendName,
                                          m_overlayWidget->width(),
                                          m_overlayWidget->height(),
                                          fps,
                                          avgDecodeMs,
                                          0.0,
                                          0.0);
}

void VideoStreamTab::handleDeviceStatusUpdated(const PelcoD::DeviceStatus& status)
{
    m_overlayWidget->setDeviceStatus(status.connected, status.address);
    m_overlayWidget->setPanAngle(status.panDegrees());
    m_overlayWidget->setTiltAngle(status.tiltDegrees());

    // Approximate optical magnification calculation from raw 0..35999 zoom
    const double approxMag = 1.0 + (static_cast<double>(status.zoomPosition) / 65535.0) * 39.0;
    const double approxFocal = 35.0 + (static_cast<double>(status.zoomPosition) / 65535.0) * 765.0;
    m_overlayWidget->setZoomInfo(status.zoomPosition, approxMag, approxFocal);
}

void VideoStreamTab::handleFujinonStatusUpdated(const PelcoD::FujinonStatus& status)
{
    handleDeviceStatusUpdated(status.baseStatus);

    QString oisStr = "OFF";
    switch (status.oisMode) {
    case PelcoD::FujinonOISMode::Auto:   oisStr = "AUTO"; break;
    case PelcoD::FujinonOISMode::OisOn:  oisStr = "OIS ON"; break;
    case PelcoD::FujinonOISMode::EisOn:  oisStr = "EIS ON"; break;
    case PelcoD::FujinonOISMode::Off:   oisStr = "OFF"; break;
    }

    QString defogStr = "OFF";
    switch (status.defogLevel) {
    case PelcoD::FujinonDefogLevel::Level1: defogStr = "L1"; break;
    case PelcoD::FujinonDefogLevel::Level2: defogStr = "L2"; break;
    case PelcoD::FujinonDefogLevel::Level3: defogStr = "L3"; break;
    default: defogStr = "OFF"; break;
    }

    QString dnStr = "AUTO";
    switch (status.dayNightMode) {
    case PelcoD::FujinonDayNightMode::Day:   dnStr = "DAY [VIS]"; break;
    case PelcoD::FujinonDayNightMode::Night: dnStr = "NIGHT [IR]"; break;
    default: dnStr = "AUTO"; break;
    }

    m_overlayWidget->setOpticalStatus(
        (status.baseStatus.autoFocus == PelcoD::AutoMode::On) ? "AUTO" : "MANUAL",
        (status.baseStatus.autoIris == PelcoD::AutoMode::On) ? "AUTO" : "MANUAL",
        oisStr,
        defogStr,
        dnStr
    );
}

void VideoStreamTab::handleRttStatsUpdated(double avgRttMs, double jitterMs)
{
    m_overlayWidget->setStreamDiagnostics(m_backendCombo->currentText(),
                                          m_overlayWidget->width(),
                                          m_overlayWidget->height(),
                                          0.0,
                                          0.0,
                                          avgRttMs,
                                          jitterMs);
}

void VideoStreamTab::handleOverlayPanTiltRequested(int panSpeed, int tiltSpeed, bool left, bool right, bool up, bool down)
{
    if (!m_device) return;
    int panDir = 0;
    if (left) panDir = -1;
    else if (right) panDir = 1;

    int tiltDir = 0;
    if (up) tiltDir = 1;
    else if (down) tiltDir = -1;

    m_device->move(panDir, panSpeed, tiltDir, tiltSpeed);
}

void VideoStreamTab::handleOverlayStopRequested()
{
    if (!m_device) return;
    m_device->stopMotion();
}

void VideoStreamTab::handleOverlayZoomRequested(bool zoomIn)
{
    if (!m_device) return;
    if (zoomIn) {
        m_device->zoomTele();
    } else {
        m_device->zoomWide();
    }
}

void VideoStreamTab::toggleFullscreen()
{
    if (!m_isFullscreen) {
        m_isFullscreen = true;
        m_overlayWidget->setParent(nullptr);
        m_overlayWidget->showFullScreen();
    } else {
        m_isFullscreen = false;
        m_overlayWidget->showNormal();
        setupUi(); // Re-attach to layout
    }
}

} // namespace PelcoDApp
