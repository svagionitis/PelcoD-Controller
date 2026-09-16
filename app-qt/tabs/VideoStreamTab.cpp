#include "VideoStreamTab.h"
#include "DecoderFactory.h"
#include "DeviceEnumerator.h"

#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>

#if defined(PELCOD_HAS_FILTERS)
#include "VideoFilters.h"
#endif

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

    topLayout->addWidget(new QLabel(tr("Source:"), this));

    m_sourceTypeCombo = new QComboBox(this);
    m_sourceTypeCombo->addItem(tr("RTSP / Stream"), 0);
    m_sourceTypeCombo->addItem(tr("Video File"), 1);
    m_sourceTypeCombo->addItem(tr("Capture Device"), 2);
    m_sourceTypeCombo->addItem(tr("Test Pattern"), 3);
    topLayout->addWidget(m_sourceTypeCombo);

    // RTSP Container
    m_rtspContainer = new QWidget(this);
    auto* rtspLayout = new QHBoxLayout(m_rtspContainer);
    rtspLayout->setContentsMargins(0, 0, 0, 0);
    rtspLayout->setSpacing(4);
    m_sourceCombo = new QComboBox(m_rtspContainer);
    m_sourceCombo->setEditable(true);
    m_sourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_sourceCombo->addItem(QStringLiteral("rtsp://admin:admin@192.168.1.108:554/cam/realmonitor?channel=1&subtype=0"),
        tr("Preset: Fujinon SX800 RTSP"));
    m_sourceCombo->addItem(QStringLiteral("rtsp://192.168.1.100:554/stream1"), tr("Preset: Camera 1 RTSP"));
    rtspLayout->addWidget(m_sourceCombo);
    topLayout->addWidget(m_rtspContainer, 1);

    // File Container
    m_fileContainer = new QWidget(this);
    auto* fileLayout = new QHBoxLayout(m_fileContainer);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->setSpacing(4);
    m_filePathEdit = new QLineEdit(m_fileContainer);
    m_filePathEdit->setPlaceholderText(tr("Select local video file (.mp4, .mkv, .avi, .mov, etc.)..."));
    m_btnBrowseFile = new QPushButton(tr("Browse..."), m_fileContainer);
    m_chkLoopFile = new QCheckBox(tr("Loop"), m_fileContainer);
    m_chkLoopFile->setChecked(true);
    m_chkLoopFile->setToolTip(tr("Continuously loop video playback when reaching end of file"));
    fileLayout->addWidget(m_filePathEdit, 1);
    fileLayout->addWidget(m_btnBrowseFile);
    fileLayout->addWidget(m_chkLoopFile);
    topLayout->addWidget(m_fileContainer, 1);
    m_fileContainer->hide();

    // Device Container
    m_deviceContainer = new QWidget(this);
    auto* devLayout = new QHBoxLayout(m_deviceContainer);
    devLayout->setContentsMargins(0, 0, 0, 0);
    devLayout->setSpacing(4);
    m_deviceCombo = new QComboBox(m_deviceContainer);
    m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnRefreshDevices = new QPushButton(tr("Refresh"), m_deviceContainer);
    m_btnRefreshDevices->setToolTip(tr("Rescan hardware video capture devices"));
    devLayout->addWidget(m_deviceCombo, 1);
    devLayout->addWidget(m_btnRefreshDevices);
    topLayout->addWidget(m_deviceContainer, 1);
    m_deviceContainer->hide();

    populateCaptureDevices();

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
    m_btnConnect->setStyleSheet("QPushButton { font-weight: bold; background-color: #2E7D32; color: white; padding: "
                                "4px 12px; border-radius: 3px; }"
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

#if defined(PELCOD_HAS_FILTERS)
    // Group: Tactical Vision & Image Enhancement
    auto* visionGroup = new QGroupBox(tr("Tactical Image Enhancement"), sidePanel);
    auto* visionLayout = new QVBoxLayout(visionGroup);
    visionLayout->setSpacing(4);

    visionLayout->addWidget(new QLabel(tr("Thermal / False Color:"), visionGroup));
    m_comboPalette = new QComboBox(visionGroup);
    m_comboPalette->addItem(tr("Off (Natural Colors)"), -1);
    m_comboPalette->addItem(tr("White Hot (Grayscale)"), static_cast<int>(PelcoD::Video::FalseColorPalette::WhiteHot));
    m_comboPalette->addItem(tr("Black Hot (Inverted)"), static_cast<int>(PelcoD::Video::FalseColorPalette::BlackHot));
    m_comboPalette->addItem(tr("Iron256 (Thermal Iron)"), static_cast<int>(PelcoD::Video::FalseColorPalette::Iron256));
    m_comboPalette->addItem(tr("Jet (Rainbow Spectrum)"), static_cast<int>(PelcoD::Video::FalseColorPalette::Jet));
    m_comboPalette->addItem(
        tr("Turbo (High Dynamic Range)"), static_cast<int>(PelcoD::Video::FalseColorPalette::Turbo));
    m_comboPalette->addItem(tr("Rainbow"), static_cast<int>(PelcoD::Video::FalseColorPalette::Rainbow));
    m_comboPalette->addItem(tr("Hot-Cold"), static_cast<int>(PelcoD::Video::FalseColorPalette::HotCold));
    m_comboPalette->addItem(tr("Ice-Fire"), static_cast<int>(PelcoD::Video::FalseColorPalette::IceFire));
    m_comboPalette->addItem(tr("Bone"), static_cast<int>(PelcoD::Video::FalseColorPalette::Bone));
    visionLayout->addWidget(m_comboPalette);

    m_chkDcpDehaze = new QCheckBox(tr("Atmospheric Dehaze (DCP)"), visionGroup);
    visionLayout->addWidget(m_chkDcpDehaze);

    m_chkStabilizer = new QCheckBox(tr("Electronic Stabilization (EIS)"), visionGroup);
    visionLayout->addWidget(m_chkStabilizer);

    m_chkWhiteBalance = new QCheckBox(tr("Auto White Balance (AWB)"), visionGroup);
    visionLayout->addWidget(m_chkWhiteBalance);

    m_chkChromaticAberration = new QCheckBox(tr("Chromatic Aberration Fix"), visionGroup);
    visionLayout->addWidget(m_chkChromaticAberration);

    m_chkLapHaze = new QCheckBox(tr("Fog / Haze Penetration (LAP)"), visionGroup);
    visionLayout->addWidget(m_chkLapHaze);

    m_chkClahe = new QCheckBox(tr("Adaptive Contrast (CLAHE)"), visionGroup);
    visionLayout->addWidget(m_chkClahe);

    m_chkDenoise = new QCheckBox(tr("Heat Shimmer Denoise"), visionGroup);
    visionLayout->addWidget(m_chkDenoise);

    m_chkSharpen = new QCheckBox(tr("Acuity Sharpening"), visionGroup);
    visionLayout->addWidget(m_chkSharpen);

    m_chkEdgeDetect = new QCheckBox(tr("Canny Edge Outlines"), visionGroup);
    visionLayout->addWidget(m_chkEdgeDetect);

    // Tactical & Thermal Analytics Sub-Panel
    visionLayout->addWidget(new QLabel(tr("Tactical & Thermal Analytics:"), visionGroup));
    auto* isoLayout = new QHBoxLayout();
    m_chkIsotherm = new QCheckBox(tr("Isotherm"), visionGroup);
    m_comboIsothermPreset = new QComboBox(visionGroup);
    m_comboIsothermPreset->addItem(tr("Body Heat"), static_cast<int>(PelcoD::Video::IsothermFilter::Preset::HumanBody));
    m_comboIsothermPreset->addItem(tr("High Heat"), static_cast<int>(PelcoD::Video::IsothermFilter::Preset::HighHeat));
    m_comboIsothermPreset->addItem(
        tr("Custom (140-180)"), static_cast<int>(PelcoD::Video::IsothermFilter::Preset::Custom));
    isoLayout->addWidget(m_chkIsotherm);
    isoLayout->addWidget(m_comboIsothermPreset);
    visionLayout->addLayout(isoLayout);

    m_chkHotspotTracker = new QCheckBox(tr("Hotspot & Radiometry"), visionGroup);
    visionLayout->addWidget(m_chkHotspotTracker);

    m_chkMtiMotion = new QCheckBox(tr("Moving Target (MTI)"), visionGroup);
    visionLayout->addWidget(m_chkMtiMotion);

    auto* reticleLayout = new QHBoxLayout();
    m_chkReticleHud = new QCheckBox(tr("Reticle HUD"), visionGroup);
    m_comboReticleStyle = new QComboBox(visionGroup);
    m_comboReticleStyle->addItem(
        tr("Crosshair"), static_cast<int>(PelcoD::Video::TacticalReticleOverlayFilter::Style::Crosshair));
    m_comboReticleStyle->addItem(
        tr("Mil-Dot"), static_cast<int>(PelcoD::Video::TacticalReticleOverlayFilter::Style::MilDot));
    m_comboReticleStyle->addItem(
        tr("Stadiametric"), static_cast<int>(PelcoD::Video::TacticalReticleOverlayFilter::Style::Stadiametric));
    m_comboReticleStyle->addItem(
        tr("Corner Brackets"), static_cast<int>(PelcoD::Video::TacticalReticleOverlayFilter::Style::CornerBrackets));
    reticleLayout->addWidget(m_chkReticleHud);
    reticleLayout->addWidget(m_comboReticleStyle);
    visionLayout->addLayout(reticleLayout);

    // Motion & Target Tracking Sub-Panel
    visionLayout->addWidget(new QLabel(tr("Motion & Target Tracking:"), visionGroup));
    m_chkHeatmap = new QCheckBox(tr("Activity Heatmap"), visionGroup);
    visionLayout->addWidget(m_chkHeatmap);

    auto* flowLayout = new QHBoxLayout();
    m_chkOpticalFlow = new QCheckBox(tr("Motion Flow"), visionGroup);
    m_comboFlowMode = new QComboBox(visionGroup);
    m_comboFlowMode->addItem(
        tr("Vector Arrows"), static_cast<int>(PelcoD::Video::OpticalFlowFieldFilter::DisplayMode::VectorArrows));
    m_comboFlowMode->addItem(
        tr("Color Flow"), static_cast<int>(PelcoD::Video::OpticalFlowFieldFilter::DisplayMode::ColorFlow));
    flowLayout->addWidget(m_chkOpticalFlow);
    flowLayout->addWidget(m_comboFlowMode);
    visionLayout->addLayout(flowLayout);

    m_chkTargetLock = new QCheckBox(tr("Visual Target Lock"), visionGroup);
    visionLayout->addWidget(m_chkTargetLock);

    auto* tripLayout = new QHBoxLayout();
    m_chkTripwire = new QCheckBox(tr("Perimeter Tripwire"), visionGroup);
    m_comboTripwireDir = new QComboBox(visionGroup);
    m_comboTripwireDir->addItem(
        tr("Bi-directional"), static_cast<int>(PelcoD::Video::PerimeterTripwireFilter::Direction::Bidirectional));
    m_comboTripwireDir->addItem(
        tr("A -> B"), static_cast<int>(PelcoD::Video::PerimeterTripwireFilter::Direction::A_to_B));
    m_comboTripwireDir->addItem(
        tr("B -> A"), static_cast<int>(PelcoD::Video::PerimeterTripwireFilter::Direction::B_to_A));
    tripLayout->addWidget(m_chkTripwire);
    tripLayout->addWidget(m_comboTripwireDir);
    visionLayout->addLayout(tripLayout);

    // Privacy & Operational Overlays Sub-Panel
    visionLayout->addWidget(new QLabel(tr("Privacy & Operational Overlays:"), visionGroup));

    auto* privLayout = new QHBoxLayout();
    m_chkPrivacyMask = new QCheckBox(tr("Privacy Mask"), visionGroup);
    m_comboPrivacyMode = new QComboBox(visionGroup);
    m_comboPrivacyMode->addItem(
        tr("Blackout"), static_cast<int>(PelcoD::Video::PrivacyMaskFilter::ConcealmentMode::Blackout));
    m_comboPrivacyMode->addItem(tr("Blur"), static_cast<int>(PelcoD::Video::PrivacyMaskFilter::ConcealmentMode::Blur));
    m_comboPrivacyMode->addItem(
        tr("Mosaic"), static_cast<int>(PelcoD::Video::PrivacyMaskFilter::ConcealmentMode::Mosaic));
    privLayout->addWidget(m_chkPrivacyMask);
    privLayout->addWidget(m_comboPrivacyMode);
    visionLayout->addLayout(privLayout);

    m_chkForensicWatermark = new QCheckBox(tr("Forensic Watermark"), visionGroup);
    visionLayout->addWidget(m_chkForensicWatermark);

    m_chkTelemetryOsd = new QCheckBox(tr("Telemetry HUD"), visionGroup);
    visionLayout->addWidget(m_chkTelemetryOsd);

    auto* pipLayout = new QHBoxLayout();
    m_chkPictureInPicture = new QCheckBox(tr("Picture-in-Picture"), visionGroup);
    m_comboPipMode = new QComboBox(visionGroup);
    m_comboPipMode->addItem(
        tr("Digital Zoom (2x)"), static_cast<int>(PelcoD::Video::PictureInPictureFilter::Mode::DigitalZoom));
    m_comboPipMode->addItem(
        tr("Aux Feed"), static_cast<int>(PelcoD::Video::PictureInPictureFilter::Mode::SecondaryFeed));
    pipLayout->addWidget(m_chkPictureInPicture);
    pipLayout->addWidget(m_comboPipMode);
    visionLayout->addLayout(pipLayout);

    sideLayout->addWidget(visionGroup);
#endif

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
    m_statusLabel
        = new QLabel(tr("Status: Idle. Enter an RTSP URL or choose Mock Test Pattern and press Connect."), this);
    m_statusLabel->setStyleSheet("color: #90A4AE; font-size: 11px; padding: 2px 4px;");
    mainLayout->addWidget(m_statusLabel);

    // Connect D-Pad buttons
    auto getSpeed = [this]() -> std::uint8_t { return static_cast<std::uint8_t>(m_speedSlider->value()); };

    connect(btnUp, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->tiltUp(getSpeed());
    });
    connect(btnDown, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->tiltDown(getSpeed());
    });
    connect(btnLeft, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->panLeft(getSpeed());
    });
    connect(btnRight, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->panRight(getSpeed());
    });

    connect(btnUpLeft, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->move(-1, getSpeed(), 1, getSpeed());
    });
    connect(btnUpRight, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->move(1, getSpeed(), 1, getSpeed());
    });
    connect(btnDownLeft, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->move(-1, getSpeed(), -1, getSpeed());
    });
    connect(btnDownRight, &QPushButton::pressed, this, [this, getSpeed]() {
        if (m_device)
            m_device->move(1, getSpeed(), -1, getSpeed());
    });

    connect(btnStop, &QPushButton::clicked, this, [this]() {
        if (m_device)
            m_device->stopMotion();
    });
    connect(btnZoomIn, &QPushButton::pressed, this, [this]() {
        if (m_device)
            m_device->zoomTele();
    });
    connect(btnZoomIn, &QPushButton::released, this, [this]() {
        if (m_device)
            m_device->zoomStop();
    });
    connect(btnZoomOut, &QPushButton::pressed, this, [this]() {
        if (m_device)
            m_device->zoomWide();
    });
    connect(btnZoomOut, &QPushButton::released, this, [this]() {
        if (m_device)
            m_device->zoomStop();
    });

    connect(
        m_speedSlider, &QSlider::valueChanged, this, [this](int val) { m_speedLabel->setText(QString::number(val)); });

    // F11 Shortcut for fullscreen toggle
    auto* f11Shortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(f11Shortcut, &QShortcut::activated, this, &VideoStreamTab::toggleFullscreen);
}

