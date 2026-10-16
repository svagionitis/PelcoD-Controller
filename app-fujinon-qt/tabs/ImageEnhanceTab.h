#pragma once

/// @file ImageEnhanceTab.h
/// @brief Image enhancements, stabilization, defog, haze, WDR, and fine tuning tab.

#include "FujinonTypes.h"
#include "QPelcoDFujinonDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace FujinonApp {

/// @class ImageEnhanceTab
/// @brief Tab providing image stabilization, optical filters, defog, and fine 1..100 picture tuners.
class ImageEnhanceTab : public QWidget {
    Q_OBJECT

public:
    explicit ImageEnhanceTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent = nullptr);
    ~ImageEnhanceTab() override = default;

public slots:
    /// @brief Update indicators from latest Fujinon telemetry snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    void handleOisChanged(int index);
    void handleVlcToggled(bool checked);
    void handleAntialiasingToggled(bool checked);

    void handleDefogChanged(int index);
    void handleHeatHazeChanged(int index);
    void handleWdrChanged(int index);
    void handleNoiseReductionChanged(int val);

    void handleWbModeChanged(int index);
    void handleColorTempChanged(int index);

    void handleBrightnessChanged(int val);
    void handleContrastChanged(int val);
    void handleSaturationChanged(int val);
    void handleSharpnessChanged(int val);
    void handleWbRedChanged(int val);
    void handleWbBlueChanged(int val);

    void handleQueryFineQuality();
    void handleQueryImageQuality();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    // Stabilization & Filters
    QComboBox* cmbOis { nullptr };
    QCheckBox* chkVlc { nullptr };
    QCheckBox* chkAntialiasing { nullptr };

    // Atmospheric & Noise
    QComboBox* cmbDefog { nullptr };
    QComboBox* cmbHeatHaze { nullptr };
    QComboBox* cmbWdr { nullptr };
    QSpinBox* spinNoiseReduction { nullptr };

    // White Balance & Color Temp
    QComboBox* cmbWbMode { nullptr };
    QComboBox* cmbColorTemp { nullptr };

    // Fine Picture Controls (1..100)
    QSlider* sliderBrightness { nullptr };
    QSpinBox* spinBrightness { nullptr };
    QSlider* sliderContrast { nullptr };
    QSpinBox* spinContrast { nullptr };
    QSlider* sliderSaturation { nullptr };
    QSpinBox* spinSaturation { nullptr };
    QSlider* sliderSharpness { nullptr };
    QSpinBox* spinSharpness { nullptr };
    QSlider* sliderWbRed { nullptr };
    QSpinBox* spinWbRed { nullptr };
    QSlider* sliderWbBlue { nullptr };
    QSpinBox* spinWbBlue { nullptr };

    QPushButton* btnQueryFine { nullptr };
    QPushButton* btnQueryQuality { nullptr };
};

} // namespace FujinonApp
