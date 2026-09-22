/// @file ViscaBusScanDialog.cpp
/// @brief Implementation of VISCA daisy-chain bus scan dialog.

#include "ViscaBusScanDialog.h"

#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

namespace ViscaApp {

ViscaBusScanDialog::ViscaBusScanDialog(std::shared_ptr<::Transport::ITransport> transport, QWidget* parent)
    : QDialog(parent)
    , m_transport(std::move(transport))
    , m_scanner(new QViscaBusScanner(m_transport, this))
{
    setupUi();

    connect(m_scanner, &QViscaBusScanner::deviceDiscovered, this, &ViscaBusScanDialog::onDeviceDiscovered);
    connect(m_scanner, &QViscaBusScanner::scanProgress, this, &ViscaBusScanDialog::onScanProgress);
    connect(m_scanner, &QViscaBusScanner::scanFinished, this, &ViscaBusScanDialog::onScanFinished);
    connect(m_scanner, &QViscaBusScanner::scanFailed, this, [this](const QString& err) {
        lblStatus->setText(tr("Scan failed: %1").arg(err));
        btnStartScan->setEnabled(true);
        progressBar->setValue(0);
    });
}

void ViscaBusScanDialog::setupUi()
{
    setWindowTitle(tr("VISCA Daisy-Chain Bus Auto-Discovery"));
    resize(640, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    lblStatus
        = new QLabel(tr("Click 'Scan Daisy Chain' to broadcast AddressSet (88 30 01 FF) and identify cameras."), this);
    lblStatus->setWordWrap(true);
    mainLayout->addWidget(lblStatus);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    mainLayout->addWidget(progressBar);

    tableResults = new QTableWidget(this);
    tableResults->setColumnCount(5);
    tableResults->setHorizontalHeaderLabels({
        tr("Address"),
        tr("Camera Model"),
        tr("Vendor ID"),
        tr("Model ID"),
        tr("ROM Version"),
    });

    tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableResults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tableResults->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tableResults->verticalHeader()->setVisible(false);
    tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableResults->setSelectionMode(QAbstractItemView::SingleSelection);
    tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableResults->setAlternatingRowColors(true);

    mainLayout->addWidget(tableResults);

    // Button row
    auto* btnLayout = new QHBoxLayout();
    btnStartScan = new QPushButton(tr("Scan Daisy Chain"), this);
    btnStartScan->setObjectName("btnPrimary");

    btnApply = new QPushButton(tr("Apply Selected Camera"), this);
    btnApply->setEnabled(false);

    btnClose = new QPushButton(tr("Close"), this);

    btnLayout->addWidget(btnStartScan);
    btnLayout->addStretch();
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnClose);

    mainLayout->addLayout(btnLayout);

    connect(btnStartScan, &QPushButton::clicked, this, &ViscaBusScanDialog::handleStartScan);
    connect(btnApply, &QPushButton::clicked, this, &ViscaBusScanDialog::handleApply);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(tableResults, &QTableWidget::itemSelectionChanged, this, &ViscaBusScanDialog::handleTableSelectionChanged);
}

void ViscaBusScanDialog::handleStartScan()
{
    tableResults->setRowCount(0);
    progressBar->setValue(0);
    lblStatus->setText(tr("Enumerating daisy-chain bus..."));
    btnStartScan->setEnabled(false);
    btnApply->setEnabled(false);

    m_scanner->setTransport(m_transport);
    m_scanner->startScan();
}

void ViscaBusScanDialog::handleApply()
{
    if (m_selectedAddress > 0) {
        emit cameraSelected(static_cast<uint8_t>(m_selectedAddress), m_selectedModelName);
        accept();
    }
}

void ViscaBusScanDialog::handleTableSelectionChanged()
{
    const auto selected = tableResults->selectedItems();
    if (selected.isEmpty()) {
        btnApply->setEnabled(false);
        m_selectedAddress = -1;
        m_selectedModelName.clear();
        return;
    }

    const int row = selected.first()->row();
    m_selectedAddress = tableResults->item(row, 0)->text().toInt();
    m_selectedModelName = tableResults->item(row, 1)->text();
    btnApply->setEnabled(true);
}

void ViscaBusScanDialog::onDeviceDiscovered(const Visca::DiscoveredCamera& camera)
{
    const int row = tableResults->rowCount();
    tableResults->insertRow(row);

    const auto modelType = Visca::Sony::SonyCameraModel::identify(camera.vendorId, camera.modelId);
    const auto caps = Visca::Sony::SonyCameraModel::getCapabilities(modelType);

    QString modelDisplay;
    if (modelType == Visca::Sony::SonyCameraModelType::FCB_EV9520L) {
        modelDisplay = QStringLiteral("Sony FCB-EV9520L (STARVIS 2 FHD)");
    } else if (modelType == Visca::Sony::SonyCameraModelType::FCB_EW9500H) {
        modelDisplay = QStringLiteral("Sony FCB-EW9500H (4K UHD)");
    } else if (camera.vendorId == Visca::Sony::SonyCameraModel::kSonyVendorId) {
        modelDisplay = QStringLiteral("Sony FCB (Generic / Model 0x%1)").arg(camera.modelId, 4, 16, QLatin1Char('0'));
    } else {
        modelDisplay = QStringLiteral("VISCA Device (Vendor 0x%1)").arg(camera.vendorId, 4, 16, QLatin1Char('0'));
    }

    auto* itemAddr = new QTableWidgetItem(QString::number(camera.address));
    auto* itemModel = new QTableWidgetItem(modelDisplay);
    auto* itemVendor = new QTableWidgetItem(QStringLiteral("0x%1").arg(camera.vendorId, 4, 16, QLatin1Char('0')));
    auto* itemModelId = new QTableWidgetItem(QStringLiteral("0x%1").arg(camera.modelId, 4, 16, QLatin1Char('0')));
    auto* itemRom = new QTableWidgetItem(QStringLiteral("0x%1").arg(camera.romVersion, 4, 16, QLatin1Char('0')));

    tableResults->setItem(row, 0, itemAddr);
    tableResults->setItem(row, 1, itemModel);
    tableResults->setItem(row, 2, itemVendor);
    tableResults->setItem(row, 3, itemModelId);
    tableResults->setItem(row, 4, itemRom);

    if (row == 0) {
        tableResults->selectRow(0);
    }
}

void ViscaBusScanDialog::onScanProgress(int current, int total, int percent)
{
    progressBar->setValue(percent);
    lblStatus->setText(tr("Querying camera %1 of %2...").arg(current).arg(total));
}

void ViscaBusScanDialog::onScanFinished(int totalFound)
{
    progressBar->setValue(100);
    btnStartScan->setEnabled(true);
    lblStatus->setText(tr("Bus scan completed: found %1 camera(s) on daisy-chain.").arg(totalFound));
}

} // namespace ViscaApp