void VideoStreamTab::setupConnections()
{
    // Source selection controls
    connect(m_sourceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onSourceTypeChanged);
    connect(m_btnBrowseFile, &QPushButton::clicked, this, &VideoStreamTab::onBrowseFileClicked);
    connect(m_btnRefreshDevices, &QPushButton::clicked, this, &VideoStreamTab::onRefreshDevicesClicked);
    connect(m_chkLoopFile, &QCheckBox::toggled, this, &VideoStreamTab::onLoopFileToggled);

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
    connect(m_comboColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onColorSchemeChanged);

#if defined(PELCOD_HAS_FILTERS)
    connect(m_comboPalette, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkDcpDehaze, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkStabilizer, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkWhiteBalance, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkChromaticAberration, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkLapHaze, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkClahe, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkDenoise, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkSharpen, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkEdgeDetect, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkIsotherm, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_comboIsothermPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkHotspotTracker, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkMtiMotion, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkReticleHud, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_comboReticleStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkHeatmap, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkOpticalFlow, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_comboFlowMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkTargetLock, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkTripwire, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_comboTripwireDir, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkPrivacyMask, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_comboPrivacyMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkForensicWatermark, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkTelemetryOsd, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_chkPictureInPicture, &QCheckBox::toggled, this, &VideoStreamTab::onFilterConfigurationChanged);
    connect(m_comboPipMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &VideoStreamTab::onFilterConfigurationChanged);
#endif

    // Worker signals to Overlay Widget
    connect(
        m_worker, &PelcoD::Video::QVideoStreamWorker::frameReady, m_overlayWidget, &VideoOverlayWidget::updateFrame);
    connect(m_worker, &PelcoD::Video::QVideoStreamWorker::streamStatusChanged, this,
        &VideoStreamTab::onWorkerStatusChanged);
    connect(m_worker, &PelcoD::Video::QVideoStreamWorker::statsUpdated, this, &VideoStreamTab::onWorkerStatsUpdated);

    // Interactive Joystick signals from Overlay to Device
    connect(
        m_overlayWidget, &VideoOverlayWidget::panTiltRequested, this, &VideoStreamTab::handleOverlayPanTiltRequested);
    connect(m_overlayWidget, &VideoOverlayWidget::stopPtzRequested, this, &VideoStreamTab::handleOverlayStopRequested);
    connect(m_overlayWidget, &VideoOverlayWidget::zoomRequested, this, &VideoStreamTab::handleOverlayZoomRequested);
}

