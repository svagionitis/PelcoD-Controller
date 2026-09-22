#pragma once

/// @file ExposureImagingTab.h
/// @brief UI tab for exposure modes, shutter, iris, gain, white balance, stabilizer, and defog.

#include "../QViscaSonyDevice.h"
#include <SonyViscaTypes.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace ViscaApp {

/// @class ExposureImagingTab
/// @brief Controls camera sensor exposure, manual white balance, Super+ stabilizer, and VE/Defog.
class ExposureImagingTab : public QWidget {
    Q_OBJECT

public:
    explicit ExposureImagingTab(QViscaSonyDevice* device, QWidget* parent = nullptr);
    ~ExposureImagingTab() override = default;

public slots:
    void updateTelemetry(const Visca::Sony::SonyFCBStatus& status);

private slots:
    void handleExposureModeChanged(int index);
    void handleShutterChanged(int index);
    void handleIrisChanged(int index);
    void handleGainChanged(int value);
    void handleExpCompToggled(bool checked);

    void handleWbModeChanged(int index);
    void handleRgainChanged(int value);
    void handleBgainChanged(int value);
    void handleOnePushWb();

    void handleStabilizerChanged(int index);
    void handleDefogChanged(int index);
    void handleIcrToggled(bool checked);

private:
    void setupUi();

    QViscaSonyDevice* m_device { nullptr };

    // Exposure
    QComboBox* cmbExpMode { nullptr };
    QComboBox* cmbShutter { nullptr };
    QComboBox* cmbIris { nullptr };
    QSlider* sliderGain { nullptr };
    QLabel* lblGainVal { nullptr };
    QCheckBox* chkExpComp { nullptr };
    QSlider* sliderExpComp { nullptr };

    // White Balance
    QComboBox* cmbWbMode { nullptr };
    QPushButton* btnOnePushWb { nullptr };
    QSlider* sliderRgain { nullptr };
    QLabel* lblRgainVal { nullptr };
    QSlider* sliderBgain { nullptr };
    QLabel* lblBgainVal { nullptr };

    // Enhancements
    QComboBox* cmbStabilizer { nullptr };
    QComboBox* cmbDefog { nullptr };
    QCheckBox* chkIcr { nullptr };
    QCheckBox* chkAutoIcr { nullptr };

    bool m_updatingFromTelemetry { false };
};

} // namespace ViscaApp
