#pragma once

/// @file BusScanDialog.h
/// @brief Interactive dialog for RS-485 bus address auto-discovery.

#include "ITransport.h"
#include "QBusScanner.h"

#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <memory>

namespace PelcoDApp {

/// @class BusScanDialog
/// @brief Modal dialog probing an RS-485 / network transport for active Pelco-D addresses.
class BusScanDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Construct a BusScanDialog for the specified transport.
    /// @param[in] transport Underlying transport used to communicate with the bus.
    /// @param[in] parent Optional parent QWidget.
    explicit BusScanDialog(std::shared_ptr<PelcoD::ITransport> transport, QWidget* parent = nullptr);

    /// @brief Destructor stopping any active background scan.
    ~BusScanDialog() override;

    /// @brief Retrieve the selected device address chosen by user.
    /// @return Device address (1–254) or -1 if none chosen.
    [[nodiscard]] int selectedAddress() const noexcept
    {
        return m_selectedAddress;
    }

signals:
    /// @brief Emitted when user selects and confirms an address.
    /// @param address Chosen Pelco-D address (1–254).
    void addressSelected(int address);

private slots:
    void handleStartScan();
    void handleStopScan();
    void handleApplyAddress();
    void handleTableSelectionChanged();

    void onDeviceDiscovered(int address, int responseTimeMs, bool hasPan, int panCentidegrees);
    void onProgressUpdated(int currentAddress, int scannedCount, int totalCount, int percent);
    void onStateChanged(PelcoD::ScanState state);
    void onScanFinished(int totalFound);

private:
    void setupUi();

    std::shared_ptr<PelcoD::ITransport> m_transport;
    PelcoDQt::QBusScanner* m_scanner { nullptr };

    QSpinBox* spinStartAddr { nullptr };
    QSpinBox* spinEndAddr { nullptr };
    QSpinBox* spinTimeoutMs { nullptr };

    QPushButton* btnStartScan { nullptr };
    QPushButton* btnStopScan { nullptr };
    QPushButton* btnApply { nullptr };
    QPushButton* btnClose { nullptr };

    QProgressBar* progressBar { nullptr };
    QLabel* lblStatus { nullptr };
    QTableWidget* tableResults { nullptr };

    int m_selectedAddress { -1 };
};

} // namespace PelcoDApp
