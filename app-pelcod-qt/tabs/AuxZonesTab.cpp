/// @file AuxZonesTab.cpp
/// @brief Implementation of auxiliaries, zones, and patterns tab.

#include "AuxZonesTab.h"

namespace PelcoDApp {

AuxZonesTab::AuxZonesTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
}

void AuxZonesTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // Group 1: Auxiliaries (1 - 8)
    auto* grpAux = new QGroupBox(tr("Auxiliary Relays / Switches (1 - 8)"), this);
    auto* layoutAux = new QHBoxLayout(grpAux);

    layoutAux->addWidget(new QLabel(tr("Aux ID:")));
    spinAuxId = new QSpinBox();
    spinAuxId->setRange(1, 8);
    spinAuxId->setValue(1);
    layoutAux->addWidget(spinAuxId);

    btnAuxSet = new QPushButton(tr("Set Auxiliary (On)"));
    btnAuxSet->setObjectName("btnPrimary");
    btnAuxClear = new QPushButton(tr("Clear Auxiliary (Off)"));

    layoutAux->addWidget(btnAuxSet);
    layoutAux->addWidget(btnAuxClear);
    layoutAux->addStretch();
    mainLayout->addWidget(grpAux);

    // Group 2: Zones (1 - 8)
    auto* grpZone = new QGroupBox(tr("Sector Zones (1 - 8)"), this);
    auto* layoutZone = new QHBoxLayout(grpZone);

    layoutZone->addWidget(new QLabel(tr("Zone ID:")));
    spinZoneId = new QSpinBox();
    spinZoneId->setRange(1, 8);
    spinZoneId->setValue(1);
    layoutZone->addWidget(spinZoneId);

    btnZoneStart = new QPushButton(tr("Set Zone Start (Left)"));
    btnZoneEnd = new QPushButton(tr("Set Zone End (Right)"));
    btnZoneScanOn = new QPushButton(tr("Zone Scan On"));
    btnZoneScanOff = new QPushButton(tr("Zone Scan Off"));

    layoutZone->addWidget(btnZoneStart);
    layoutZone->addWidget(btnZoneEnd);
    layoutZone->addWidget(btnZoneScanOn);
    layoutZone->addWidget(btnZoneScanOff);
    layoutZone->addStretch();
    mainLayout->addWidget(grpZone);

    // Group 3: Recorded Patterns (1 - 4)
    auto* grpPattern = new QGroupBox(tr("Recorded Guard Tour Patterns (1 - 4)"), this);
    auto* layoutPattern = new QHBoxLayout(grpPattern);

    layoutPattern->addWidget(new QLabel(tr("Pattern ID:")));
    spinPatternId = new QSpinBox();
    spinPatternId->setRange(1, 4);
    spinPatternId->setValue(1);
    layoutPattern->addWidget(spinPatternId);

    btnPatternRecord = new QPushButton(tr("Record Start"));
    btnPatternRecord->setObjectName("btnDanger");
    btnPatternStop = new QPushButton(tr("Record Stop"));
    btnPatternRun = new QPushButton(tr("Run Pattern"));
    btnPatternRun->setObjectName("btnPrimary");

    layoutPattern->addWidget(btnPatternRecord);
    layoutPattern->addWidget(btnPatternStop);
    layoutPattern->addWidget(btnPatternRun);
    layoutPattern->addStretch();
    mainLayout->addWidget(grpPattern);

    mainLayout->addStretch();

    // Signal Connections
    connect(btnAuxSet, &QPushButton::clicked, this, &AuxZonesTab::handleAuxSet);
    connect(btnAuxClear, &QPushButton::clicked, this, &AuxZonesTab::handleAuxClear);

    connect(btnZoneStart, &QPushButton::clicked, this, &AuxZonesTab::handleZoneStart);
    connect(btnZoneEnd, &QPushButton::clicked, this, &AuxZonesTab::handleZoneEnd);
    connect(btnZoneScanOn, &QPushButton::clicked, this, &AuxZonesTab::handleZoneScanOn);
    connect(btnZoneScanOff, &QPushButton::clicked, this, &AuxZonesTab::handleZoneScanOff);

    connect(btnPatternRecord, &QPushButton::clicked, this, &AuxZonesTab::handlePatternRecord);
    connect(btnPatternStop, &QPushButton::clicked, this, &AuxZonesTab::handlePatternStop);
    connect(btnPatternRun, &QPushButton::clicked, this, &AuxZonesTab::handlePatternRun);
}

void AuxZonesTab::handleAuxSet()
{
    m_device->setAuxiliary(spinAuxId->value());
}

void AuxZonesTab::handleAuxClear()
{
    m_device->clearAuxiliary(spinAuxId->value());
}

void AuxZonesTab::handleZoneStart()
{
    m_device->setZoneStart(spinZoneId->value());
}

void AuxZonesTab::handleZoneEnd()
{
    m_device->setZoneEnd(spinZoneId->value());
}

void AuxZonesTab::handleZoneScanOn()
{
    m_device->setZoneScan(true);
}

void AuxZonesTab::handleZoneScanOff()
{
    m_device->setZoneScan(false);
}

void AuxZonesTab::handlePatternRecord()
{
    m_device->recordPatternStart(spinPatternId->value());
}

void AuxZonesTab::handlePatternStop()
{
    m_device->recordPatternStop();
}

void AuxZonesTab::handlePatternRun()
{
    m_device->runPattern(spinPatternId->value());
}

} // namespace PelcoDApp
