#pragma once

/// @file TrafficInspectorWidget.h
/// @brief Live Pelco-D protocol packet inspector and raw hex frame injector.

#include <PelcoDStats.h>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <TransportStats.h>

namespace FujinonApp {

/// @class TrafficInspectorWidget
/// @brief Displays real-time serial/socket traffic with decoded Pelco-D commands.
class TrafficInspectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrafficInspectorWidget(QWidget* parent = nullptr);
    ~TrafficInspectorWidget() override = default;

    /// @brief Check whether diagnostics stats bar autohide is enabled.
    /// @return True if stats bar automatically hides during inactivity.
    [[nodiscard]] bool isAutoHideStats() const noexcept;

    /// @brief Enable or disable diagnostics stats bar autohide.
    /// @param[in] enable True to automatically hide stats bar during inactivity.
    void setAutoHideStats(bool enable) noexcept;

signals:
    void sendRawHexRequested(const QByteArray& hexData);
    void openProfilerRequested();

public slots:
    void logFrame(bool isTx, const QByteArray& frame, const QString& description);
    void clearLog();

    /// @brief Updates the real-time transport and protocol telemetry metrics display panel.
    /// @param[in] stats Aggregated communication statistics snapshot.
    /// @param[in] protoStats Pelco-D protocol telemetry snapshot.
    void updateTransportStats(
        const Transport::TransportStatsSnapshot& stats, const PelcoD::PelcoDProtocolStats& protoStats = {});

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleSendClicked();
    void handleFilterChanged(int index);
    void handleOpenMacros();
    void handleAutoHideStatsToggled(bool checked);

private:
    void setupUi();

    QTableWidget* tableInspector { nullptr };
    QCheckBox* chkAutoScroll { nullptr };
    QCheckBox* chkAutoHideStats { nullptr };
    QComboBox* cmbFilter { nullptr };
    QPushButton* btnClear { nullptr };
    QPushButton* btnMacros { nullptr };
    QPushButton* btnProfiler { nullptr };

    QFrame* statsFrame { nullptr };
    QLabel* lblTransportStats { nullptr };
    QLabel* lblKernelStats { nullptr };
    QLabel* lblProtocolStats { nullptr };

    QLineEdit* editRawHex { nullptr };
    QPushButton* btnSendRaw { nullptr };
    QTimer* m_statsHideTimer { nullptr };

    int filterMode { 0 }; // 0 All, 1 TX, 2 RX
};

} // namespace FujinonApp
