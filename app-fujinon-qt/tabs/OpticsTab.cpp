/// @file OpticsTab.cpp
/// @brief Implementation of OpticsTab for Fujinon optical controls and telemetry.

#include "OpticsTab.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

/// @class Hex16SpinBox
/// @brief 16-bit hexadecimal spinbox formatted with leading zeros (e.g. 0x0000 to 0xFFFF).
class Hex16SpinBox : public QSpinBox {
public:
    explicit Hex16SpinBox(QWidget* parent = nullptr)
        : QSpinBox(parent)
    {
        setRange(0, 65535);
        setDisplayIntegerBase(16);
        setPrefix(QStringLiteral("0x"));
        setMinimumWidth(100);
        setAlignment(Qt::AlignRight);
    }

protected:
    [[nodiscard]] QString textFromValue(int value) const override
    {
        return QStringLiteral("%1").arg(static_cast<std::uint16_t>(value), 4, 16, QLatin1Char('0')).toUpper();
    }

    [[nodiscard]] int valueFromText(const QString& text) const override
    {
        bool ok { false };
        QString clean = text.trimmed();
        if (clean.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            clean = clean.mid(2);
        }
        const int val = clean.toInt(&ok, 16);
        return ok ? val : 0;
    }
};

} // namespace

namespace FujinonApp {

OpticsTab::OpticsTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* OpticsTab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void OpticsTab::setupUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* mainLayout = new QHBoxLayout(container);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(14);

    // =========================================================================
    // Column 1: Continuous Drive, Coordinates & Telemetry
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    // Group 1: Continuous Zoom & Focus Drive
    auto* grpDrive = new QGroupBox(tr("Continuous Optics Drive"), container);
    auto* gridDrive = new QGridLayout(grpDrive);

    btnZoomIn = new QPushButton(tr("Zoom Tele (In)"), grpDrive);
    btnZoomOut = new QPushButton(tr("Zoom Wide (Out)"), grpDrive);
    btnFocusNear = new QPushButton(tr("Focus Near"), grpDrive);
    btnFocusFar = new QPushButton(tr("Focus Far"), grpDrive);

    gridDrive->addWidget(btnZoomIn, 0, 0);
    gridDrive->addWidget(btnZoomOut, 0, 1);
    gridDrive->addWidget(btnFocusNear, 1, 0);
    gridDrive->addWidget(btnFocusFar, 1, 1);
    col1->addWidget(grpDrive);

    // Group 2: Absolute Coordinates (16-bit)
    auto* grpCoords = new QGroupBox(tr("Absolute 16-Bit Coordinates"), container);
    auto* gridCoords = new QGridLayout(grpCoords);

    gridCoords->addWidget(new QLabel(tr("Zoom Target:")), 0, 0);
    spinZoomPos = new Hex16SpinBox(grpCoords);
    sliderZoomPos = new QSlider(Qt::Horizontal, grpCoords);
    sliderZoomPos->setRange(0, 65535);
    btnSetZoomPos = new QPushButton(tr("Drive Zoom"), grpCoords);
    btnQueryZoom = new QPushButton(tr("Query Zoom"), grpCoords);
    gridCoords->addWidget(spinZoomPos, 0, 1);
    gridCoords->addWidget(sliderZoomPos, 0, 2);
    gridCoords->addWidget(btnSetZoomPos, 0, 3);
    gridCoords->addWidget(btnQueryZoom, 0, 4);

    gridCoords->addWidget(new QLabel(tr("Focus Target:")), 1, 0);
    spinFocusPos = new Hex16SpinBox(grpCoords);
    sliderFocusPos = new QSlider(Qt::Horizontal, grpCoords);
    sliderFocusPos->setRange(0, 65535);
    btnSetFocusPos = new QPushButton(tr("Drive Focus"), grpCoords);
    btnQueryFocus = new QPushButton(tr("Query Focus"), grpCoords);
    gridCoords->addWidget(spinFocusPos, 1, 1);
    gridCoords->addWidget(sliderFocusPos, 1, 2);
    gridCoords->addWidget(btnSetFocusPos, 1, 3);
    gridCoords->addWidget(btnQueryFocus, 1, 4);

    gridCoords->setColumnStretch(1, 0);
    gridCoords->setColumnStretch(2, 1);
    col1->addWidget(grpCoords);

    // Group 3: Telemetry Display
    auto* grpTelem = new QGroupBox(tr("Optical Telemetry"), container);
    auto* gridTelem = new QGridLayout(grpTelem);