void VideoStreamTab::onSourceTypeChanged(int index)
{
    m_rtspContainer->setVisible(index == 0);
    m_fileContainer->setVisible(index == 1);
    m_deviceContainer->setVisible(index == 2);
}

void VideoStreamTab::onBrowseFileClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Open Video File"), QString(),
        tr("Video Files (*.mp4 *.mkv *.avi *.mov *.webm *.ts *.flv *.m4v);;All Files (*.*)"));

    if (!filePath.isEmpty()) {
        m_filePathEdit->setText(filePath);
    }
}

void VideoStreamTab::onRefreshDevicesClicked()
{
    populateCaptureDevices();
}

void VideoStreamTab::onLoopFileToggled(bool checked)
{
    if (m_worker != nullptr) {
        m_worker->setLoopPlayback(checked);
    }
}

void VideoStreamTab::populateCaptureDevices()
{
    m_deviceCombo->clear();
    const auto devices = PelcoD::Video::DeviceEnumerator::enumerateDevices();
    for (const auto& dev : devices) {
        m_deviceCombo->addItem(QString::fromStdString(dev.name), QString::fromStdString(dev.path));
    }

    if (m_deviceCombo->count() == 0) {
#ifdef _WIN32
        m_deviceCombo->addItem(
            tr("Default Windows Camera (video=Integrated Camera)"), QStringLiteral("video=Integrated Camera"));
        m_deviceCombo->addItem(tr("Generic DirectShow (video=default)"), QStringLiteral("video=default"));
#else
        m_deviceCombo->addItem(tr("Primary V4L2 Device (/dev/video0)"), QStringLiteral("/dev/video0"));
#endif
    }
}

