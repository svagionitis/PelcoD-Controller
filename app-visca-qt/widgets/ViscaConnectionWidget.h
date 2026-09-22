#pragma once

/// @file ViscaConnectionWidget.h
/// @brief Toolbar widget managing transport selection, address, and connection state for VISCA cameras.

#include <SonyCameraModel.h>
#include <Transport/ITransport.h>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QWidget>

#include <memory>

namespace ViscaApp {

/// @class ViscaConnectionWidget
/// @brief UI widget providing Serial, TCP, UDP, and Mock camera transport configuration.
class ViscaConnectionWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViscaConnectionWidget(QWidget* parent = nullptr);
    ~ViscaConnectionWidget() override = default;

    [[nodiscard]] std::shared_ptr<::Transport::ITransport> createConfiguredTransport() const;
    [[nodiscard]] uint8_t selectedAddress() const noexcept;
    void setSelectedAddress(uint8_t address);

    [[nodiscard]] static QStringList enumerateSerialPorts();

signals:
    void connectRequested(std::shared_ptr<::Transport::ITransport> transport, uint8_t address);
    void disconnectRequested();

public slots:
    void setConnectionState(bool connected);
    void setConnecting(bool connecting);
    void setModelBadge(const QString& modelName);
    void refreshSerialPorts();

private slots:
    void handleConnectClicked();
    void handleModeChanged(int index);
    void handleScanBus();

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

    // UDP widgets
    QWidget* pageUdp { nullptr };
    QLineEdit* editUdpHost { nullptr };
    QSpinBox* spinUdpPort { nullptr };

    // Mock widgets
    QWidget* pageMock { nullptr };
    QComboBox* cmbMockModel { nullptr };

    // Common
    QSpinBox* spinAddress { nullptr };
    QPushButton* btnScanBus { nullptr };
    QPushButton* btnConnect { nullptr };
    QLabel* lblLed { nullptr };
    QLabel* lblStatusText { nullptr };

    bool m_connected { false };
};

} // namespace ViscaApp
