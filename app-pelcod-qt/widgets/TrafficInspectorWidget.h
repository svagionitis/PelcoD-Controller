#pragma once

/// @file TrafficInspectorWidget.h
/// @brief Live Pelco-D protocol packet inspector and raw hex frame injector.

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <TransportStats.h>

namespace PelcoDApp {

/// @class TrafficInspectorWidget
/// @brief Displays real-time serial/socket traffic with decoded Pelco-D commands.
class TrafficInspectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrafficInspectorWidget(QWidget* parent = nullptr);
    ~TrafficInspectorWidget() override = default;

signals:
    void sendRawHexRequested(const QByteArray& hexData);

public slots:
    void logFrame(bool isTx, const QByteArray& frame, const QString& description);
    void clearLog();

    /// @brief Updates the real-time transport and kernel-level metrics display panel.
    /// @param[in] stats Aggregated communication statistics snapshot.
    void updateTransportStats(const Transport::TransportStatsSnapshot& stats);

private slots:
    void handleSendClicked();
    void handleFilterChanged(int index);
    void handleOpenMacros();

private:
    void setupUi();

    QTableWidget* tableInspector { nullptr };
    QCheckBox* chkAutoScroll { nullptr };
    QComboBox* cmbFilter { nullptr };
    QPushButton* btnClear { nullptr };
    QPushButton* btnMacros { nullptr };

    QLabel* lblTransportStats { nullptr };
    QLabel* lblKernelStats { nullptr };

    QLineEdit* editRawHex { nullptr };
    QPushButton* btnSendRaw { nullptr };

    int filterMode { 0 }; // 0 All, 1 TX, 2 RX
};

} // namespace PelcoDApp