void VideoStreamTab::onConnectClicked()
{
    QString source;
    const int typeIdx = m_sourceTypeCombo->currentIndex();

    if (typeIdx == 0) { // RTSP
        source = m_sourceCombo->currentText().trimmed();
        if (source.isEmpty()) {
            QMessageBox::warning(
                this, tr("Invalid Source"), tr("Please enter a valid RTSP stream URL or select a preset."));
            return;
        }
    } else if (typeIdx == 1) { // File
        source = m_filePathEdit->text().trimmed();
        if (source.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Source"), tr("Please select or enter a valid video file path."));
            return;
        }
    } else if (typeIdx == 2) { // Device
        source = m_deviceCombo->currentData().toString();
        if (source.isEmpty()) {
            source = m_deviceCombo->currentText().trimmed();
        }
        if (source.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Source"), tr("Please select a valid hardware capture device."));
            return;
        }
    } else { // Mock
        source = QStringLiteral("mock://smpte-bars");
    }

    const auto backend = static_cast<PelcoD::Video::BackendType>(m_backendCombo->currentData().toInt());

    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    m_sourceTypeCombo->setEnabled(false);
    m_sourceCombo->setEnabled(false);
    m_filePathEdit->setEnabled(false);
    m_btnBrowseFile->setEnabled(false);
    m_deviceCombo->setEnabled(false);
    m_btnRefreshDevices->setEnabled(false);
    m_backendCombo->setEnabled(false);

    m_worker->setLoopPlayback(m_chkLoopFile->isChecked());
    m_worker->openStream(source, backend, PelcoD::Video::DeviceType::CPU);
#if defined(PELCOD_HAS_FILTERS)
    onFilterConfigurationChanged();
#endif
}

