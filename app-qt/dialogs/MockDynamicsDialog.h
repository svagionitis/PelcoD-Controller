#pragma once

/// @file MockDynamicsDialog.h
/// @brief Dialog for configuring simulated PTZ physical kinematics, link latency, and packet loss.

#include "KinematicsSimulator.h"
#include "LatencyPipeline.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>

namespace PelcoDApp {

/// @class MockDynamicsDialog
/// @brief Modal dialog allowing user to configure simulated physics and latency for Mock Pelco-D device.
class MockDynamicsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MockDynamicsDialog(QWidget* parent = nullptr);
    ~MockDynamicsDialog() override = default;

    /// @brief Set initial kinematics settings to display in UI.
    /// @param[in] config Kinematics settings.
    void setKinematicsConfig(const PelcoD::KinematicsConfig& config);

    /// @brief Retrieve kinematics settings configured by user.
    /// @return KinematicsConfig descriptor.
    [[nodiscard]] PelcoD::KinematicsConfig kinematicsConfig() const;

    /// @brief Set initial latency settings to display in UI.
    /// @param[in] config Latency settings.
    void setLatencyConfig(const PelcoD::LatencyConfig& config);

    /// @brief Retrieve latency settings configured by user.
    /// @return LatencyConfig descriptor.
    [[nodiscard]] PelcoD::LatencyConfig latencyConfig() const;

private slots:
    void onKinematicsPresetChanged(int index);
    void onLatencyPresetChanged(int index);
    void onResetToIdeal();

private:
    void setupUi();

    // Kinematics controls
    QGroupBox* grpKinematics { nullptr };
    QComboBox* cmbKinematicsPreset { nullptr };
    QDoubleSpinBox* spinMaxPanSpeed { nullptr };
    QDoubleSpinBox* spinMaxTiltSpeed { nullptr };
    QDoubleSpinBox* spinPanAccel { nullptr };
    QDoubleSpinBox* spinTiltAccel { nullptr };
    QDoubleSpinBox* spinZoomTransit { nullptr };

    // Latency controls
    QGroupBox* grpLatency { nullptr };
    QComboBox* cmbLatencyPreset { nullptr };
    QSpinBox* spinBaseLatency { nullptr };
    QSpinBox* spinJitter { nullptr };
    QDoubleSpinBox* spinPacketDrop { nullptr };

    // Action buttons
    QPushButton* btnResetIdeal { nullptr };
    QPushButton* btnApply { nullptr };
    QPushButton* btnCancel { nullptr };
};

} // namespace PelcoDApp
