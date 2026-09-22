/// @file PtzControlTab.cpp
/// @brief Implementation of interactive PTZ control dashboard tab.

#include "PtzControlTab.h"

namespace PelcoDApp {

PtzControlTab::PtzControlTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
}

void PtzControlTab::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // Left Column: Directional Pad & Speeds
    auto* dpadGroup = new QGroupBox(tr("Pan / Tilt Directional Control"), this);
    auto* dpadMainLayout = new QVBoxLayout(dpadGroup);

    auto* dpadGrid = new QGridLayout();
    createDpad(dpadGrid);
    dpadMainLayout->addLayout(dpadGrid);

    // Speed Controls
    auto* speedLayout = new QVBoxLayout();
    auto* panSpeedHeader = new QHBoxLayout();
    panSpeedHeader->addWidget(new QLabel(tr("Pan Speed:")));
    lblPanSpeedVal = new QLabel("32");
    lblPanSpeedVal->setStyleSheet("font-weight: bold; color: #58a6ff;");
    panSpeedHeader->addWidget(lblPanSpeedVal);
    panSpeedHeader->addStretch();
    chkTurboPan = new QCheckBox(tr("Turbo (0x40)"));
    panSpeedHeader->addWidget(chkTurboPan);

    sliderPanSpeed = new QSlider(Qt::Horizontal);
    sliderPanSpeed->setRange(0, 63);
    sliderPanSpeed->setValue(32);

    auto* tiltSpeedHeader = new QHBoxLayout();
    tiltSpeedHeader->addWidget(new QLabel(tr("Tilt Speed:")));
    lblTiltSpeedVal = new QLabel("32");
    lblTiltSpeedVal->setStyleSheet("font-weight: bold; color: #58a6ff;");
    tiltSpeedHeader->addWidget(lblTiltSpeedVal);
    tiltSpeedHeader->addStretch();

    sliderTiltSpeed = new QSlider(Qt::Horizontal);
    sliderTiltSpeed->setRange(0, 63);
    sliderTiltSpeed->setValue(32);

    speedLayout->addLayout(panSpeedHeader);
    speedLayout->addWidget(sliderPanSpeed);
    speedLayout->addLayout(tiltSpeedHeader);
    speedLayout->addWidget(sliderTiltSpeed);

    dpadMainLayout->addLayout(speedLayout);

    // Quick Actions
    auto* quickActions = new QHBoxLayout();
    auto* btnZero = new QPushButton(tr("Zero Pan (0°)"));
    auto* btnFlip = new QPushButton(tr("Flip 180°"));
    auto* btnSetZeroPos = new QPushButton(tr("Set Hardware Zero"));
    btnSetZeroPos->setToolTip(tr("Calibrate azimuth zero reference at current position (opcode 0x49)"));
    quickActions->addWidget(btnZero);
    quickActions->addWidget(btnFlip);
    quickActions->addWidget(btnSetZeroPos);
    dpadMainLayout->addLayout(quickActions);

    mainLayout->addWidget(dpadGroup, 1);

    // Middle Column: Optics (Zoom, Focus, Iris)
    auto* opticsGroup = new QGroupBox(tr("Lens & Optics Controls"), this);
    auto* opticsLayout = new QVBoxLayout(opticsGroup);
    createOptics(opticsLayout);
    mainLayout->addWidget(opticsGroup, 1);

    // Right Column: Coordinates & Absolute Positioning
    auto* posGroup = new QGroupBox(tr("Telemetry & Positioning"), this);
    auto* posLayout = new QVBoxLayout(posGroup);
    createPositioning(posLayout);
    mainLayout->addWidget(posGroup, 1);

    // Speed slider label updates
    connect(sliderPanSpeed, &QSlider::valueChanged, this,
        [this](int val) { lblPanSpeedVal->setText(QString::number(val)); });
    connect(sliderTiltSpeed, &QSlider::valueChanged, this,
        [this](int val) { lblTiltSpeedVal->setText(QString::number(val)); });

    // Quick action connections
    connect(btnZero, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::zeroPan);
    connect(btnFlip, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::flip180);
    connect(btnSetZeroPos, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::setZeroPosition);
}