void VideoStreamTab::onDisconnectClicked()
{
    m_worker->stopPlayback();
    m_overlayWidget->clearFrame();

    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    m_sourceTypeCombo->setEnabled(true);
    m_sourceCombo->setEnabled(true);
    m_filePathEdit->setEnabled(true);
    m_btnBrowseFile->setEnabled(true);
    m_deviceCombo->setEnabled(true);
    m_btnRefreshDevices->setEnabled(true);
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

    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/PelcoD_Snapshot_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Video Snapshot"), defaultPath, tr("PNG Image (*.png);;JPEG Image (*.jpg)"));
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
    m_overlayWidget->setStreamDiagnostics(
        backendName, m_overlayWidget->width(), m_overlayWidget->height(), fps, avgDecodeMs, 0.0, 0.0);
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
    case PelcoD::FujinonOISMode::Auto:
        oisStr = "AUTO";
        break;
    case PelcoD::FujinonOISMode::OisOn:
        oisStr = "OIS ON";
        break;
    case PelcoD::FujinonOISMode::EisOn:
        oisStr = "EIS ON";
        break;
    case PelcoD::FujinonOISMode::Off:
        oisStr = "OFF";
        break;
    }

    QString defogStr = "OFF";
    switch (status.defogLevel) {
    case PelcoD::FujinonDefogLevel::Level1:
        defogStr = "L1";
        break;
    case PelcoD::FujinonDefogLevel::Level2:
        defogStr = "L2";
        break;
    case PelcoD::FujinonDefogLevel::Level3:
        defogStr = "L3";
        break;
    default:
        defogStr = "OFF";
        break;
    }

    QString dnStr = "AUTO";
    switch (status.dayNightMode) {
    case PelcoD::FujinonDayNightMode::Day:
        dnStr = "DAY [VIS]";
        break;
    case PelcoD::FujinonDayNightMode::Night:
        dnStr = "NIGHT [IR]";
        break;
    default:
        dnStr = "AUTO";
        break;
    }

    m_overlayWidget->setOpticalStatus((status.baseStatus.autoFocus == PelcoD::AutoMode::On) ? "AUTO" : "MANUAL",
        (status.baseStatus.autoIris == PelcoD::AutoMode::On) ? "AUTO" : "MANUAL", oisStr, defogStr, dnStr);
}

