#pragma once

/// @file ViscaTrafficInspectorWidget.h
/// @brief Live VISCA protocol traffic monitor and raw hex packet injection widget.

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace ViscaApp {

/// @class ViscaTrafficInspectorWidget
/// @brief Displays real-time serial/socket traffic with decoded VISCA packet descriptions.
class ViscaTrafficInspectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViscaTrafficInspectorWidget(QWidget* parent = nullptr);
    ~ViscaTrafficInspectorWidget() override = default;

signals:
    void sendRawHexRequested(const QByteArray& hexData);

public slots:
    void logFrame(bool isTx, const QByteArray& frame, const QString& description);
    void clearLog();

private slots:
    void handleSendClicked();
    void handleFilterChanged(int index);

private:
    void setupUi();

    QTableWidget* tableInspector { nullptr };
    QCheckBox* chkAutoScroll { nullptr };
    QComboBox* cmbFilter { nullptr };
    QPushButton* btnClear { nullptr };

    QLineEdit* editRawHex { nullptr };
    QPushButton* btnSendRaw { nullptr };

    int filterMode { 0 }; // 0 All, 1 TX, 2 RX
};

} // namespace ViscaApp
