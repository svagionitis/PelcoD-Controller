#pragma once

/// @file DeviceSettingsTab.h
/// @brief Camera and optical settings (Focus, Iris, AGC, Shutter, Gain, BLC, AWB).

#include "QPelcoDDevice.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class DeviceSettingsTab
/// @brief UI tab configuring device optical modes, exposure parameters, and motor speeds.
class DeviceSettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit DeviceSettingsTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~DeviceSettingsTab() override = default;

private slots:
    void handleAutoFocusChanged(int index);
    void handleAutoIrisChanged(int index);
    void handleAgcChanged(int index);
    void handleBlcChanged(int index);
    void handleAwbChanged(int index);
    void handleSetShutter();
    void handleSetGain();
    void handleSetIrisLevel();
    void handleSetIrisPeak();
    void handleZoomSpeedChanged(int speed);
    void handleFocusSpeedChanged(int speed);

private:
    void setupUi();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    QComboBox* cmbAutoFocus { nullptr };
    QComboBox* cmbAutoIris { nullptr };
    QComboBox* cmbAgc { nullptr };
    QComboBox* cmbBlc { nullptr };
    QComboBox* cmbAwb { nullptr };

    QSpinBox* spinShutter { nullptr };
    QSpinBox* spinGain { nullptr };
    QSlider* sliderIrisLevel { nullptr };
    QSlider* sliderIrisPeak { nullptr };

    QComboBox* cmbZoomSpeed { nullptr };
    QComboBox* cmbFocusSpeed { nullptr };
};

} // namespace PelcoDApp