    lblFocalLength = new QLabel(tr("Focal Length: 20.0 mm"), grpTelem);
    barFocalLength = new QProgressBar(grpTelem);
    barFocalLength->setRange(20, 800);
    barFocalLength->setValue(20);
    barFocalLength->setTextVisible(false);
    lblZoomVal = new QLabel(tr("Raw Zoom: 0x0000"), grpTelem);
    lblFocusVal = new QLabel(tr("Raw Focus: 0x0000"), grpTelem);
    lblSerial = new QLabel(tr("Serial: --"), grpTelem);
    lblFirmware = new QLabel(tr("Firmware: --"), grpTelem);
    lblLensStatus = new QLabel(tr("Lens Status: Normal"), grpTelem);

    gridTelem->addWidget(lblFocalLength, 0, 0);
    gridTelem->addWidget(barFocalLength, 0, 1);
    gridTelem->addWidget(lblZoomVal, 1, 0);
    gridTelem->addWidget(lblFocusVal, 1, 1);
    gridTelem->addWidget(lblSerial, 2, 0);
    gridTelem->addWidget(lblFirmware, 2, 1);
    gridTelem->addWidget(lblLensStatus, 3, 0, 1, 2);
    col1->addWidget(grpTelem);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: Auto Focus, Speeds & Digital Zoom
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    // Group 4: Auto Focus
    auto* grpAf = new QGroupBox(tr("Auto Focus Controls"), container);
    auto* gridAf = new QGridLayout(grpAf);

    gridAf->addWidget(new QLabel(tr("AF Mode:")), 0, 0);
    cmbAfMode = new QComboBox(grpAf);
    cmbAfMode->addItem(tr("Auto Focus (Continuous)"), static_cast<int>(PelcoD::FujinonAutoFocusMode::AFOn));
    cmbAfMode->addItem(tr("Manual Focus (AF Off)"), static_cast<int>(PelcoD::FujinonAutoFocusMode::AFOff));
    cmbAfMode->addItem(tr("Quick AF"), static_cast<int>(PelcoD::FujinonAutoFocusMode::QuickAF));
    btnOnePushAf = new QPushButton(tr("Push to Focus (One-Push AF)"), grpAf);
    gridAf->addWidget(cmbAfMode, 0, 1);
    gridAf->addWidget(btnOnePushAf, 0, 2);

    gridAf->addWidget(new QLabel(tr("AF Sensitivity:")), 1, 0);
    cmbAfSensitivity = new QComboBox(grpAf);
    cmbAfSensitivity->addItem(tr("Low (1)"), 1);
    cmbAfSensitivity->addItem(tr("Middle (2)"), 2);
    cmbAfSensitivity->addItem(tr("High (3)"), 3);
    gridAf->addWidget(cmbAfSensitivity, 1, 1, 1, 2);

    gridAf->addWidget(new QLabel(tr("AF Area:")), 2, 0);
    cmbAfArea = new QComboBox(grpAf);
    cmbAfArea->addItem(tr("Center (Fixed) [0]"), 0);
    cmbAfArea->addItem(tr("Upper Left [1]"), 1);
    cmbAfArea->addItem(tr("Upper Center [2]"), 2);
    cmbAfArea->addItem(tr("Upper Right [3]"), 3);
    cmbAfArea->addItem(tr("Middle Left [4]"), 4);
    cmbAfArea->addItem(tr("Center (Middle) [5]"), 5);
    cmbAfArea->addItem(tr("Middle Right [6]"), 6);
    cmbAfArea->addItem(tr("Lower Left [7]"), 7);
    cmbAfArea->addItem(tr("Lower Center [8]"), 8);
    cmbAfArea->addItem(tr("Lower Right [9]"), 9);
    gridAf->addWidget(cmbAfArea, 2, 1, 1, 2);
    col2->addWidget(grpAf);

    // Group 5: Speeds & Digital Zoom
    auto* grpSpeedDz = new QGroupBox(tr("Speeds & Digital Zoom"), container);
    auto* gridSpeedDz = new QGridLayout(grpSpeedDz);

    gridSpeedDz->addWidget(new QLabel(tr("Zoom Speed:")), 0, 0);
    cmbZoomSpeed = new QComboBox(grpSpeedDz);
    for (int i = 1; i <= 8; ++i) {
        cmbZoomSpeed->addItem(tr("Speed %1").arg(i), i);
    }
    gridSpeedDz->addWidget(cmbZoomSpeed, 0, 1);

