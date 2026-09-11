#pragma once

/// @file PresetsTab.h
/// @brief Preset management dashboard tab (Set, Go To, Clear, Scan).

#include "QPelcoDDevice.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
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

private:
    void setupUi();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    QSpinBox* spinPresetId { nullptr };
    QPushButton* btnGoTo { nullptr };
    QPushButton* btnSet { nullptr };
    QPushButton* btnClear { nullptr };

    QSpinBox* spinDwell { nullptr };
    QPushButton* btnPresetScan { nullptr };
};

} // namespace PelcoDApp