void VideoStreamTab::handleRttStatsUpdated(double avgRttMs, double jitterMs)
{
    m_overlayWidget->setStreamDiagnostics(m_backendCombo->currentText(), m_overlayWidget->width(),
        m_overlayWidget->height(), 0.0, 0.0, avgRttMs, jitterMs);
}

void VideoStreamTab::handleOverlayPanTiltRequested(
    int panSpeed, int tiltSpeed, bool left, bool right, bool up, bool down)
{
    if (!m_device)
        return;
    int panDir = 0;
    if (left)
        panDir = -1;
    else if (right)
        panDir = 1;

    int tiltDir = 0;
    if (up)
        tiltDir = 1;
    else if (down)
        tiltDir = -1;

    m_device->move(panDir, panSpeed, tiltDir, tiltSpeed);
}

void VideoStreamTab::handleOverlayStopRequested()
{
    if (!m_device)
        return;
    m_device->stopMotion();
}

void VideoStreamTab::handleOverlayZoomRequested(bool zoomIn)
{
    if (!m_device)
        return;
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

#if defined(PELCOD_HAS_FILTERS)
void VideoStreamTab::onFilterConfigurationChanged()
{
    if (m_worker == nullptr) {
        return;
    }

    m_worker->clearFrameProcessors();

    // 0. Electronic Image Stabilization (EIS) - applied first on incoming raw frame
    if (m_chkStabilizer != nullptr && m_chkStabilizer->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::ImageStabilizationFilter>(0.8, 30.0, 0.04));
    }

    // 1. Denoise to suppress scintillation before edge/contrast amplification
    if (m_chkDenoise != nullptr && m_chkDenoise->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::TemporalDenoiseFilter>(0.5, 30.0));
    }

    // 2. Dark Channel Prior (DCP) Dehaze - removes atmospheric haze/fog
    if (m_chkDcpDehaze != nullptr && m_chkDcpDehaze->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::DarkChannelDehazeFilter>(0.85, 9, 0.1));
    }

    // 3. Auto White Balance (AWB) - corrects illumination color casts
    if (m_chkWhiteBalance != nullptr && m_chkWhiteBalance->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::WhiteBalanceFilter>(
            PelcoD::Video::WhiteBalanceFilter::Mode::GrayWorld, 1.0));
    }

    // 4. Chromatic Aberration Correction - fixes radial color fringing at extreme zoom
    if (m_chkChromaticAberration != nullptr && m_chkChromaticAberration->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::ChromaticAberrationFilter>(0.005, -0.005));
    }

    // 5. Atmospheric Penetration / LAP
    if (m_chkLapHaze != nullptr && m_chkLapHaze->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::LocalAreaProcessingFilter>(5, 0.6, 5.0));
    }

    // 6. CLAHE Adaptive Contrast
    if (m_chkClahe != nullptr && m_chkClahe->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::ClaheFilter>(2.5, 8, 1.0));
    }

    // 7. Optical Acuity Sharpening
    if (m_chkSharpen != nullptr && m_chkSharpen->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::SharpenFilter>(1.2, 3));
    }

    // 8. Canny Edge Outlines
    if (m_chkEdgeDetect != nullptr && m_chkEdgeDetect->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::EdgeDetectionFilter>(50.0, 150.0));
    }

    // 9. Isotherm Thermal Slicing
    if (m_chkIsotherm != nullptr && m_chkIsotherm->isChecked()) {
        auto iso = std::make_shared<PelcoD::Video::IsothermFilter>();
        if (m_comboIsothermPreset != nullptr) {
            const auto preset
                = static_cast<PelcoD::Video::IsothermFilter::Preset>(m_comboIsothermPreset->currentData().toInt());
            iso->setPreset(preset);
        }
        m_worker->addFrameProcessor(iso);
    }

    // 10. Thermal / False Color Palette (applied across color mapped result)
    if (m_comboPalette != nullptr && m_comboPalette->currentIndex() > 0) {
        const auto palette = static_cast<PelcoD::Video::FalseColorPalette>(m_comboPalette->currentData().toInt());
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::FalseColorFilter>(palette));
    }

    // 11. Moving Target Indication (MTI) - target acquisition brackets
    if (m_chkMtiMotion != nullptr && m_chkMtiMotion->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::MovingTargetIndicatorFilter>(80, 50000, 16));
    }

    // 12. Hotspot & Spot Radiometry Tracker
    if (m_chkHotspotTracker != nullptr && m_chkHotspotTracker->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::HotspotTrackerFilter>(true, 32));
    }

    // 13. Tactical Reticle HUD Overlay (drawn on top of all image layers)
    if (m_chkReticleHud != nullptr && m_chkReticleHud->isChecked()) {
        auto style = PelcoD::Video::TacticalReticleOverlayFilter::Style::Crosshair;
        if (m_comboReticleStyle != nullptr) {
            style = static_cast<PelcoD::Video::TacticalReticleOverlayFilter::Style>(
                m_comboReticleStyle->currentData().toInt());
        }
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::TacticalReticleOverlayFilter>(
            style, PelcoD::Video::TacticalReticleOverlayFilter::Color::TacticalGreen, 1, 14));
    }

    // 14. Motion Activity Heatmap
    if (m_chkHeatmap != nullptr && m_chkHeatmap->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::MotionHeatmapFilter>(0.95, 0.40, 20));
    }

    // 15. Optical Flow Motion Field
    if (m_chkOpticalFlow != nullptr && m_chkOpticalFlow->isChecked()) {
        auto mode = PelcoD::Video::OpticalFlowFieldFilter::DisplayMode::VectorArrows;
        if (m_comboFlowMode != nullptr) {
            mode = static_cast<PelcoD::Video::OpticalFlowFieldFilter::DisplayMode>(
                m_comboFlowMode->currentData().toInt());
        }
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::OpticalFlowFieldFilter>(mode, 16, 1.5, 2.0));
    }

    // 16. Visual Target Lock-On & Boresight Offset Tracker
    if (m_chkTargetLock != nullptr && m_chkTargetLock->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::CentroidTargetTrackerFilter>(true, 40, 40));
    }

    // 17. Perimeter Tripwire Intrusion Detection
    if (m_chkTripwire != nullptr && m_chkTripwire->isChecked()) {
        auto dir = PelcoD::Video::PerimeterTripwireFilter::Direction::Bidirectional;
        if (m_comboTripwireDir != nullptr) {
            dir = static_cast<PelcoD::Video::PerimeterTripwireFilter::Direction>(
                m_comboTripwireDir->currentData().toInt());
        }
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::PerimeterTripwireFilter>(0.1, 0.5, 0.9, 0.5, dir));
    }

    // 18. Privacy Masking (censors private property / windows before operational HUD)
    if (m_chkPrivacyMask != nullptr && m_chkPrivacyMask->isChecked()) {
        auto mode = PelcoD::Video::PrivacyMaskFilter::ConcealmentMode::Blackout;
        if (m_comboPrivacyMode != nullptr) {
            mode = static_cast<PelcoD::Video::PrivacyMaskFilter::ConcealmentMode>(
                m_comboPrivacyMode->currentData().toInt());
        }
        auto privacy = std::make_shared<PelcoD::Video::PrivacyMaskFilter>(mode);
        privacy->addZone(0.05, 0.05, 0.25, 0.20, mode, "Restricted Zone 1");
        privacy->addZone(0.70, 0.10, 0.22, 0.25, mode, "Restricted Zone 2");
        m_worker->addFrameProcessor(privacy);
    }

    // 19. Picture-in-Picture (PiP) Inset (Electronic Zoom or Secondary Feed)
    if (m_chkPictureInPicture != nullptr && m_chkPictureInPicture->isChecked()) {
        auto mode = PelcoD::Video::PictureInPictureFilter::Mode::DigitalZoom;
        if (m_comboPipMode != nullptr) {
            mode = static_cast<PelcoD::Video::PictureInPictureFilter::Mode>(m_comboPipMode->currentData().toInt());
        }
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::PictureInPictureFilter>(
            mode, PelcoD::Video::PictureInPictureFilter::Corner::TopRight, 0.28, 2.0));
    }

    // 20. Operational Telemetry OSD (PTZ angles, compass heading, FOV)
    if (m_chkTelemetryOsd != nullptr && m_chkTelemetryOsd->isChecked()) {
        auto telem = std::make_shared<PelcoD::Video::TelemetryOsdFilter>(
            PelcoD::Video::TelemetryOsdFilter::Color::TacticalGreen, true, true);
        PelcoD::Video::TelemetryOsdFilter::TelemetryData telemData;
        telemData.panDegrees = 184.5;
        telemData.tiltDegrees = -12.3;
        telemData.zoomMagnification = 25.0;
        telemData.horizontalFovDegrees = 2.4;
        telemData.sensorPayload = "OPTICAL HD";
        telemData.statusMessage = "LINK: OK";
        telem->setTelemetry(telemData);
        m_worker->addFrameProcessor(telem);
    }

    // 21. Forensic Timestamp & Evidentiary Watermark (burned into top layer)
    if (m_chkForensicWatermark != nullptr && m_chkForensicWatermark->isChecked()) {
        m_worker->addFrameProcessor(std::make_shared<PelcoD::Video::TimestampWatermarkFilter>(
            PelcoD::Video::TimestampWatermarkFilter::Position::TopLeft, "CAM-01 [PTZ]", true, true));
    }
}
#endif

} // namespace PelcoDApp
