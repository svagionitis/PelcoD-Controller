#pragma once

/// @file PresetsTab.h
/// @brief Preset management dashboard tab (Set, Go To, Clear, and Patrol Sequence Tour).

#include "QPatrolController.h"
#include "QPelcoDDevice.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class PresetsTab
/// @brief UI tab for configuring, recalling, and touring stored PTZ preset coordinates.
class PresetsTab : public QWidget {
    Q_OBJECT

public:
    explicit PresetsTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~PresetsTab() override = default;

private slots:
    void handleGoToPreset();
    void handleSetPreset();
    void handleClearPreset();
    void handleQuickPreset(int presetId);

    // Patrol sequence slots
    void handleAddPatrolStep();
    void handleRemovePatrolStep();
    void handleMoveStepUp();
    void handleMoveStepDown();
    void handleClearPatrol();
    void handleStartPatrol();
    void handlePausePatrol();
    void handleStopPatrol();
    void handleNextStep();
    void handleLoopToggled(bool checked);

    void onPatrolStateChanged(PelcoD::PatrolState state);
    void onPatrolStepChanged(int stepIndex, int presetId, const QString& name);
    void onPatrolDwellTick(int stepIndex, int remainingSeconds);
    void onPatrolTourFinished();

private:
    void setupUi();
    void refreshPatrolTable();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };
    PelcoDQt::QPatrolController* m_patrol { nullptr };

    // Standard Presets
    QSpinBox* spinPresetId { nullptr };
    QPushButton* btnGoTo { nullptr };
    QPushButton* btnSet { nullptr };
    QPushButton* btnClear { nullptr };

    // Patrol Tour UI
    QTableWidget* tablePatrol { nullptr };
    QSpinBox* spinStepPreset { nullptr };
    QSpinBox* spinStepDwell { nullptr };
    QLineEdit* editStepName { nullptr };
    QPushButton* btnAddStep { nullptr };
    QPushButton* btnRemoveStep { nullptr };
    QPushButton* btnMoveUp { nullptr };
    QPushButton* btnMoveDown { nullptr };
    QPushButton* btnClearPatrol { nullptr };

    QPushButton* btnStartPatrol { nullptr };
    QPushButton* btnPausePatrol { nullptr };
    QPushButton* btnStopPatrol { nullptr };
    QPushButton* btnNextStep { nullptr };
    QCheckBox* chkLoop { nullptr };

    QLabel* lblPatrolStatus { nullptr };
    QProgressBar* progressDwell { nullptr };
};

} // namespace PelcoDApp
