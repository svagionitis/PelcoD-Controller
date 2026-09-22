#pragma once

/// @file FujinonSX800Tab.h
/// @brief Dedicated dashboard tab for Fujinon SX800 / SX801 optical surveillance cameras.

#include "FujinonTypes.h"
#include "QPelcoDDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class FujinonSX800Tab
/// @brief Dashboard tab providing comprehensive controls and telemetry for Fujinon SX800/SX801.
class FujinonSX800Tab : public QWidget {
    Q_OBJECT

public:
    explicit FujinonSX800Tab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~FujinonSX800Tab() override = default;

public slots:
    /// @brief Updates GUI telemetry and indicators from received Fujinon status snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    // Optics & Stabilization
    void handleOisChanged(int index);
    void handleVlcToggled(bool checked);
    void handleDefogChanged(int index);
    void handleHeatHazeChanged(int index);
    void handleWdrChanged(int index);
    void handleAntialiasingToggled(bool checked);

    // Fine Image Quality (1..100)
    void handleBrightnessFineChanged(int val);
    void handleContrastFineChanged(int val);
    void handleSaturationFineChanged(int val);
    void handleSharpnessFineChanged(int val);
    void handleWbShiftRedFineChanged(int val);
    void handleWbShiftBlueFineChanged(int val);
    void handleWbModeChanged(int index);
    void handleNoiseReductionChanged(int val);
    void handleQueryFineSettings();

    // Day / Night & Scheduling
    void handleDayNightExModeChanged(int index);
    void handleSetDayToNightThreshold();
    void handleSetNightToDayThreshold();
    void handleSetAutoDelay();
    void handleSetDayStartTime();
    void handleSetNightStartTime();
    void handleDayOpticalFilterChanged(int index);
    void handleNightOpticalFilterChanged(int index);
    void handleIrWavelengthChanged(int index);
    void handleQueryDayNight();

    // Zoom, Focus & Telemetry
    void handleZoomSpeedExChanged(int index);
    void handleFocusSpeedExChanged(int index);
    void handleDigitalZoomModeChanged(int index);
    void handleDigitalZoomMagChanged(int index);
    void handleOnePushAF();
    void handleQueryAllTelemetry();

    // System, OSD & Maintenance
    void handleLanguageChanged(int index);
    void handleOsdTimeToggled(bool checked);
    void handleOsdTitleToggled(bool checked);
    void handleOsdIdToggled(bool checked);
    void handleOsdReticleToggled(bool checked);
    void handleMenuNav(PelcoD::FujinonMenuDirection dir);
    void handleMenuOk();
    void handleMenuBack();
    void handleSyncRtc();
    void handleFormatSDCard();
    void handleFactoryReset();
    void handleReboot();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    // Group 1: Optics & Stabilization
    QComboBox* cmbOis { nullptr };
    QCheckBox* chkVlc { nullptr };
    QComboBox* cmbDefog { nullptr };
    QComboBox* cmbHeatHaze { nullptr };
    QComboBox* cmbWdr { nullptr };
    QCheckBox* chkAntialiasing { nullptr };

    // Group 2: Fine Image Adjustments (1..100)
    QSlider* sliderBrightnessFine { nullptr };
    QSpinBox* spinBrightnessFine { nullptr };
    QSlider* sliderContrastFine { nullptr };
    QSpinBox* spinContrastFine { nullptr };
    QSlider* sliderSaturationFine { nullptr };
    QSpinBox* spinSaturationFine { nullptr };
    QSlider* sliderSharpnessFine { nullptr };
    QSpinBox* spinSharpnessFine { nullptr };
    QSlider* sliderWbShiftRedFine { nullptr };
    QSpinBox* spinWbShiftRedFine { nullptr };
    QSlider* sliderWbShiftBlueFine { nullptr };
    QSpinBox* spinWbShiftBlueFine { nullptr };
    QComboBox* cmbWbMode { nullptr };
    QSpinBox* spinNoiseReduction { nullptr };
    QPushButton* btnQueryFine { nullptr };

    // Group 3: Day / Night & Scheduling
    QComboBox* cmbDayNightEx { nullptr };
    QSpinBox* spinDayToNight { nullptr };
    QSpinBox* spinNightToDay { nullptr };
    QSpinBox* spinAutoDelay { nullptr };
    QTimeEdit* timeDayStart { nullptr };
    QTimeEdit* timeNightStart { nullptr };
    QComboBox* cmbDayOpticalFilter { nullptr };
    QComboBox* cmbNightOpticalFilter { nullptr };
    QComboBox* cmbIrWavelength { nullptr };
    QPushButton* btnQueryDayNight { nullptr };

    // Group 4: Zoom, Focus & Telemetry
    QLabel* lblFocalLength { nullptr };
    QProgressBar* barFocalLength { nullptr };
    QLabel* lblZoomCounts { nullptr };
    QLabel* lblFocusCounts { nullptr };
    QLabel* lblSerial { nullptr };
    QLabel* lblFirmware { nullptr };
    QLabel* lblLensStatus { nullptr };
    QComboBox* cmbZoomSpeedEx { nullptr };
    QComboBox* cmbFocusSpeedEx { nullptr };
    QComboBox* cmbDigitalZoomMode { nullptr };
    QComboBox* cmbDigitalZoomMag { nullptr };
    QPushButton* btnOnePushAf { nullptr };
    QPushButton* btnQueryAll { nullptr };

    // Group 5: System, OSD & Maintenance
    QComboBox* cmbLanguage { nullptr };
    QCheckBox* chkOsdTime { nullptr };
    QCheckBox* chkOsdTitle { nullptr };
    QCheckBox* chkOsdId { nullptr };
    QCheckBox* chkOsdReticle { nullptr };
    QPushButton* btnMenuUp { nullptr };
    QPushButton* btnMenuDown { nullptr };
    QPushButton* btnMenuLeft { nullptr };
    QPushButton* btnMenuRight { nullptr };
    QPushButton* btnMenuOk { nullptr };
    QPushButton* btnMenuBack { nullptr };
    QPushButton* btnSyncRtc { nullptr };
    QPushButton* btnFormatSD { nullptr };
    QPushButton* btnFactoryReset { nullptr };
    QPushButton* btnReboot { nullptr };
};

} // namespace PelcoDApp
