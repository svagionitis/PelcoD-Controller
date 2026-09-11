#pragma once

/// @file PtzControlTab.h
/// @brief Interactive PTZ directional controls, speeds, and absolute angle positioning.

#include "DeviceStatus.h"
#include "QPelcoDDevice.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class PtzControlTab
/// @brief Graphical PTZ control pad with continuous motion and coordinate displays.
class PtzControlTab : public QWidget {
    Q_OBJECT

public:
    explicit PtzControlTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~PtzControlTab() override = default;

public slots:
    void updateTelemetry(const PelcoD::DeviceStatus& status);

private slots:
    void handleDpadPressed(int panDir, int tiltDir);
    void handleDpadReleased();
    void handleSetPanAngle();
    void handleSetTiltAngle();
    void handleSetZoomPosition();

private:
    void setupUi();
    void createDpad(QGridLayout* layout);
    void createOptics(QVBoxLayout* layout);
    void createPositioning(QVBoxLayout* layout);

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    // D-Pad buttons
    QPushButton* btnUp { nullptr };
    QPushButton* btnDown { nullptr };
    QPushButton* btnLeft { nullptr };
    QPushButton* btnRight { nullptr };
    QPushButton* btnUpLeft { nullptr };
    QPushButton* btnUpRight { nullptr };
    QPushButton* btnDownLeft { nullptr };
    QPushButton* btnDownRight { nullptr };
    QPushButton* btnStop { nullptr };

    // Speeds
    QSlider* sliderPanSpeed { nullptr };
    QSlider* sliderTiltSpeed { nullptr };
    QLabel* lblPanSpeedVal { nullptr };
    QLabel* lblTiltSpeedVal { nullptr };
    QCheckBox* chkTurboPan { nullptr };

    // Optics buttons
    QPushButton* btnZoomTele { nullptr };
    QPushButton* btnZoomWide { nullptr };
    QPushButton* btnFocusNear { nullptr };
    QPushButton* btnFocusFar { nullptr };
    QPushButton* btnIrisOpen { nullptr };
    QPushButton* btnIrisClose { nullptr };

    // Absolute positioning
    QDoubleSpinBox* spinPanAngle { nullptr };
    QDoubleSpinBox* spinTiltAngle { nullptr };
    QSpinBox* spinZoomPos { nullptr };

    // Telemetry labels
    QLabel* lblTelemPan { nullptr };
    QLabel* lblTelemTilt { nullptr };
    QLabel* lblTelemZoom { nullptr };
};

} // namespace PelcoDApp