    gridSpeedDz->addWidget(new QLabel(tr("Focus Speed:")), 1, 0);
    cmbFocusSpeed = new QComboBox(grpSpeedDz);
    for (int i = 1; i <= 5; ++i) {
        cmbFocusSpeed->addItem(tr("Speed %1").arg(i), i);
    }
    gridSpeedDz->addWidget(cmbFocusSpeed, 1, 1);

    gridSpeedDz->addWidget(new QLabel(tr("Digital Zoom Mode:")), 2, 0);
    cmbDzMode = new QComboBox(grpSpeedDz);
    cmbDzMode->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonDigitalZoomMode::Off));
    cmbDzMode->addItem(tr("Digital Zoom (Mag)"), static_cast<int>(PelcoD::FujinonDigitalZoomMode::DigitalZoom));
    cmbDzMode->addItem(tr("Crop Mode (Full HD)"), static_cast<int>(PelcoD::FujinonDigitalZoomMode::CropMode));
    gridSpeedDz->addWidget(cmbDzMode, 2, 1);

    gridSpeedDz->addWidget(new QLabel(tr("Magnification:")), 3, 0);
    cmbDzMag = new QComboBox(grpSpeedDz);
    cmbDzMag->addItem(tr("1.00x (Off)"), 0);
    cmbDzMag->addItem(tr("1.25x"), 1);
    cmbDzMag->addItem(tr("1.50x"), 2);
    cmbDzMag->addItem(tr("1.75x"), 3);
    cmbDzMag->addItem(tr("2.00x"), 4);
    gridSpeedDz->addWidget(cmbDzMag, 3, 1);

    btnQueryZfEx = new QPushButton(tr("Query Zoom/Focus Ex"), grpSpeedDz);
    gridSpeedDz->addWidget(btnQueryZfEx, 4, 0, 1, 2);
    col2->addWidget(grpSpeedDz);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void OpticsTab::setupConnections()
{
    // Continuous drive
    connect(btnZoomIn, &QPushButton::pressed, this, &OpticsTab::handleZoomInPressed);
    connect(btnZoomIn, &QPushButton::released, this, &OpticsTab::handleZoomInReleased);
    connect(btnZoomOut, &QPushButton::pressed, this, &OpticsTab::handleZoomOutPressed);
    connect(btnZoomOut, &QPushButton::released, this, &OpticsTab::handleZoomOutReleased);
    connect(btnFocusNear, &QPushButton::pressed, this, &OpticsTab::handleFocusNearPressed);
    connect(btnFocusNear, &QPushButton::released, this, &OpticsTab::handleFocusNearReleased);
    connect(btnFocusFar, &QPushButton::pressed, this, &OpticsTab::handleFocusFarPressed);
    connect(btnFocusFar, &QPushButton::released, this, &OpticsTab::handleFocusFarReleased);

    // Sync spinbox and sliders
    connect(spinZoomPos, QOverload<int>::of(&QSpinBox::valueChanged), sliderZoomPos, &QSlider::setValue);
    connect(sliderZoomPos, &QSlider::valueChanged, spinZoomPos, &QSpinBox::setValue);
    connect(spinFocusPos, QOverload<int>::of(&QSpinBox::valueChanged), sliderFocusPos, &QSlider::setValue);
    connect(sliderFocusPos, &QSlider::valueChanged, spinFocusPos, &QSpinBox::setValue);

    connect(btnSetZoomPos, &QPushButton::clicked, this, &OpticsTab::handleSetZoomPos);
    connect(btnQueryZoom, &QPushButton::clicked, this, &OpticsTab::handleQueryZoom);
    connect(btnSetFocusPos, &QPushButton::clicked, this, &OpticsTab::handleSetFocusPos);
    connect(btnQueryFocus, &QPushButton::clicked, this, &OpticsTab::handleQueryFocus);

    // Auto focus
    connect(cmbAfMode, QOverload<int>::of(&QComboBox::activated), this, &OpticsTab::handleAfModeChanged);
    connect(btnOnePushAf, &QPushButton::clicked, this, &OpticsTab::handleOnePushAf);
    connect(cmbAfSensitivity, QOverload<int>::of(&QComboBox::activated), this, &OpticsTab::handleAfSensChanged);
    connect(cmbAfArea, QOverload<int>::of(&QComboBox::activated), this, &OpticsTab::handleAfAreaChanged);

    // Speeds & Digital Zoom
    connect(
        cmbZoomSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpticsTab::handleZoomSpeedChanged);
    connect(
        cmbFocusSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpticsTab::handleFocusSpeedChanged);
    connect(cmbDzMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpticsTab::handleDzModeChanged);
    connect(cmbDzMag, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpticsTab::handleDzMagChanged);
    connect(btnQueryZfEx, &QPushButton::clicked, this, &OpticsTab::handleQueryZoomFocusEx);
}

