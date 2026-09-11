/// @file PresetsTab.cpp
/// @brief Implementation of presets management tab.

#include "PresetsTab.h"

namespace PelcoDApp {

PresetsTab::PresetsTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
}

void PresetsTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

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

    // Group 4: Preset Tour / Scan
    auto* grpScan = new QGroupBox(tr("Preset Tour / Scan"), this);
    auto* layoutScan = new QHBoxLayout(grpScan);

    layoutScan->addWidget(new QLabel(tr("Dwell Time (seconds):")));
    spinDwell = new QSpinBox();
    spinDwell->setRange(1, 255);
    spinDwell->setValue(5);
    layoutScan->addWidget(spinDwell);

    btnPresetScan = new QPushButton(tr("Start Preset Scan"));
    connect(btnPresetScan, &QPushButton::clicked, this, [this] { m_device->goToPreset(spinPresetId->value()); });
    layoutScan->addWidget(btnPresetScan);
    layoutScan->addStretch();
    mainLayout->addWidget(grpScan);

    mainLayout->addStretch();

    // Connections
    connect(btnGoTo, &QPushButton::clicked, this, &PresetsTab::handleGoToPreset);
    connect(btnSet, &QPushButton::clicked, this, &PresetsTab::handleSetPreset);
    connect(btnClear, &QPushButton::clicked, this, &PresetsTab::handleClearPreset);
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
