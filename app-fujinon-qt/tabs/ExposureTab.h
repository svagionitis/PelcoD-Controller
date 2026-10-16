#pragma once

/// @file ExposureTab.h
/// @brief Exposure, iris, shutter, ISO gain, and AGC/BLC controls for Fujinon SX800.

#include "FujinonTypes.h"
#include "QPelcoDFujinonDevice.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace FujinonApp {

/// @class ExposureTab
/// @brief Tab providing auto/manual iris, shutter speeds, sensor gain, and scene presets.
class ExposureTab : public QWidget {
    Q_OBJECT

public:
    explicit ExposureTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent = nullptr);
    ~ExposureTab() override = default;

public slots:
    /// @brief Update indicators from latest Fujinon telemetry snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    void handleAutoIrisChanged(int index);
    void handleSetManualIris();
    void handleQueryIris();

    void handleShutterLimitChanged(int index);
    void handleSetManualShutter();
    void handleQueryShutter();

    void handleSetManualIso();
    void handleQueryIso();

    void handleAgcChanged(int index);
    void handleBlcChanged(int index);
    void handlePhotoModeChanged(int index);

    void handleQueryManualSettings();
    void handleQueryPhotoSettings();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    // Iris
    QComboBox* cmbAutoIris { nullptr };
    QSpinBox* spinManualIris { nullptr };
    QSlider* sliderManualIris { nullptr };
    QPushButton* btnSetManualIris { nullptr };
    QPushButton* btnQueryIris { nullptr };

    // Shutter
    QComboBox* cmbShutterLimit { nullptr };
    QSpinBox* spinManualShutter { nullptr };
    QSlider* sliderManualShutter { nullptr };
    QPushButton* btnSetManualShutter { nullptr };
    QPushButton* btnQueryShutter { nullptr };

    // ISO
    QSpinBox* spinManualIso { nullptr };
    QSlider* sliderManualIso { nullptr };
    QPushButton* btnSetManualIso { nullptr };
    QPushButton* btnQueryIso { nullptr };

    // AGC & BLC & Photo Preset
    QComboBox* cmbAgc { nullptr };
    QComboBox* cmbBlc { nullptr };
    QComboBox* cmbPhotoMode { nullptr };

    // Status & Queries
    QPushButton* btnQueryManual { nullptr };
    QPushButton* btnQueryPhoto { nullptr };
    QLabel* lblCurrentExposure { nullptr };
};

} // namespace FujinonApp
