#pragma once

/// @file RegistersHardwareTab.h
/// @brief UI tab for Sony hardware capability gating (4K vs 1080p, distortion comp) and register manipulation.

#include "../QViscaSonyDevice.h"
#include <SonyCameraModel.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

namespace ViscaApp {

/// @class RegistersHardwareTab
/// @brief Provides hardware capability inspection, model-gated feature toggles, and direct register I/O.
class RegistersHardwareTab : public QWidget {
    Q_OBJECT

public:
    explicit RegistersHardwareTab(QViscaSonyDevice* device, QWidget* parent = nullptr);
    ~RegistersHardwareTab() override = default;

public slots:
    void updateModelCapabilities(
        Visca::Sony::SonyCameraModelType modelType, const Visca::Sony::CameraCapabilities& caps);
    void updateTelemetry(const Visca::Sony::SonyFCBStatus& status);

private slots:
    void handleOperatingModeChanged(int index);
    void handleDistortionCompToggled(bool checked);
    void handleOpticalAxisGapToggled(bool checked);
    void handleLvdsModeChanged(int index);
    void handleTmdsModeChanged(int index);

    void handleReadRegisterClicked();
    void handleWriteRegisterClicked();

private:
    void setupUi();
    void applyGatingUI();

    QViscaSonyDevice* m_device { nullptr };
    Visca::Sony::CameraCapabilities m_capabilities {};

    // Model Info
    QLabel* lblModelName { nullptr };
    QLabel* lblSensorDesc { nullptr };
    QLabel* lblOpticalRange { nullptr };

    // Capability Gated Features
    QGroupBox* grpGatedFeatures { nullptr };
    QComboBox* cmbOperatingMode { nullptr };
    QLabel* lblOperatingModeWarn { nullptr };

    QCheckBox* chkDistortionComp { nullptr };
    QLabel* lblDistortionCompWarn { nullptr };

    QCheckBox* chkOpticalAxisGap { nullptr };
    QLabel* lblOpticalAxisGapWarn { nullptr };

    QComboBox* cmbLvdsMode { nullptr };
    QComboBox* cmbTmdsMode { nullptr };

    // Register Direct Access
    QSpinBox* spinRegAddress { nullptr };
    QSpinBox* spinRegValue { nullptr };
    QPushButton* btnReadReg { nullptr };
    QPushButton* btnWriteReg { nullptr };
    QLabel* lblRegResult { nullptr };
};

} // namespace ViscaApp
