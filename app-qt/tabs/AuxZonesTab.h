#pragma once

/// @file AuxZonesTab.h
/// @brief Auxiliary relay outputs, sector zones, and recorded PTZ patterns tab.

#include "QPelcoDDevice.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class AuxZonesTab
/// @brief UI tab managing auxiliary relays, sector zone boundaries, and patterns.
class AuxZonesTab : public QWidget {
    Q_OBJECT

public:
    explicit AuxZonesTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~AuxZonesTab() override = default;

private slots:
    void handleAuxSet();
    void handleAuxClear();
    void handleZoneStart();
    void handleZoneEnd();
    void handleZoneScanOn();
    void handleZoneScanOff();
    void handlePatternRecord();
    void handlePatternStop();
    void handlePatternRun();

private:
    void setupUi();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    // Aux
    QSpinBox* spinAuxId { nullptr };
    QPushButton* btnAuxSet { nullptr };
    QPushButton* btnAuxClear { nullptr };

    // Zones
    QSpinBox* spinZoneId { nullptr };
    QPushButton* btnZoneStart { nullptr };
    QPushButton* btnZoneEnd { nullptr };
    QPushButton* btnZoneScanOn { nullptr };
    QPushButton* btnZoneScanOff { nullptr };

    // Patterns
    QSpinBox* spinPatternId { nullptr };
    QPushButton* btnPatternRecord { nullptr };
    QPushButton* btnPatternStop { nullptr };
    QPushButton* btnPatternRun { nullptr };
};

} // namespace PelcoDApp
