/// @file TacticalOverlaysDialog.cpp
/// @brief Modal dialog for configuring tactical target tracking overlays (breadcrumbs and predictive lead vector).

#include "TacticalOverlaysDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace PelcoDApp {

TacticalOverlaysDialog::TacticalOverlaysDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void TacticalOverlaysDialog::setupUi()
{
    setWindowTitle(tr("Tactical Overlays Configuration"));
    resize(480, 440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- 1. Trajectory Breadcrumbs Group ---
    grpBreadcrumbs = new QGroupBox(tr("Historical Trajectory Breadcrumbs"), this);
    grpBreadcrumbs->setCheckable(true);
    grpBreadcrumbs->setChecked(true);
    auto* breadcrumbsForm = new QFormLayout(grpBreadcrumbs);
    breadcrumbsForm->setSpacing(8);

    spinDurationSec = new QDoubleSpinBox(grpBreadcrumbs);
    spinDurationSec->setRange(0.2, 10.0);
    spinDurationSec->setSingleStep(0.2);
    spinDurationSec->setValue(2.0);
    spinDurationSec->setSuffix(tr(" s"));
    spinDurationSec->setToolTip(tr("Physical wall-clock time window for trajectory breadcrumb retention"));
    breadcrumbsForm->addRow(tr("History Duration:"), spinDurationSec);

    spinMaxCapacity = new QSpinBox(grpBreadcrumbs);
    spinMaxCapacity->setRange(10, 200);
    spinMaxCapacity->setSingleStep(5);
    spinMaxCapacity->setValue(60);
    spinMaxCapacity->setSuffix(tr(" pts"));
    spinMaxCapacity->setToolTip(tr("Maximum number of history points preserved in the tracking ring buffer"));
    breadcrumbsForm->addRow(tr("Max Capacity:"), spinMaxCapacity);

    chkSmoothSpline = new QCheckBox(tr("Catmull-Rom Spline Smoothing"), grpBreadcrumbs);
    chkSmoothSpline->setChecked(true);
    chkSmoothSpline->setToolTip(
        tr("Interpolates curved motion path between points to eliminate pixel quantization jitter"));
    breadcrumbsForm->addRow(QString(), chkSmoothSpline);

    chkSpeedGradient = new QCheckBox(tr("Speed-Based Thermal Gradient"), grpBreadcrumbs);
    chkSpeedGradient->setChecked(true);
    chkSpeedGradient->setToolTip(
        tr("Renders color gradient from green (cruising) through amber to red (accelerating)"));
    breadcrumbsForm->addRow(QString(), chkSpeedGradient);

    mainLayout->addWidget(grpBreadcrumbs);

    // --- 2. Predictive Lead Vector Group ---
    grpPredictiveLead = new QGroupBox(tr("Predictive Lead Vector & Reticle"), this);
    grpPredictiveLead->setCheckable(true);
    grpPredictiveLead->setChecked(true);
    auto* leadForm = new QFormLayout(grpPredictiveLead);
    leadForm->setSpacing(8);

    spinLookaheadHorizon = new QDoubleSpinBox(grpPredictiveLead);
    spinLookaheadHorizon->setRange(0.1, 5.0);
    spinLookaheadHorizon->setSingleStep(0.1);
    spinLookaheadHorizon->setValue(1.5);
    spinLookaheadHorizon->setSuffix(tr(" s"));
    spinLookaheadHorizon->setToolTip(
        tr("Forward time horizon for kinematic trajectory projection and interception reticle"));
    leadForm->addRow(tr("Lookahead Horizon:"), spinLookaheadHorizon);

    chkCurvilinear = new QCheckBox(tr("CTRA Curvilinear Turn Prediction"), grpPredictiveLead);
    chkCurvilinear->setChecked(true);
    chkCurvilinear->setToolTip(tr("Projects circular arc using instantaneous turn rate during angular maneuvers"));
    leadForm->addRow(QString(), chkCurvilinear);

    chkUncertaintyEllipse = new QCheckBox(tr("Kalman Uncertainty Covariance Ellipse (2σ)"), grpPredictiveLead);
    chkUncertaintyEllipse->setChecked(true);
    chkUncertaintyEllipse->setToolTip(
        tr("Renders 95% confidence covariance error ellipse around predicted interception reticle"));
    leadForm->addRow(QString(), chkUncertaintyEllipse);

    chkBoresightLeadMarker = new QCheckBox(tr("Show Mechanical PTZ Boresight Lead Marker"), grpPredictiveLead);
    chkBoresightLeadMarker->setChecked(true);
    chkBoresightLeadMarker->setToolTip(tr("Displays tactical diamond marker showing camera head steering setpoint"));
    leadForm->addRow(QString(), chkBoresightLeadMarker);

    mainLayout->addWidget(grpPredictiveLead);

    mainLayout->addStretch();

    // --- 3. Action Buttons ---
    auto* buttonLayout = new QHBoxLayout();
    btnReset = new QPushButton(tr("Reset Defaults"), this);
    btnApply = new QPushButton(tr("Apply"), this);
    btnOk = new QPushButton(tr("OK"), this);
    btnCancel = new QPushButton(tr("Cancel"), this);

    btnOk->setDefault(true);

    buttonLayout->addWidget(btnReset);
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnApply);
    buttonLayout->addWidget(btnOk);
    buttonLayout->addWidget(btnCancel);
    mainLayout->addLayout(buttonLayout);

    connect(btnReset, &QPushButton::clicked, this, &TacticalOverlaysDialog::onResetToDefaults);
    connect(btnApply, &QPushButton::clicked, this, &TacticalOverlaysDialog::onApplyClicked);
    connect(btnOk, &QPushButton::clicked, this, [this]() {
        onApplyClicked();
        accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void TacticalOverlaysDialog::setTrajectoryConfig(const Video::TrajectoryConfig& config)
{
    grpBreadcrumbs->setChecked(config.enabled);
    spinDurationSec->setValue(config.maxDurationSec);
    spinMaxCapacity->setValue(config.maxPoints);
    chkSmoothSpline->setChecked(config.smoothSpline);
    chkSpeedGradient->setChecked(config.speedGradient);
}

Video::TrajectoryConfig TacticalOverlaysDialog::trajectoryConfig() const
{
    Video::TrajectoryConfig cfg;
    cfg.enabled = grpBreadcrumbs->isChecked();
    cfg.maxDurationSec = spinDurationSec->value();
    cfg.maxPoints = spinMaxCapacity->value();
    cfg.smoothSpline = chkSmoothSpline->isChecked();
    cfg.speedGradient = chkSpeedGradient->isChecked();
    return cfg;
}

void TacticalOverlaysDialog::setPredictiveLeadConfig(const Video::PredictiveLeadConfig& config)
{
    grpPredictiveLead->setChecked(config.enabled);
    spinLookaheadHorizon->setValue(config.lookaheadSeconds);
    chkCurvilinear->setChecked(config.curvilinearPrediction);
    chkUncertaintyEllipse->setChecked(config.showUncertaintyEllipse);
    chkBoresightLeadMarker->setChecked(config.showBoresightLeadSetpoint);
}

Video::PredictiveLeadConfig TacticalOverlaysDialog::predictiveLeadConfig() const
{
    Video::PredictiveLeadConfig cfg;
    cfg.enabled = grpPredictiveLead->isChecked();
    cfg.lookaheadSeconds = spinLookaheadHorizon->value();
    cfg.curvilinearPrediction = chkCurvilinear->isChecked();
    cfg.showUncertaintyEllipse = chkUncertaintyEllipse->isChecked();
    cfg.showBoresightLeadSetpoint = chkBoresightLeadMarker->isChecked();
    return cfg;
}

void TacticalOverlaysDialog::onResetToDefaults()
{
    grpBreadcrumbs->setChecked(true);
    spinDurationSec->setValue(2.0);
    spinMaxCapacity->setValue(60);
    chkSmoothSpline->setChecked(true);
    chkSpeedGradient->setChecked(true);

    grpPredictiveLead->setChecked(true);
    spinLookaheadHorizon->setValue(1.5);
    chkCurvilinear->setChecked(true);
    chkUncertaintyEllipse->setChecked(true);
    chkBoresightLeadMarker->setChecked(true);
}

void TacticalOverlaysDialog::onApplyClicked()
{
    emit overlaysConfigChanged(trajectoryConfig(), predictiveLeadConfig());
}

} // namespace PelcoDApp
