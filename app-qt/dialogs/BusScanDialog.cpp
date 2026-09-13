/// @file BusScanDialog.cpp
/// @brief Implementation of interactive RS-485 bus address auto-discovery dialog.

#include "BusScanDialog.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace PelcoDApp {

BusScanDialog::BusScanDialog(std::shared_ptr<PelcoD::ITransport> transport, QWidget* parent)
    : QDialog(parent)
    , m_transport(std::move(transport))
    , m_scanner(new PelcoDQt::QBusScanner(m_transport, this))
{
    setupUi();

    connect(m_scanner, &PelcoDQt::QBusScanner::deviceDiscovered, this, &BusScanDialog::onDeviceDiscovered);
    connect(m_scanner, &PelcoDQt::QBusScanner::progressUpdated, this, &BusScanDialog::onProgressUpdated);
    connect(m_scanner, &PelcoDQt::QBusScanner::stateChanged, this, &BusScanDialog::onStateChanged);
    connect(m_scanner, &PelcoDQt::QBusScanner::scanFinished, this, &BusScanDialog::onScanFinished);
}

BusScanDialog::~BusScanDialog()
{
    if (m_scanner) {
        m_scanner->stopScan();
    }
}

void BusScanDialog::setupUi()
{
    setWindowTitle(tr("RS-485 Bus Address Scanner"));
    resize(520, 440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Range Configuration
    auto* grpConfig = new QGroupBox(tr("Scan Parameters"), this);
    auto* cfgLayout = new QHBoxLayout(grpConfig);
    cfgLayout->setSpacing(10);

    cfgLayout->addWidget(new QLabel(tr("Start ID:")));
    spinStartAddr = new QSpinBox(grpConfig);
    spinStartAddr->setRange(1, 254);
    spinStartAddr->setValue(1);
    cfgLayout->addWidget(spinStartAddr);

    cfgLayout->addWidget(new QLabel(tr("End ID:")));
    spinEndAddr = new QSpinBox(grpConfig);
    spinEndAddr->setRange(1, 254);
    spinEndAddr->setValue(32);
    cfgLayout->addWidget(spinEndAddr);

    cfgLayout->addWidget(new QLabel(tr("Timeout:")));
    spinTimeoutMs = new QSpinBox(grpConfig);
    spinTimeoutMs->setRange(20, 2000);
    spinTimeoutMs->setValue(150);
    spinTimeoutMs->setSuffix(tr(" ms"));
    cfgLayout->addWidget(spinTimeoutMs);

    mainLayout->addWidget(grpConfig);

    // Controls & Progress
    auto* actionLayout = new QHBoxLayout();
    btnStartScan = new QPushButton(tr("▶ Start Scan"), this);
    btnStartScan->setObjectName("btnSuccess");
    btnStopScan = new QPushButton(tr("⏹ Stop"), this);
    btnStopScan->setObjectName("btnDanger");
    btnStopScan->setEnabled(false);

    actionLayout->addWidget(btnStartScan);
    actionLayout->addWidget(btnStopScan);
    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    // Progress Bar & Status
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setFormat(tr("%p%"));
    mainLayout->addWidget(progressBar);

    lblStatus = new QLabel(tr("Ready to scan bus."), this);
    lblStatus->setStyleSheet("color: #a0a0a0; font-style: italic;");
    mainLayout->addWidget(lblStatus);

    // Discovered Devices Table
    tableResults = new QTableWidget(0, 3, this);
    tableResults->setHorizontalHeaderLabels({ tr("Address ID"), tr("Response Time"), tr("Status / Telemetry") });
    tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableResults->setSelectionMode(QAbstractItemView::SingleSelection);
    tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(tableResults);

    // Bottom Action Buttons
    auto* bottomLayout = new QHBoxLayout();
    btnApply = new QPushButton(tr("Use Selected Address"), this);
    btnApply->setObjectName("btnPrimary");
    btnApply->setEnabled(false);

    btnClose = new QPushButton(tr("Close"), this);

    bottomLayout->addWidget(btnApply);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomLayout);

    // Connections
    connect(btnStartScan, &QPushButton::clicked, this, &BusScanDialog::handleStartScan);
    connect(btnStopScan, &QPushButton::clicked, this, &BusScanDialog::handleStopScan);
    connect(btnApply, &QPushButton::clicked, this, &BusScanDialog::handleApplyAddress);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(tableResults, &QTableWidget::itemSelectionChanged, this, &BusScanDialog::handleTableSelectionChanged);
    connect(tableResults, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        tableResults->selectRow(row);
        handleApplyAddress();
    });
}

void BusScanDialog::handleStartScan()
{
    tableResults->setRowCount(0);
    btnApply->setEnabled(false);
    m_selectedAddress = -1;

    const int startAddr = spinStartAddr->value();
    const int endAddr = spinEndAddr->value();
    const int timeout = spinTimeoutMs->value();

    m_scanner->startScan(startAddr, endAddr, timeout);
}

void BusScanDialog::handleStopScan()
{
    m_scanner->stopScan();
}

void BusScanDialog::handleApplyAddress()
{
    const int row = tableResults->currentRow();
    if (row >= 0 && row < tableResults->rowCount()) {
        auto* item = tableResults->item(row, 0);
        if (item) {
            m_selectedAddress = item->text().toInt();
            emit addressSelected(m_selectedAddress);
            accept();
        }
    }
}

void BusScanDialog::handleTableSelectionChanged()
{
    btnApply->setEnabled(tableResults->currentRow() >= 0);
}

void BusScanDialog::onDeviceDiscovered(int address, int responseTimeMs, bool hasPan, int panCentidegrees)
{
    const int row = tableResults->rowCount();
    tableResults->insertRow(row);

    tableResults->setItem(row, 0, new QTableWidgetItem(QString::number(address)));
    tableResults->setItem(row, 1, new QTableWidgetItem(tr("%1 ms").arg(responseTimeMs)));

    QString info;
    if (hasPan) {
        info = tr("Online (Pan: %1°)").arg(QString::number(panCentidegrees / 100.0, 'f', 2));
    } else {
        info = tr("Online (Responded)");
    }
    tableResults->setItem(row, 2, new QTableWidgetItem(info));
    tableResults->selectRow(row);
}

void BusScanDialog::onProgressUpdated(int currentAddress, int scannedCount, int totalCount, int percent)
{
    progressBar->setValue(percent);
    lblStatus->setText(
        tr("Scanning address %1... (%2 of %3 completed | Found %4 device(s))")
            .arg(currentAddress)
            .arg(scannedCount)
            .arg(totalCount)
            .arg(tableResults->rowCount()));
}

void BusScanDialog::onStateChanged(PelcoD::ScanState state)
{
    switch (state) {
    case PelcoD::ScanState::Scanning:
        btnStartScan->setEnabled(false);
        btnStopScan->setEnabled(true);
        spinStartAddr->setEnabled(false);
        spinEndAddr->setEnabled(false);
        spinTimeoutMs->setEnabled(false);
        break;
    case PelcoD::ScanState::Paused:
        btnStartScan->setEnabled(true);
        btnStopScan->setEnabled(true);
        break;
    case PelcoD::ScanState::Idle:
        btnStartScan->setEnabled(true);
        btnStopScan->setEnabled(false);
        spinStartAddr->setEnabled(true);
        spinEndAddr->setEnabled(true);
        spinTimeoutMs->setEnabled(true);
        break;
    }
}

void BusScanDialog::onScanFinished(int totalFound)
{
    progressBar->setValue(100);
    lblStatus->setText(tr("Scan complete. %1 device(s) found on bus.").arg(totalFound));
}

} // namespace PelcoDApp
