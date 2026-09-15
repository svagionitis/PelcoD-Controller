#pragma once

/// @file RttProfilerDialog.h
/// @brief Dialog visualizing real-time RTT latency, jitter distribution, and packet loss.

#include "QRttProfiler.h"

#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <vector>

namespace PelcoDApp {

/// @class RttSparklineWidget
/// @brief Custom QWidget drawing a real-time latency curve with average threshold and timeout markers.
class RttSparklineWidget : public QWidget {
    Q_OBJECT

public:
    explicit RttSparklineWidget(QWidget* parent = nullptr);
    ~RttSparklineWidget() override = default;

    void updateData(const std::vector<PelcoD::RttSample>& history, double avgRttMs, double maxRttMs);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<PelcoD::RttSample> m_samples;
    double m_avgRttMs { 0.0 };
    double m_maxRttMs { 0.0 };
};

/// @class RttProfilerDialog
/// @brief Interactive diagnostic dialog providing real-time RTT profiling, jitter metrics, and CSV/JSON export.
class RttProfilerDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Construct an RttProfilerDialog for the specified device controller.
    /// @param[in] device Pointer to device controller.
    /// @param[in] parent Optional parent widget.
    explicit RttProfilerDialog(std::shared_ptr<PelcoD::PelcoDDevice> device, QWidget* parent = nullptr);

    /// @brief Destructor stopping background profiling.
    ~RttProfilerDialog() override;

private slots:
    void handleStartStopClicked();
    void handleResetClicked();
    void handleExportCsvClicked();
    void handleExportJsonClicked();
    void handleModeChanged(int index);

    void onSampleRecorded(const PelcoD::RttSample& sample, const PelcoD::RttStatistics& stats);
    void onStatisticsUpdated(const PelcoD::RttStatistics& stats);
    void onStateChanged(bool isRunning);
    void onProfilingFinished(const PelcoD::RttStatistics& stats);

private:
    void setupUi();
    void updateKpiCards(const PelcoD::RttStatistics& stats);

    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    std::unique_ptr<PelcoDQt::QRttProfiler> m_profiler;

    // KPI Card Labels
    QLabel* lblCurrentRtt { nullptr };
    QLabel* lblAvgRtt { nullptr };
    QLabel* lblMinMaxRtt { nullptr };
    QLabel* lblJitterRfc { nullptr };
    QLabel* lblJitterStdDev { nullptr };
    QLabel* lblLossPercent { nullptr };
    QLabel* lblProbeCounts { nullptr };
    QLabel* lblPercentiles { nullptr };

    // Chart & Table
    RttSparklineWidget* sparklineChart { nullptr };
    QTableWidget* tableSamples { nullptr };

    // Controls
    QComboBox* cmbMode { nullptr };
    QSpinBox* spinBurstCount { nullptr };
    QSpinBox* spinIntervalMs { nullptr };
    QComboBox* cmbQueryTag { nullptr };

    QPushButton* btnStartStop { nullptr };
    QPushButton* btnReset { nullptr };
    QPushButton* btnExportCsv { nullptr };
    QPushButton* btnExportJson { nullptr };
    QPushButton* btnClose { nullptr };
};

} // namespace PelcoDApp