void PtzControlTab::createDpad(QGridLayout* layout)
{
    btnUpLeft = new QPushButton(QString::fromUtf8("◤"));
    btnUp = new QPushButton(QString::fromUtf8("▲"));
    btnUpRight = new QPushButton(QString::fromUtf8("◥"));
    btnLeft = new QPushButton(QString::fromUtf8("◀"));
    btnStop = new QPushButton(QString::fromUtf8("■"));
    btnRight = new QPushButton(QString::fromUtf8("▶"));
    btnDownLeft = new QPushButton(QString::fromUtf8("◣"));
    btnDown = new QPushButton(QString::fromUtf8("▼"));
    btnDownRight = new QPushButton(QString::fromUtf8("◢"));

    const auto buttons
        = { btnUpLeft, btnUp, btnUpRight, btnLeft, btnStop, btnRight, btnDownLeft, btnDown, btnDownRight };
    for (auto* btn : buttons) {
        btn->setObjectName("btnDpad");
        btn->setFixedSize(56, 56);
    }
    btnStop->setObjectName("btnDanger");

    layout->addWidget(btnUpLeft, 0, 0);
    layout->addWidget(btnUp, 0, 1);
    layout->addWidget(btnUpRight, 0, 2);
    layout->addWidget(btnLeft, 1, 0);
    layout->addWidget(btnStop, 1, 1);
    layout->addWidget(btnRight, 1, 2);
    layout->addWidget(btnDownLeft, 2, 0);
    layout->addWidget(btnDown, 2, 1);
    layout->addWidget(btnDownRight, 2, 2);

    // Motion Press Handlers:
    // Pan: Left = 0x04, Right = 0x02
    // Tilt: Up = 0x08, Down = 0x10
    connect(btnUp, &QPushButton::pressed, this, [this] { handleDpadPressed(0x00, 0x08); });
    connect(btnDown, &QPushButton::pressed, this, [this] { handleDpadPressed(0x00, 0x10); });
    connect(btnLeft, &QPushButton::pressed, this, [this] { handleDpadPressed(0x04, 0x00); });
    connect(btnRight, &QPushButton::pressed, this, [this] { handleDpadPressed(0x02, 0x00); });
    connect(btnUpLeft, &QPushButton::pressed, this, [this] { handleDpadPressed(0x04, 0x08); });
    connect(btnUpRight, &QPushButton::pressed, this, [this] { handleDpadPressed(0x02, 0x08); });
    connect(btnDownLeft, &QPushButton::pressed, this, [this] { handleDpadPressed(0x04, 0x10); });
    connect(btnDownRight, &QPushButton::pressed, this, [this] { handleDpadPressed(0x02, 0x10); });

    // Stop button click & Release Handlers
    connect(btnStop, &QPushButton::clicked, this, &PtzControlTab::handleDpadReleased);

    for (auto* btn : { btnUp, btnDown, btnLeft, btnRight, btnUpLeft, btnUpRight, btnDownLeft, btnDownRight }) {
        connect(btn, &QPushButton::released, this, &PtzControlTab::handleDpadReleased);
    }
}

void PtzControlTab::createOptics(QVBoxLayout* layout)
{
    // Zoom
    auto* zoomGroup = new QGroupBox(tr("Zoom"), this);
    auto* zoomLayout = new QHBoxLayout(zoomGroup);
    btnZoomTele = new QPushButton(tr("Tele (In)"));
    btnZoomWide = new QPushButton(tr("Wide (Out)"));
    zoomLayout->addWidget(btnZoomTele);
    zoomLayout->addWidget(btnZoomWide);
    layout->addWidget(zoomGroup);

    // Focus
    auto* focusGroup = new QGroupBox(tr("Focus"), this);
    auto* focusLayout = new QHBoxLayout(focusGroup);
    btnFocusNear = new QPushButton(tr("Near"));
    btnFocusFar = new QPushButton(tr("Far"));
    focusLayout->addWidget(btnFocusNear);
    focusLayout->addWidget(btnFocusFar);
    layout->addWidget(focusGroup);

    // Iris
    auto* irisGroup = new QGroupBox(tr("Iris"), this);
    auto* irisLayout = new QHBoxLayout(irisGroup);
    btnIrisOpen = new QPushButton(tr("Open"));
    btnIrisClose = new QPushButton(tr("Close"));
    irisLayout->addWidget(btnIrisOpen);
    irisLayout->addWidget(btnIrisClose);
    layout->addWidget(irisGroup);

    layout->addStretch();

    // Zoom connections (press & release)
    connect(btnZoomTele, &QPushButton::pressed, m_device, &PelcoDQt::QPelcoDDevice::zoomTele);
    connect(btnZoomTele, &QPushButton::released, m_device, &PelcoDQt::QPelcoDDevice::zoomStop);
    connect(btnZoomWide, &QPushButton::pressed, m_device, &PelcoDQt::QPelcoDDevice::zoomWide);
    connect(btnZoomWide, &QPushButton::released, m_device, &PelcoDQt::QPelcoDDevice::zoomStop);

    // Focus connections
    connect(btnFocusNear, &QPushButton::pressed, m_device, &PelcoDQt::QPelcoDDevice::focusNear);
    connect(btnFocusNear, &QPushButton::released, m_device, &PelcoDQt::QPelcoDDevice::focusStop);
    connect(btnFocusFar, &QPushButton::pressed, m_device, &PelcoDQt::QPelcoDDevice::focusFar);
    connect(btnFocusFar, &QPushButton::released, m_device, &PelcoDQt::QPelcoDDevice::focusStop);

    // Iris connections
    connect(btnIrisOpen, &QPushButton::pressed, m_device, &PelcoDQt::QPelcoDDevice::irisOpen);
    connect(btnIrisOpen, &QPushButton::released, m_device, &PelcoDQt::QPelcoDDevice::irisStop);
    connect(btnIrisClose, &QPushButton::pressed, m_device, &PelcoDQt::QPelcoDDevice::irisClose);
    connect(btnIrisClose, &QPushButton::released, m_device, &PelcoDQt::QPelcoDDevice::irisStop);
}

