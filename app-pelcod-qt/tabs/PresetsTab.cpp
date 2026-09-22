/// @file PresetsTab.cpp
/// @brief Implementation of presets management and automated patrol sequence tour tab.

#include "PresetsTab.h"

namespace PelcoDApp {

PresetsTab::PresetsTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
    , m_patrol { new PelcoDQt::QPatrolController(device, this) }
{
    setupUi();

    // Default sample patrol steps for instant testing
    m_patrol->addStep(1, 5, tr("Main Entrance"));
    m_patrol->addStep(2, 8, tr("Perimeter West"));
    m_patrol->addStep(3, 5, tr("Loading Bay"));
    refreshPatrolTable();
}

void PresetsTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Group 1: General Preset Command
    auto* grpGeneral = new QGroupBox(tr("Preset Command (1 - 255)"), this);
    auto* layoutGeneral = new QHBoxLayout(grpGeneral);

    layoutGeneral->addWidget(new QLabel(tr("Preset ID:")));
    spinPresetId = new QSpinBox();
    spinPresetId->setRange(1, 255);
    spinPresetId->setValue(1);
    layoutGeneral->addWidget(spinPresetId);

    btnGoTo = new QPushButton(tr("Go To Preset"));
    btnGoTo->setObjectName("btnPrimary");
    btnSet = new QPushButton(tr("Set Current Position"));
    btnClear = new QPushButton(tr("Clear Preset"));
    btnClear->setObjectName("btnDanger");

    layoutGeneral->addWidget(btnGoTo);
    layoutGeneral->addWidget(btnSet);
    layoutGeneral->addWidget(btnClear);
    layoutGeneral->addStretch();
    mainLayout->addWidget(grpGeneral);

    // Group 2: Quick Presets (1 to 8)
    auto* grpQuick = new QGroupBox(tr("Quick Recall Presets"), this);
    auto* gridQuick = new QGridLayout(grpQuick);

    for (int i = 1; i <= 8; ++i) {
        auto* btn = new QPushButton(tr("Preset %1").arg(i));
        connect(btn, &QPushButton::clicked, this, [this, i] { handleQuickPreset(i); });
        gridQuick->addWidget(btn, (i - 1) / 4, (i - 1) % 4);
    }
    mainLayout->addWidget(grpQuick);

    // Group 3: Special Presets
    auto* grpSpecial = new QGroupBox(tr("Special / Predefined Presets"), this);
    auto* layoutSpecial = new QHBoxLayout(grpSpecial);

    auto* btnMenu95 = new QPushButton(tr("Preset 95 (OSD Menu)"));
    auto* btnFlip = new QPushButton(tr("Preset 33 (180° Flip)"));
    auto* btnZero = new QPushButton(tr("Preset 34 (Zero Pan)"));

    connect(btnMenu95, &QPushButton::clicked, this, [this] { m_device->goToPreset(95); });
    connect(btnFlip, &QPushButton::clicked, this, [this] { m_device->flip180(); });
    connect(btnZero, &QPushButton::clicked, this, [this] { m_device->zeroPan(); });

    layoutSpecial->addWidget(btnMenu95);
    layoutSpecial->addWidget(btnFlip);
    layoutSpecial->addWidget(btnZero);
    layoutSpecial->addStretch();
    mainLayout->addWidget(grpSpecial);

    // Group 4: Automated Preset Tour / Patrol Sequence
    auto* grpTour = new QGroupBox(tr("Automated Preset Tour / Patrol Sequence"), this);
    auto* layoutTour = new QVBoxLayout(grpTour);
    layoutTour->setSpacing(8);

    // Top: Step Configuration Inputs
    auto* stepInputLayout = new QHBoxLayout();
    stepInputLayout->addWidget(new QLabel(tr("Preset:")));
    spinStepPreset = new QSpinBox();
    spinStepPreset->setRange(1, 255);
    spinStepPreset->setValue(1);
    stepInputLayout->addWidget(spinStepPreset);

    stepInputLayout->addWidget(new QLabel(tr("Dwell (s):")));
    spinStepDwell = new QSpinBox();
    spinStepDwell->setRange(1, 3600);
    spinStepDwell->setValue(5);
    stepInputLayout->addWidget(spinStepDwell);

    stepInputLayout->addWidget(new QLabel(tr("Name / Label:")));
    editStepName = new QLineEdit();
    editStepName->setPlaceholderText(tr("e.g. North Gate"));
    stepInputLayout->addWidget(editStepName);

    btnAddStep = new QPushButton(tr("+ Add Step"));
    btnAddStep->setObjectName("btnPrimary");
    btnRemoveStep = new QPushButton(tr("- Remove"));
    btnMoveUp = new QPushButton(tr("▲ Up"));
    btnMoveDown = new QPushButton(tr("▼ Down"));
    btnClearPatrol = new QPushButton(tr("Clear All"));

    stepInputLayout->addWidget(btnAddStep);
    stepInputLayout->addWidget(btnRemoveStep);
    stepInputLayout->addWidget(btnMoveUp);
    stepInputLayout->addWidget(btnMoveDown);
    stepInputLayout->addWidget(btnClearPatrol);
    layoutTour->addLayout(stepInputLayout);