void OpticsTab::handleZoomInPressed()
{
    if (m_device) {
        m_device->zoomTele();
    }
}

void OpticsTab::handleZoomInReleased()
{
    if (m_device) {
        m_device->zoomStop();
    }
}

void OpticsTab::handleZoomOutPressed()
{
    if (m_device) {
        m_device->zoomWide();
    }
}

void OpticsTab::handleZoomOutReleased()
{
    if (m_device) {
        m_device->zoomStop();
    }
}

void OpticsTab::handleFocusNearPressed()
{
    if (m_device) {
        m_device->focusNear();
    }
}

void OpticsTab::handleFocusNearReleased()
{
    if (m_device) {
        m_device->focusStop();
    }
}

void OpticsTab::handleFocusFarPressed()
{
    if (m_device) {
        m_device->focusFar();
    }
}

void OpticsTab::handleFocusFarReleased()
{
    if (m_device) {
        m_device->focusStop();
    }
}

void OpticsTab::handleSetZoomPos()
{
    if (auto* f = fujinon()) {
        f->setZoomPosition(spinZoomPos->value());
    }
}

void OpticsTab::handleQueryZoom()
{
    if (auto* f = fujinon()) {
        f->queryZoom();
    } else if (m_device) {
        m_device->queryZoom();
    }
}

void OpticsTab::handleSetFocusPos()
{
    if (auto* f = fujinon()) {
        f->setFocusPosition(spinFocusPos->value());
    }
}

void OpticsTab::handleQueryFocus()
{
    if (auto* f = fujinon()) {
        f->queryFocus();
    }
}

void OpticsTab::handleAfModeChanged(int index)
{
    if (index < 0) {
        return;
    }
    const auto mode = static_cast<PelcoD::FujinonAutoFocusMode>(cmbAfMode->itemData(index).toInt());
    if (auto* f = fujinon()) {
        f->setAutoFocus(mode);
    } else if (m_device) {
        m_device->setAutoFocus(mode);
    }
}

void OpticsTab::handleOnePushAf()
{
    // Section 5.4.3: One-push AF requires AF to be Off (Manual Focus)
    const int offIdx = cmbAfMode->findData(static_cast<int>(PelcoD::FujinonAutoFocusMode::AFOff));
    if (offIdx >= 0 && cmbAfMode->currentIndex() != offIdx) {
        const QSignalBlocker blocker(cmbAfMode);
        cmbAfMode->setCurrentIndex(offIdx);
    }
    if (auto* f = fujinon()) {
        f->setAutoFocus(PelcoD::FujinonAutoFocusMode::AFOff);
        f->onePushAF();
    } else if (m_device) {
        m_device->setAutoFocus(PelcoD::FujinonAutoFocusMode::AFOff);
        m_device->onePushAF();
    }
}

void OpticsTab::handleAfSensChanged(int index)
{
    if (index < 0) {
        return;
    }
    const int sens = cmbAfSensitivity->itemData(index).toInt();
    if (auto* f = fujinon()) {
        f->setAFSensitivity(sens);
    } else if (m_device && m_device->fujinonCoreDevice()) {
        static_cast<void>(m_device->fujinonCoreDevice()->setAFSensitivity(static_cast<std::uint8_t>(sens)));
    }
}

void OpticsTab::handleAfAreaChanged(int index)
{
    if (index < 0) {
        return;
    }
    const int area = cmbAfArea->itemData(index).toInt();
    if (auto* f = fujinon()) {
        f->setAFArea(area);
    } else if (m_device && m_device->fujinonCoreDevice()) {
        static_cast<void>(m_device->fujinonCoreDevice()->setAFArea(static_cast<std::uint8_t>(area)));
    }
}

void OpticsTab::handleZoomSpeedChanged(int index)
{
    if (auto* f = fujinon()) {
        const int spd = cmbZoomSpeed->itemData(index).toInt();
        f->setZoomSpeed(spd);
        f->setZoomSpeedEx(spd);
    }
}

