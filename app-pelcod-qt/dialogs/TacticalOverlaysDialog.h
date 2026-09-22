#pragma once

/// @file TacticalOverlaysDialog.h
/// @brief Modal dialog for configuring tactical target tracking overlays (breadcrumbs and predictive lead vector).

#include "DecoderTypes.h"

#include <QCheckBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>

namespace PelcoDApp {

/// @class TacticalOverlaysDialog
/// @brief Configuration dialog for trajectory breadcrumbs and predictive lead vector parameters.
class TacticalOverlaysDialog : public QDialog {
    Q_OBJECT

public:
    explicit TacticalOverlaysDialog(QWidget* parent = nullptr);
    ~TacticalOverlaysDialog() override = default;

    /// @brief Set the initial trajectory configuration.
    /// @param[in] config Trajectory breadcrumbs configuration.
    void setTrajectoryConfig(const PelcoD::Video::TrajectoryConfig& config);

    /// @brief Retrieve the configured trajectory settings.
    /// @return TrajectoryConfig descriptor.
    [[nodiscard]] PelcoD::Video::TrajectoryConfig trajectoryConfig() const;

    /// @brief Set the initial predictive lead configuration.
    /// @param[in] config Predictive lead vector configuration.
    void setPredictiveLeadConfig(const PelcoD::Video::PredictiveLeadConfig& config);

    /// @brief Retrieve the configured predictive lead settings.
    /// @return PredictiveLeadConfig descriptor.
    [[nodiscard]] PelcoD::Video::PredictiveLeadConfig predictiveLeadConfig() const;

signals:
    /// @brief Emitted when the user applies overlay settings.
    /// @param[in] trajCfg Updated trajectory settings.
    /// @param[in] leadCfg Updated predictive lead settings.
    void overlaysConfigChanged(const PelcoD::Video::TrajectoryConfig& trajCfg,
        const PelcoD::Video::PredictiveLeadConfig& leadCfg);

private slots:
    void onResetToDefaults();
    void onApplyClicked();

private:
    void setupUi();

    // Breadcrumbs controls
    QGroupBox* grpBreadcrumbs { nullptr };
    QCheckBox* chkBreadcrumbsEnabled { nullptr };
    QDoubleSpinBox* spinDurationSec { nullptr };
    QSpinBox* spinMaxCapacity { nullptr };
    QCheckBox* chkSmoothSpline { nullptr };
    QCheckBox* chkSpeedGradient { nullptr };

    // Predictive Lead controls
    QGroupBox* grpPredictiveLead { nullptr };
    QCheckBox* chkLeadEnabled { nullptr };
    QDoubleSpinBox* spinLookaheadHorizon { nullptr };
    QCheckBox* chkCurvilinear { nullptr };
    QCheckBox* chkUncertaintyEllipse { nullptr };
    QCheckBox* chkBoresightLeadMarker { nullptr };

    QPushButton* btnReset { nullptr };
    QPushButton* btnApply { nullptr };
    QPushButton* btnOk { nullptr };
    QPushButton* btnCancel { nullptr };
};

} // namespace PelcoDApp