    // Middle: Table of Steps
    tablePatrol = new QTableWidget(0, 4, this);
    tablePatrol->setHorizontalHeaderLabels({ tr("#"), tr("Preset"), tr("Name / Location"), tr("Dwell Time (s)") });
    tablePatrol->horizontalHeader()->setStretchLastSection(true);
    tablePatrol->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tablePatrol->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tablePatrol->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tablePatrol->setSelectionBehavior(QAbstractItemView::SelectRows);
    tablePatrol->setSelectionMode(QAbstractItemView::SingleSelection);
    tablePatrol->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tablePatrol->setMaximumHeight(160);
    layoutTour->addWidget(tablePatrol);

    // Bottom: Tour Playback Controls & Status
    auto* playLayout = new QHBoxLayout();
    btnStartPatrol = new QPushButton(tr("▶ Start Tour"));
    btnStartPatrol->setObjectName("btnSuccess");
    btnPausePatrol = new QPushButton(tr("⏸ Pause"));
    btnPausePatrol->setEnabled(false);
    btnStopPatrol = new QPushButton(tr("⏹ Stop"));
    btnStopPatrol->setObjectName("btnDanger");
    btnStopPatrol->setEnabled(false);
    btnNextStep = new QPushButton(tr("⏭ Skip Next"));
    btnNextStep->setEnabled(false);

    chkLoop = new QCheckBox(tr("Continuous Loop"), this);
    chkLoop->setChecked(true);

    playLayout->addWidget(btnStartPatrol);
    playLayout->addWidget(btnPausePatrol);
    playLayout->addWidget(btnStopPatrol);
    playLayout->addWidget(btnNextStep);
    playLayout->addWidget(chkLoop);
    playLayout->addStretch();
    layoutTour->addLayout(playLayout);

    // Progress Bar & Status
    auto* statusLayout = new QHBoxLayout();
    lblPatrolStatus = new QLabel(tr("Tour Status: Ready (Idle)"));
    progressDwell = new QProgressBar();
    progressDwell->setRange(0, 100);
    progressDwell->setValue(0);
    progressDwell->setTextVisible(false);
    progressDwell->setMaximumHeight(12);

    statusLayout->addWidget(lblPatrolStatus);
    statusLayout->addWidget(progressDwell);
    layoutTour->addLayout(statusLayout);

    mainLayout->addWidget(grpTour);
    mainLayout->addStretch();

    // Standard preset connections
    connect(btnGoTo, &QPushButton::clicked, this, &PresetsTab::handleGoToPreset);
    connect(btnSet, &QPushButton::clicked, this, &PresetsTab::handleSetPreset);
    connect(btnClear, &QPushButton::clicked, this, &PresetsTab::handleClearPreset);

    // Patrol connections
    connect(btnAddStep, &QPushButton::clicked, this, &PresetsTab::handleAddPatrolStep);
    connect(btnRemoveStep, &QPushButton::clicked, this, &PresetsTab::handleRemovePatrolStep);
    connect(btnMoveUp, &QPushButton::clicked, this, &PresetsTab::handleMoveStepUp);
    connect(btnMoveDown, &QPushButton::clicked, this, &PresetsTab::handleMoveStepDown);
    connect(btnClearPatrol, &QPushButton::clicked, this, &PresetsTab::handleClearPatrol);

    connect(btnStartPatrol, &QPushButton::clicked, this, &PresetsTab::handleStartPatrol);
    connect(btnPausePatrol, &QPushButton::clicked, this, &PresetsTab::handlePausePatrol);
    connect(btnStopPatrol, &QPushButton::clicked, this, &PresetsTab::handleStopPatrol);
    connect(btnNextStep, &QPushButton::clicked, this, &PresetsTab::handleNextStep);
    connect(chkLoop, &QCheckBox::toggled, this, &PresetsTab::handleLoopToggled);

    // Patrol controller signals
    connect(m_patrol, &PelcoDQt::QPatrolController::stateChanged, this, &PresetsTab::onPatrolStateChanged);
    connect(m_patrol, &PelcoDQt::QPatrolController::stepChanged, this, &PresetsTab::onPatrolStepChanged);
    connect(m_patrol, &PelcoDQt::QPatrolController::dwellTick, this, &PresetsTab::onPatrolDwellTick);
    connect(m_patrol, &PelcoDQt::QPatrolController::tourFinished, this, &PresetsTab::onPatrolTourFinished);
}

void PresetsTab::refreshPatrolTable()
{
    const auto steps = m_patrol->steps();
    tablePatrol->setRowCount(static_cast<int>(steps.size()));

    for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
        const auto& s = steps[static_cast<std::size_t>(i)];
        tablePatrol->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        tablePatrol->setItem(i, 1, new QTableWidgetItem(tr("Preset %1").arg(s.presetId)));
        tablePatrol->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(s.name)));
        tablePatrol->setItem(i, 3, new QTableWidgetItem(tr("%1 s").arg(s.dwellTimeSeconds)));
    }
}

