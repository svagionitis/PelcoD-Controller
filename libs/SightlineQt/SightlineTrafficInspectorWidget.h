#pragma once

/// @file SightlineTrafficInspectorWidget.h
/// @brief Live Sightline protocol traffic monitor and raw hex packet injection widget.

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
#include <TransportStats.h>

class SightlineTrafficInspectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit SightlineTrafficInspectorWidget(QWidget* parent = nullptr);
    ~SightlineTrafficInspectorWidget() override = default;

signals:
    void sendRawHexRequested(const QByteArray& hexData);

public slots:
    void logFrame(bool isTx, const QByteArray& frame);
    void clearLog();
    void updateTransportStats(const Transport::TransportStatsSnapshot& stats);

private slots:
    void handleSendClicked();
    void handleFilterChanged(int index);

private:
    void setupUi();

    QTableWidget* tableInspector { nullptr };
    QCheckBox* chkAutoScroll { nullptr };
    QComboBox* cmbFilter { nullptr };
    QPushButton* btnClear { nullptr };

    QLabel* lblTransportStats { nullptr };
    QLabel* lblKernelStats { nullptr };

    QLineEdit* editRawHex { nullptr };
    QPushButton* btnSendRaw { nullptr };

    int filterMode { 0 }; // 0: All, 1: TX, 2: RX
};
