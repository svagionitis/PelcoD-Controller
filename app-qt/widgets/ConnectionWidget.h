#pragma once

/// @file ConnectionWidget.h
/// @brief UI widget for configuring and initiating Pelco-D device transport connections.

#include "ITransport.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QWidget>
#include <memory>

namespace PelcoDApp {

/// @class ConnectionWidget
/// @brief Toolbar widget managing transport selection and connection state.
class ConnectionWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionWidget(QWidget* parent = nullptr);
    ~ConnectionWidget() override = default;

signals:
    void connectRequested(std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address);
    void disconnectRequested();
    void refreshPortsRequested();

public slots:
    void setConnectionState(bool connected);
    void setConnecting(bool connecting);
    void refreshSerialPorts();

private slots:
    void handleConnectClicked();
    void handleModeChanged(int index);

private:
    void setupUi();
    void updateLedState(bool connected);

    QComboBox* cmbMode { nullptr };
    QStackedWidget* stackedConfig { nullptr };

    // Serial widgets
    QWidget* pageSerial { nullptr };
    QComboBox* cmbSerialPort { nullptr };
    QComboBox* cmbBaudRate { nullptr };
    QPushButton* btnRefreshPorts { nullptr };

    // TCP widgets
    QWidget* pageTcp { nullptr };
    QLineEdit* editTcpHost { nullptr };
    QSpinBox* spinTcpPort { nullptr };

    // Mock widgets
    QWidget* pageMock { nullptr };

    // Common
    QSpinBox* spinAddress { nullptr };
    QPushButton* btnConnect { nullptr };
    QLabel* lblLed { nullptr };
    QLabel* lblStatusText { nullptr };

    bool isConnected { false };
};

} // namespace PelcoDApp