void PresetsTab::handleAddPatrolStep()
{
    const int preset = spinStepPreset->value();
    const int dwell = spinStepDwell->value();
    const QString name = editStepName->text().trimmed();

    m_patrol->addStep(preset, dwell, name.isEmpty() ? tr("Preset %1").arg(preset) : name);
    refreshPatrolTable();
    editStepName->clear();
}

void PresetsTab::handleRemovePatrolStep()
{
    const int row = tablePatrol->currentRow();
    if (row >= 0) {
        m_patrol->removeStep(row);
        refreshPatrolTable();
    }
}

void PresetsTab::handleMoveStepUp()
{
    const int row = tablePatrol->currentRow();
    if (row > 0) {
        m_patrol->moveStepUp(row);
        refreshPatrolTable();
        tablePatrol->selectRow(row - 1);
    }
}

void PresetsTab::handleMoveStepDown()
{
    const int row = tablePatrol->currentRow();
    if (row >= 0 && row + 1 < tablePatrol->rowCount()) {
        m_patrol->moveStepDown(row);
        refreshPatrolTable();
        tablePatrol->selectRow(row + 1);
    }
}

void PresetsTab::handleClearPatrol()
{
    m_patrol->clearSteps();
    refreshPatrolTable();
}

void PresetsTab::handleStartPatrol()
{
    if (m_patrol->isPaused()) {
        m_patrol->resume();
    } else {
        m_patrol->start();
    }
}

void PresetsTab::handlePausePatrol()
{
    m_patrol->pause();
}

void PresetsTab::handleStopPatrol()
{
    m_patrol->stop();
}

void PresetsTab::handleNextStep()
{
    m_patrol->nextStep();
}

void PresetsTab::handleLoopToggled(bool checked)
{
    m_patrol->setLoop(checked);
}

void PresetsTab::onPatrolStateChanged(PelcoD::PatrolState state)
{
    switch (state) {
    case PelcoD::PatrolState::Running:
        btnStartPatrol->setEnabled(false);
        btnPausePatrol->setEnabled(true);
        btnStopPatrol->setEnabled(true);
        btnNextStep->setEnabled(true);
        lblPatrolStatus->setText(tr("Tour Status: Patrolling..."));
        break;
    case PelcoD::PatrolState::Paused:
        btnStartPatrol->setEnabled(true);
        btnStartPatrol->setText(tr("▶ Resume"));
        btnPausePatrol->setEnabled(false);
        btnStopPatrol->setEnabled(true);
        btnNextStep->setEnabled(true);
        lblPatrolStatus->setText(tr("Tour Status: Paused"));
        break;
    case PelcoD::PatrolState::Idle:
        btnStartPatrol->setEnabled(true);
        btnStartPatrol->setText(tr("▶ Start Tour"));
        btnPausePatrol->setEnabled(false);
        btnStopPatrol->setEnabled(false);
        btnNextStep->setEnabled(false);
        lblPatrolStatus->setText(tr("Tour Status: Idle"));
        progressDwell->setValue(0);
        tablePatrol->clearSelection();
        break;
    }
}

void PresetsTab::onPatrolStepChanged(int stepIndex, int presetId, const QString& name)
{
    tablePatrol->selectRow(stepIndex);
    const int total = m_patrol->stepCount();
    lblPatrolStatus->setText(
        tr("Touring: Step %1/%2 — %3 (Preset %4)").arg(stepIndex + 1).arg(total).arg(name).arg(presetId));
}

void PresetsTab::onPatrolDwellTick(int stepIndex, int remainingSeconds)
{
    const auto steps = m_patrol->steps();
    if (static_cast<std::size_t>(stepIndex) < steps.size()) {
        const auto totalDwell = static_cast<int>(steps[static_cast<std::size_t>(stepIndex)].dwellTimeSeconds);
        const int pct = totalDwell > 0 ? ((totalDwell - remainingSeconds) * 100 / totalDwell) : 100;
        progressDwell->setValue(pct);
        lblPatrolStatus->setText(tr("Touring: Step %1/%2 — %3 (Preset %4) | Dwell: %5s remaining")
                                     .arg(stepIndex + 1)
                                     .arg(steps.size())
                                     .arg(QString::fromStdString(steps[static_cast<std::size_t>(stepIndex)].name))
                                     .arg(steps[static_cast<std::size_t>(stepIndex)].presetId)
                                     .arg(remainingSeconds));
    }
}

void PresetsTab::onPatrolTourFinished()
{
    lblPatrolStatus->setText(tr("Tour Status: Completed"));
    progressDwell->setValue(100);
}

void PresetsTab::handleGoToPreset()
{
    m_device->goToPreset(spinPresetId->value());
}

void PresetsTab::handleSetPreset()
{
    m_device->setPreset(spinPresetId->value());
}

void PresetsTab::handleClearPreset()
{
    m_device->clearPreset(spinPresetId->value());
}

void PresetsTab::handleQuickPreset(int presetId)
{
    m_device->goToPreset(presetId);
}

} // namespace PelcoDApp