void OpticsTab::handleFocusSpeedChanged(int index)
{
    if (auto* f = fujinon()) {
        const int spd = cmbFocusSpeed->itemData(index).toInt();
        f->setFocusSpeed(spd);
        f->setFocusSpeedEx(spd);
    }
}

void OpticsTab::handleDzModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonDigitalZoomMode>(cmbDzMode->itemData(index).toInt());
        f->setDigitalZoomMode(mode);
    }
}

void OpticsTab::handleDzMagChanged(int index)
{
    if (auto* f = fujinon()) {
        const int mag = cmbDzMag->itemData(index).toInt();
        if (mag == 0) {
            f->setDigitalZoom(PelcoD::FujinonDigitalZoom::Off);
        } else {
            f->setDigitalZoomEx(mag - 1);
        }
    }
}

void OpticsTab::handleQueryZoomFocusEx()
{
    if (auto* f = fujinon()) {
        f->queryZoomFocusEx(0x00);
    }
}

void OpticsTab::updateBaseStatus(const PelcoD::DeviceStatus& status)
{
    const int zoomVal = static_cast<int>(status.zoomPosition);
    {
        const QSignalBlocker bSpin(spinZoomPos);
        const QSignalBlocker bSlider(sliderZoomPos);
        spinZoomPos->setValue(zoomVal);
        sliderZoomPos->setValue(zoomVal);
    }
    lblZoomVal->setText(tr("Raw Zoom: 0x%1").arg(zoomVal, 4, 16, QChar('0')));
}

void OpticsTab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    lblFocalLength->setText(tr("Focal Length: %1 mm").arg(status.focalLengthMm, 0, 'f', 1));
    barFocalLength->setValue(static_cast<int>(status.focalLengthMm));

    const int currentZoom = (status.absoluteZoomPosition != 0U) ? static_cast<int>(status.absoluteZoomPosition)
                                                                : static_cast<int>(status.baseStatus.zoomPosition);

    const int currentFocus = (status.absoluteFocusPosition != 0U) ? static_cast<int>(status.absoluteFocusPosition)
                                                                  : static_cast<int>(status.baseStatus.focusPosition);

    lblZoomVal->setText(tr("Raw Zoom: 0x%1").arg(currentZoom, 4, 16, QChar('0')));
    lblFocusVal->setText(tr("Raw Focus: 0x%1").arg(currentFocus, 4, 16, QChar('0')));

    {
        const QSignalBlocker bSpin(spinZoomPos);
        const QSignalBlocker bSlider(sliderZoomPos);
        spinZoomPos->setValue(currentZoom);
        sliderZoomPos->setValue(currentZoom);
    }

    {
        const QSignalBlocker bSpin(spinFocusPos);
        const QSignalBlocker bSlider(sliderFocusPos);
        spinFocusPos->setValue(currentFocus);
        sliderFocusPos->setValue(currentFocus);
    }

    const int afModeIdx = cmbAfMode->findData(static_cast<int>(status.autoFocusMode));
    if (afModeIdx >= 0 && cmbAfMode->currentIndex() != afModeIdx) {
        const QSignalBlocker blocker(cmbAfMode);
        cmbAfMode->setCurrentIndex(afModeIdx);
    }

    const int afSensIdx = cmbAfSensitivity->findData(static_cast<int>(status.afSensitivity));
    if (afSensIdx >= 0 && cmbAfSensitivity->currentIndex() != afSensIdx) {
        const QSignalBlocker blocker(cmbAfSensitivity);
        cmbAfSensitivity->setCurrentIndex(afSensIdx);
    }

    const int afAreaIdx = cmbAfArea->findData(static_cast<int>(status.afArea));
    if (afAreaIdx >= 0 && cmbAfArea->currentIndex() != afAreaIdx) {
        const QSignalBlocker blocker(cmbAfArea);
        cmbAfArea->setCurrentIndex(afAreaIdx);
    }

    if (!status.serialNumber.empty()) {
        lblSerial->setText(tr("Serial: %1").arg(QString::fromStdString(status.serialNumber)));
    }
    if (!status.firmwareVersion.empty()) {
        lblFirmware->setText(tr("Firmware: %1").arg(QString::fromStdString(status.firmwareVersion)));
    }

    QString statusText = tr("Normal");
    if (static_cast<std::uint8_t>(status.lensStatus) != 0U) {
        statusText = tr("Code 0x%1").arg(static_cast<std::uint8_t>(status.lensStatus), 2, 16, QChar('0'));
    }
    lblLensStatus->setText(tr("Lens Status: %1").arg(statusText));
}

} // namespace FujinonApp
