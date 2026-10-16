#pragma once

/// @file OpticsTab.h
/// @brief Optics, PTZ motion, autofocus, and zoom telemetry tab for Fujinon SX800.

#include "FujinonTypes.h"
#include "QPelcoDFujinonDevice.h"

#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace FujinonApp {

/// @class OpticsTab
/// @brief Tab providing optical zoom, focus, autofocus, and PTZ motion controls.
class OpticsTab : public QWidget {
    Q_OBJECT

public:
    explicit OpticsTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent = nullptr);
    ~OpticsTab() override = default;

public slots:
    /// @brief Update indicators and coordinates from base Pelco-D status.
    void updateBaseStatus(const PelcoD::DeviceStatus& status);
    /// @brief Update indicators from latest Fujinon telemetry snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    void handleZoomInPressed();
    void handleZoomInReleased();
    void handleZoomOutPressed();
    void handleZoomOutReleased();
    void handleFocusNearPressed();
    void handleFocusNearReleased();
    void handleFocusFarPressed();
    void handleFocusFarReleased();

    void handleSetZoomPos();
    void handleQueryZoom();
    void handleSetFocusPos();
    void handleQueryFocus();

    void handleAfModeChanged(int index);
    void handleOnePushAf();
    void handleAfSensChanged(int index);
    void handleAfAreaChanged(int index);

    void handleZoomSpeedChanged(int index);
    void handleFocusSpeedChanged(int index);
    void handleDzModeChanged(int index);
    void handleDzMagChanged(int index);
    void handleQueryZoomFocusEx();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    // PTZ buttons
    QPushButton* btnZoomIn { nullptr };
    QPushButton* btnZoomOut { nullptr };
    QPushButton* btnFocusNear { nullptr };
    QPushButton* btnFocusFar { nullptr };

    // Coordinates
    QSpinBox* spinZoomPos { nullptr };
    QSlider* sliderZoomPos { nullptr };
    QPushButton* btnSetZoomPos { nullptr };
    QPushButton* btnQueryZoom { nullptr };

    QSpinBox* spinFocusPos { nullptr };
    QSlider* sliderFocusPos { nullptr };
    QPushButton* btnSetFocusPos { nullptr };
    QPushButton* btnQueryFocus { nullptr };

    // Telemetry display
    QLabel* lblFocalLength { nullptr };
    QProgressBar* barFocalLength { nullptr };
    QLabel* lblZoomVal { nullptr };
    QLabel* lblFocusVal { nullptr };
    QLabel* lblSerial { nullptr };
    QLabel* lblFirmware { nullptr };
    QLabel* lblLensStatus { nullptr };

    // Auto Focus
    QComboBox* cmbAfMode { nullptr };
    QPushButton* btnOnePushAf { nullptr };
    QComboBox* cmbAfSensitivity { nullptr };
    QComboBox* cmbAfArea { nullptr };

    // Speeds & Digital Zoom
    QComboBox* cmbZoomSpeed { nullptr };
    QComboBox* cmbFocusSpeed { nullptr };
    QComboBox* cmbDzMode { nullptr };
    QComboBox* cmbDzMag { nullptr };
    QPushButton* btnQueryZfEx { nullptr };
};

} // namespace FujinonApp
