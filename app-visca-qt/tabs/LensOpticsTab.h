#pragma once

/// @file LensOpticsTab.h
/// @brief UI tab for optical zoom, focal length calculation, focus mode, and near limit.

#include "../QViscaSonyDevice.h"
#include <SonyCameraModel.h>
#include <SonyViscaTypes.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace ViscaApp {

/// @class LensOpticsTab
/// @brief Controls zoom and focus with model-calibrated focal length readouts in mm.
class LensOpticsTab : public QWidget {
    Q_OBJECT

public:
    explicit LensOpticsTab(QViscaSonyDevice* device, QWidget* parent = nullptr);
    ~LensOpticsTab() override = default;

public slots:
    void updateTelemetry(const Visca::Sony::SonyFCBStatus& status);
    void updateModelCapabilities(
        Visca::Sony::SonyCameraModelType modelType, const Visca::Sony::CameraCapabilities& caps);

private slots:
    void handleDirectZoomChanged(int value);
    void handleZoomTelePressed();
    void handleZoomWidePressed();
    void handleZoomReleased();

    void handleFocusAutoToggled(bool checked);
    void handleDirectFocusChanged(int value);
    void handleFocusNearLimitChanged(int index);
    void handleOnePushAf();

private:
    void setupUi();
    void updateFocalLengthDisplay(uint16_t zoomPos);

    QViscaSonyDevice* m_device { nullptr };
    Visca::Sony::CameraCapabilities m_capabilities {};

    // Zoom UI
    QSlider* sliderZoom { nullptr };
    QSpinBox* spinZoomHex { nullptr };
    QLabel* lblFocalLength { nullptr };
    QSlider* sliderZoomSpeed { nullptr };
    QPushButton* btnZoomTele { nullptr };
    QPushButton* btnZoomWide { nullptr };
    QComboBox* cmbDigitalZoom { nullptr };

    // Focus UI
    QCheckBox* chkAutoFocus { nullptr };
    QPushButton* btnOnePushAf { nullptr };
    QSlider* sliderFocus { nullptr };
    QSpinBox* spinFocusHex { nullptr };
    QSlider* sliderFocusSpeed { nullptr };
    QPushButton* btnFocusNear { nullptr };
    QPushButton* btnFocusFar { nullptr };
    QComboBox* cmbNearLimit { nullptr };

    bool m_updatingFromTelemetry { false };
};

} // namespace ViscaApp
