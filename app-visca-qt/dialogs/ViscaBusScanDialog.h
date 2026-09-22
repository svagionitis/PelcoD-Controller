#pragma once

/// @file ViscaBusScanDialog.h
/// @brief Modal dialog for discovering and selecting Sony / VISCA cameras on daisy chain.

#include "../QViscaBusScanner.h"
#include <SonyCameraModel.h>
#include <Transport/ITransport.h>

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>

#include <memory>

namespace ViscaApp {

/// @class ViscaBusScanDialog
/// @brief Interactive dialog initiating AddressSet and version queries across the bus.
class ViscaBusScanDialog : public QDialog {
    Q_OBJECT

public:
    explicit ViscaBusScanDialog(std::shared_ptr<::Transport::ITransport> transport, QWidget* parent = nullptr);
    ~ViscaBusScanDialog() override = default;

    [[nodiscard]] int selectedAddress() const noexcept
    {
        return m_selectedAddress;
    }

signals:
    void cameraSelected(uint8_t address, const QString& modelName);

private slots:
    void handleStartScan();
    void handleApply();
    void handleTableSelectionChanged();

    void onDeviceDiscovered(const Visca::DiscoveredCamera& camera);
    void onScanProgress(int current, int total, int percent);
    void onScanFinished(int totalFound);

private:
    void setupUi();

    std::shared_ptr<::Transport::ITransport> m_transport;
    QViscaBusScanner* m_scanner { nullptr };

    QPushButton* btnStartScan { nullptr };
    QPushButton* btnApply { nullptr };
    QPushButton* btnClose { nullptr };

    QProgressBar* progressBar { nullptr };
    QLabel* lblStatus { nullptr };
    QTableWidget* tableResults { nullptr };

    int m_selectedAddress { -1 };
    QString m_selectedModelName {};
};

} // namespace ViscaApp