void PtzControlTab::createPositioning(QVBoxLayout* layout)
{
    // Telemetry display
    auto* telemGroup = new QGroupBox(tr("Live Telemetry"), this);
    auto* telemGrid = new QGridLayout(telemGroup);

    telemGrid->addWidget(new QLabel(tr("Pan Position:")), 0, 0);
    lblTelemPan = new QLabel("0.00°");
    lblTelemPan->setObjectName("lblTelemetry");
    telemGrid->addWidget(lblTelemPan, 0, 1);

    telemGrid->addWidget(new QLabel(tr("Tilt Position:")), 1, 0);
    lblTelemTilt = new QLabel("0.00°");
    lblTelemTilt->setObjectName("lblTelemetry");
    telemGrid->addWidget(lblTelemTilt, 1, 1);

    telemGrid->addWidget(new QLabel(tr("Zoom Counts:")), 2, 0);
    lblTelemZoom = new QLabel("0");
    lblTelemZoom->setObjectName("lblTelemetry");
    telemGrid->addWidget(lblTelemZoom, 2, 1);

    layout->addWidget(telemGroup);

    // Absolute setters
    auto* absGroup = new QGroupBox(tr("Absolute Coordinate Target"), this);
    auto* absGrid = new QGridLayout(absGroup);

    absGrid->addWidget(new QLabel(tr("Pan Angle (0-359.99°):")), 0, 0);
    spinPanAngle = new QDoubleSpinBox();
    spinPanAngle->setRange(0.0, 359.99);
    spinPanAngle->setDecimals(2);
    spinPanAngle->setSingleStep(1.0);
    auto* btnSetPan = new QPushButton(tr("Set Pan"));
    absGrid->addWidget(spinPanAngle, 0, 1);
    absGrid->addWidget(btnSetPan, 0, 2);

    absGrid->addWidget(new QLabel(tr("Tilt Angle (0-359.99°):")), 1, 0);
    spinTiltAngle = new QDoubleSpinBox();
    spinTiltAngle->setRange(0.0, 359.99);
    spinTiltAngle->setDecimals(2);
    spinTiltAngle->setSingleStep(1.0);
    auto* btnSetTilt = new QPushButton(tr("Set Tilt"));
    absGrid->addWidget(spinTiltAngle, 1, 1);
    absGrid->addWidget(btnSetTilt, 1, 2);

    absGrid->addWidget(new QLabel(tr("Zoom Position (Counts):")), 2, 0);
    spinZoomPos = new QSpinBox();
    spinZoomPos->setRange(0, 65535);
    auto* btnSetZoom = new QPushButton(tr("Set Zoom"));
    absGrid->addWidget(spinZoomPos, 2, 1);
    absGrid->addWidget(btnSetZoom, 2, 2);

    layout->addWidget(absGroup);
    layout->addStretch();

    connect(btnSetPan, &QPushButton::clicked, this, &PtzControlTab::handleSetPanAngle);
    connect(btnSetTilt, &QPushButton::clicked, this, &PtzControlTab::handleSetTiltAngle);
    connect(btnSetZoom, &QPushButton::clicked, this, &PtzControlTab::handleSetZoomPosition);
}

void PtzControlTab::handleDpadPressed(int panDir, int tiltDir)
{
    const int panSpd = chkTurboPan->isChecked() ? 0x40 : sliderPanSpeed->value();
    const int tiltSpd = sliderTiltSpeed->value();
    m_device->move(panDir, panSpd, tiltDir, tiltSpd);
}

void PtzControlTab::handleDpadReleased()
{
    m_device->stopMotion();
}

void PtzControlTab::handleSetPanAngle()
{
    const auto centidegrees = static_cast<int>(spinPanAngle->value() * 100.0);
    m_device->setPanAngle(centidegrees);
}

void PtzControlTab::handleSetTiltAngle()
{
    const auto centidegrees = static_cast<int>(spinTiltAngle->value() * 100.0);
    m_device->setTiltAngle(centidegrees);
}

void PtzControlTab::handleSetZoomPosition()
{
    m_device->setZoomPosition(spinZoomPos->value());
}

void PtzControlTab::updateTelemetry(const PelcoD::DeviceStatus& status)
{
    lblTelemPan->setText(QString::number(status.panDegrees(), 'f', 2) + "°");
    lblTelemTilt->setText(QString::number(status.tiltDegrees(), 'f', 2) + "°");
    lblTelemZoom->setText(QString::number(status.zoomPosition));
}

} // namespace PelcoDApp
