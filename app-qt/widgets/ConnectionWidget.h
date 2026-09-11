#pragma once

/// @file ConnectionWidget.h
/// @brief UI widget for configuring and initiating Pelco-D device transport connections.

#include "ITransport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>
#include <memory>

namespace PelcoDApp {

/// @class ConnectionWidget
/// @brief Toolbar widget managing transport selection, connection state, and auto-reconnect strategy.
class ConnectionWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionWidget(QWidget* parent = nullptr);
    ~ConnectionWidget() override = default;

    /// @brief Check if auto-reconnect option is checked.
    [[nodiscard]] bool isAutoReconnectEnabled() const noexcept;

    /// @brief Programmatically enable or disable auto-reconnect.
    void setAutoReconnectEnabled(bool enabled);

    /// @brief Get current reconnect attempt count.
    [[nodiscard]] int reconnectAttemptCount() const noexcept
    {
        return m_reconnectAttempt;
    }

    /// @brief Check if an auto-reconnect attempt timer is actively pending.
    [[nodiscard]] bool isReconnecting() const noexcept;

    /// @brief Cancel any pending auto-reconnect timer and reset backoff counter.
    void stopAutoReconnect();

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
    void onReconnectTimerTimeout();

private:
    void setupUi();
    void updateLedState(bool connected);
    void scheduleReconnect();
    void triggerConnect();

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
    QCheckBox* chkAutoReconnect { nullptr };
    QPushButton* btnConnect { nullptr };
    QLabel* lblLed { nullptr };
    QLabel* lblStatusText { nullptr };

    QTimer* m_reconnectTimer { nullptr };
    int m_reconnectAttempt { 0 };
    bool m_manualDisconnect { false };
    bool isConnected { false };

    static constexpr int kBackoffScheduleMs[] = { 1000, 2000, 5000, 10000 };
    static constexpr int kMaxBackoffSteps = 4;
};

} // namespace PelcoDApp
