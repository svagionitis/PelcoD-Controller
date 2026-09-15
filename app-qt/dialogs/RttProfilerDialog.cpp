/// @file RttProfilerDialog.cpp
/// @brief Implementation of RttProfilerDialog and real-time RttSparklineWidget.

#include "RttProfilerDialog.h"

#include <QDateTime>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace PelcoDApp {

// =============================================================================
// RttSparklineWidget
// =============================================================================

RttSparklineWidget::RttSparklineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet("background-color: #1a1e24; border: 1px solid #2d3748; border-radius: 4px;");
}

void RttSparklineWidget::updateData(
    const std::vector<PelcoD::RttSample>& history, double avgRttMs, double maxRttMs)
{
    m_samples = history;
    m_avgRttMs = avgRttMs;
    m_maxRttMs = maxRttMs;
    update();
}

void RttSparklineWidget::clear()
{
    m_samples.clear();
    m_avgRttMs = 0.0;
    m_maxRttMs = 0.0;
    update();
}

void RttSparklineWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int padding = 16;
    const int chartW = w - (padding * 2);
    const int chartH = h - (padding * 2);

    // Background
    p.fillRect(rect(), QColor("#1a1e24"));

    // Subtle horizontal grid lines
    p.setPen(QPen(QColor("#2d3748"), 1, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        const int gy = padding + (chartH * i) / 4;
        p.drawLine(padding, gy, padding + chartW, gy);
    }

    if (m_samples.empty()) {
        p.setPen(QColor("#718096"));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(rect(), Qt::AlignCenter, "Awaiting Telemetry / Probe Samples...");
        return;
    }

    // Determine scale: max latency + 20% margin, minimum 50ms scale
    double scaleMax = std::max(50.0, m_maxRttMs * 1.25);

    // Compute point coordinates
    const std::size_t count = m_samples.size();
    const double stepX = (count > 1U) ? (static_cast<double>(chartW) / (count - 1U)) : chartW;

    QPolygonF polyline;
    QPainterPath fillPath;
    fillPath.moveTo(padding, padding + chartH);

    for (std::size_t i = 0U; i < count; ++i) {
        const double x = padding + (i * stepX);
        const double sampleVal = m_samples[i].success ? m_samples[i].rttMs : scaleMax;
        const double clampedVal = std::min(sampleVal, scaleMax);
        const double y = (padding + chartH) - (clampedVal / scaleMax * chartH);

        polyline << QPointF(x, y);
        if (i == 0U) {
            fillPath.lineTo(x, y);
        } else {
            fillPath.lineTo(x, y);
        }
    }
    fillPath.lineTo(padding + ((count - 1U) * stepX), padding + chartH);
    fillPath.closeSubpath();

    // Area fill under curve
    QLinearGradient grad(0, padding, 0, padding + chartH);
    grad.setColorAt(0.0, QColor(52, 152, 219, 120));
    grad.setColorAt(1.0, QColor(52, 152, 219, 10));
    p.fillPath(fillPath, grad);

    // Line curve
    p.setPen(QPen(QColor("#3498db"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(polyline);

    // Draw average threshold dashed line if valid
    if (m_avgRttMs > 0.0 && m_avgRttMs < scaleMax) {
        const int avgY = static_cast<int>((padding + chartH) - (m_avgRttMs / scaleMax * chartH));
        p.setPen(QPen(QColor("#f39c12"), 1, Qt::DashLine));
        p.drawLine(padding, avgY, padding + chartW, avgY);

        p.setFont(QFont("Segoe UI", 8));
        p.setPen(QColor("#f39c12"));
        p.drawText(padding + 6, avgY - 3, QString("Avg: %1 ms").arg(m_avgRttMs, 0, 'f', 1));
    }

    // Draw sample dots / timeout indicators
    for (std::size_t i = 0U; i < count; ++i) {
        const double x = padding + (i * stepX);
        if (m_samples[i].success) {
            const double y = (padding + chartH) - (std::min(m_samples[i].rttMs, scaleMax) / scaleMax * chartH);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#2ecc71"));
            p.drawEllipse(QPointF(x, y), 3.0, 3.0);
        } else {
            // Draw red cross/marker for timeout at chart ceiling
            const double y = padding + 4;
            p.setPen(QPen(QColor("#e74c3c"), 2));
            p.drawLine(static_cast<int>(x - 3), static_cast<int>(y - 3), static_cast<int>(x + 3), static_cast<int>(y + 3));
            p.drawLine(static_cast<int>(x + 3), static_cast<int>(y - 3), static_cast<int>(x - 3), static_cast<int>(y + 3));
        }
    }
}

// =============================================================================
// RttProfilerDialog
// =============================================================================

RttProfilerDialog::RttProfilerDialog(std::shared_ptr<PelcoD::PelcoDDevice> device, QWidget* parent)
    : QDialog(parent)
    , m_device(std::move(device))
    , m_profiler(std::make_unique<PelcoDQt::QRttProfiler>(m_device, this))
{
    setupUi();

    connect(m_profiler.get(), &PelcoDQt::QRttProfiler::sampleRecorded, this, &RttProfilerDialog::onSampleRecorded);
    connect(m_profiler.get(), &PelcoDQt::QRttProfiler::statisticsUpdated, this, &RttProfilerDialog::onStatisticsUpdated);
    connect(m_profiler.get(), &PelcoDQt::QRttProfiler::stateChanged, this, &RttProfilerDialog::onStateChanged);
    connect(m_profiler.get(), &PelcoDQt::QRttProfiler::profilingFinished, this, &RttProfilerDialog::onProfilingFinished);
}

RttProfilerDialog::~RttProfilerDialog()
{
    if (m_profiler) {
        m_profiler->stop();
    }
}

void RttProfilerDialog::setupUi()
{
    setWindowTitle("Round-Trip-Time (RTT) & Jitter Profiler");
    resize(760, 620);
    setStyleSheet("QDialog { background-color: #12151a; color: #ecf0f1; font-family: 'Segoe UI', sans-serif; }"
                  "QGroupBox { font-weight: bold; border: 1px solid #2d3748; border-radius: 6px; margin-top: 10px; padding-top: 12px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; color: #63b3ed; }"
                  "QLabel { color: #e2e8f0; }"
                  "QSpinBox, QComboBox { background-color: #1a202c; border: 1px solid #4a5568; border-radius: 4px; padding: 4px; color: #edf2f7; }"
                  "QPushButton { background-color: #2b6cb0; border: none; border-radius: 4px; padding: 6px 14px; color: white; font-weight: bold; }"
                  "QPushButton:hover { background-color: #3182ce; }"
                  "QPushButton:disabled { background-color: #4a5568; color: #a0aec0; }"
                  "QTableWidget { background-color: #1a202c; border: 1px solid #2d3748; border-radius: 4px; gridline-color: #2d3748; color: #e2e8f0; }"
                  "QHeaderView::section { background-color: #2d3748; color: #cbd5e0; font-weight: bold; padding: 4px; border: 1px solid #1a202c; }");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. KPI Cards Row
    auto* kpiGroup = new QGroupBox("Real-Time Telemetry & Statistical Summary", this);
    auto* kpiGrid = new QGridLayout(kpiGroup);
    kpiGrid->setHorizontalSpacing(20);
    kpiGrid->setVerticalSpacing(8);

    auto createCard = [](const QString& title, QLabel*& valLabel) -> QWidget* {
        auto* w = new QWidget();
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(8, 6, 8, 6);
        l->setSpacing(2);
        auto* titleLbl = new QLabel(title, w);
        titleLbl->setStyleSheet("color: #a0aec0; font-size: 11px; text-transform: uppercase;");
        valLabel = new QLabel("--", w);
        valLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #63b3ed;");
        l->addWidget(titleLbl);
        l->addWidget(valLabel);
        w->setStyleSheet("background-color: #1a202c; border: 1px solid #2d3748; border-radius: 6px;");
        return w;
    };

    kpiGrid->addWidget(createCard("Current RTT", lblCurrentRtt), 0, 0);
    kpiGrid->addWidget(createCard("Average RTT", lblAvgRtt), 0, 1);
    kpiGrid->addWidget(createCard("Min / Max Latency", lblMinMaxRtt), 0, 2);
    kpiGrid->addWidget(createCard("Jitter (RFC 3550)", lblJitterRfc), 0, 3);

    kpiGrid->addWidget(createCard("Jitter (StdDev)", lblJitterStdDev), 1, 0);
    kpiGrid->addWidget(createCard("Packet Loss %", lblLossPercent), 1, 1);
    kpiGrid->addWidget(createCard("Probes (OK / Drop / Sent)", lblProbeCounts), 1, 2);
    kpiGrid->addWidget(createCard("Percentiles (P50 / P95)", lblPercentiles), 1, 3);

    mainLayout->addWidget(kpiGroup);

    // 2. Sparkline Chart
    sparklineChart = new RttSparklineWidget(this);
    mainLayout->addWidget(sparklineChart);

    // 3. Recent Samples Table
    tableSamples = new QTableWidget(0, 5, this);
    tableSamples->setHorizontalHeaderLabels({ "Seq", "Timestamp", "Command", "RTT (ms)", "Status" });
    tableSamples->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableSamples->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableSamples->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tableSamples->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tableSamples->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tableSamples->verticalHeader()->setVisible(false);
    tableSamples->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableSamples->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableSamples->setMaximumHeight(160);
    mainLayout->addWidget(tableSamples);

    // 4. Configuration & Controls
    auto* ctrlGroup = new QGroupBox("Profiling Configuration & Controls", this);
    auto* ctrlLayout = new QHBoxLayout(ctrlGroup);

    ctrlLayout->addWidget(new QLabel("Mode:", this));
    cmbMode = new QComboBox(this);
    cmbMode->addItem("Active Burst", static_cast<int>(PelcoD::ProfilerMode::ActiveBurst));
    cmbMode->addItem("Active Continuous", static_cast<int>(PelcoD::ProfilerMode::ActiveContinuous));
    cmbMode->addItem("Passive Monitor", static_cast<int>(PelcoD::ProfilerMode::Passive));
    connect(cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttProfilerDialog::handleModeChanged);
    ctrlLayout->addWidget(cmbMode);

    ctrlLayout->addWidget(new QLabel("Burst Count:", this));
    spinBurstCount = new QSpinBox(this);
    spinBurstCount->setRange(1, 1000);
    spinBurstCount->setValue(20);
    ctrlLayout->addWidget(spinBurstCount);

    ctrlLayout->addWidget(new QLabel("Interval (ms):", this));
    spinIntervalMs = new QSpinBox(this);
    spinIntervalMs->setRange(20, 5000);
    spinIntervalMs->setValue(200);
    ctrlLayout->addWidget(spinIntervalMs);

    ctrlLayout->addWidget(new QLabel("Command:", this));
    cmbQueryTag = new QComboBox(this);
    cmbQueryTag->addItems({ "QueryPan", "QueryTilt", "QueryZoom", "QueryDeviceType", "QueryDiagnostics", "QueryGeneral" });
    ctrlLayout->addWidget(cmbQueryTag);

    ctrlLayout->addStretch();
    mainLayout->addWidget(ctrlGroup);

    // 5. Actions Footer
    auto* actionLayout = new QHBoxLayout();

    btnStartStop = new QPushButton("Start Probing", this);
    btnStartStop->setStyleSheet("background-color: #27ae60; font-size: 13px;");
    connect(btnStartStop, &QPushButton::clicked, this, &RttProfilerDialog::handleStartStopClicked);
    actionLayout->addWidget(btnStartStop);

    btnReset = new QPushButton("Reset Stats", this);
    btnReset->setStyleSheet("background-color: #4a5568;");
    connect(btnReset, &QPushButton::clicked, this, &RttProfilerDialog::handleResetClicked);
    actionLayout->addWidget(btnReset);

    actionLayout->addStretch();

    btnExportCsv = new QPushButton("Export CSV...", this);
    btnExportCsv->setStyleSheet("background-color: #2d3748;");
    connect(btnExportCsv, &QPushButton::clicked, this, &RttProfilerDialog::handleExportCsvClicked);
    actionLayout->addWidget(btnExportCsv);

    btnExportJson = new QPushButton("Export JSON...", this);
    btnExportJson->setStyleSheet("background-color: #2d3748;");
    connect(btnExportJson, &QPushButton::clicked, this, &RttProfilerDialog::handleExportJsonClicked);
    actionLayout->addWidget(btnExportJson);

    btnClose = new QPushButton("Close", this);
    btnClose->setStyleSheet("background-color: #4a5568;");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    actionLayout->addWidget(btnClose);

    mainLayout->addLayout(actionLayout);
}

void RttProfilerDialog::handleModeChanged(int index)
{
    const auto mode = static_cast<PelcoD::ProfilerMode>(cmbMode->itemData(index).toInt());
    spinBurstCount->setEnabled(mode == PelcoD::ProfilerMode::ActiveBurst);
    spinIntervalMs->setEnabled(mode != PelcoD::ProfilerMode::Passive);
    cmbQueryTag->setEnabled(mode != PelcoD::ProfilerMode::Passive);
}

void RttProfilerDialog::handleStartStopClicked()
{
    if (m_profiler->isRunning()) {
        m_profiler->stop();
    } else {
        tableSamples->setRowCount(0);
        sparklineChart->clear();

        const auto mode = static_cast<PelcoD::ProfilerMode>(cmbMode->currentData().toInt());
        bool ok = false;
        if (mode == PelcoD::ProfilerMode::ActiveBurst) {
            ok = m_profiler->startBurst(spinBurstCount->value(), spinIntervalMs->value(), cmbQueryTag->currentText());
        } else if (mode == PelcoD::ProfilerMode::ActiveContinuous) {
            ok = m_profiler->startContinuous(spinIntervalMs->value(), cmbQueryTag->currentText());
        } else {
            ok = m_profiler->startPassive();
        }

        if (!ok) {
            QMessageBox::warning(this, "Profiler Error",
                "Failed to start profiler. Ensure device is connected and valid parameters are selected.");
        }
    }
}

void RttProfilerDialog::handleResetClicked()
{
    m_profiler->reset();
    tableSamples->setRowCount(0);
    sparklineChart->clear();
    lblCurrentRtt->setText("--");
    lblAvgRtt->setText("--");
    lblMinMaxRtt->setText("--");
    lblJitterRfc->setText("--");
    lblJitterStdDev->setText("--");
    lblLossPercent->setText("--");
    lblProbeCounts->setText("--");
    lblPercentiles->setText("--");
}

void RttProfilerDialog::handleExportCsvClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Latency Telemetry to CSV", "", "CSV Files (*.csv)");
    if (!path.isEmpty()) {
        if (!m_profiler->exportToCsv(path)) {
            QMessageBox::critical(this, "Export Error", "Failed to write CSV file.");
        } else {
            QMessageBox::information(this, "Export Success", "Telemetry data successfully exported to CSV.");
        }
    }
}

void RttProfilerDialog::handleExportJsonClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Latency Telemetry to JSON", "", "JSON Files (*.json)");
    if (!path.isEmpty()) {
        if (!m_profiler->exportToJson(path)) {
            QMessageBox::critical(this, "Export Error", "Failed to write JSON file.");
        } else {
            QMessageBox::information(this, "Export Success", "Telemetry data successfully exported to JSON.");
        }
    }
}

void RttProfilerDialog::onStateChanged(bool isRunning)
{
    if (isRunning) {
        btnStartStop->setText("Stop Probing");
        btnStartStop->setStyleSheet("background-color: #c0392b; font-size: 13px;");
        cmbMode->setEnabled(false);
        spinBurstCount->setEnabled(false);
        spinIntervalMs->setEnabled(false);
        cmbQueryTag->setEnabled(false);
    } else {
        btnStartStop->setText("Start Probing");
        btnStartStop->setStyleSheet("background-color: #27ae60; font-size: 13px;");
        cmbMode->setEnabled(true);
        handleModeChanged(cmbMode->currentIndex());
    }
}

void RttProfilerDialog::onProfilingFinished(const PelcoD::RttStatistics& stats)
{
    updateKpiCards(stats);
}

void RttProfilerDialog::onSampleRecorded(
    const PelcoD::RttSample& sample, const PelcoD::RttStatistics& stats)
{
    const int row = tableSamples->rowCount();
    tableSamples->insertRow(row);

    tableSamples->setItem(row, 0, new QTableWidgetItem(QString::number(sample.sequenceNumber)));
    tableSamples->setItem(row, 1, new QTableWidgetItem(QDateTime::currentDateTime().toString("hh:mm:ss.zzz")));
    tableSamples->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(sample.queryTag)));

    auto* rttItem = new QTableWidgetItem(sample.success ? QString::number(sample.rttMs, 'f', 2) : "TIMEOUT");
    if (!sample.success) {
        rttItem->setForeground(QColor("#e74c3c"));
    }
    tableSamples->setItem(row, 3, rttItem);

    auto* statusItem = new QTableWidgetItem(sample.success ? "OK" : "TIMEOUT");
    statusItem->setForeground(sample.success ? QColor("#2ecc71") : QColor("#e74c3c"));
    tableSamples->setItem(row, 4, statusItem);

    tableSamples->scrollToBottom();

    sparklineChart->updateData(m_profiler->history(), stats.avgRttMs, stats.maxRttMs);
}

void RttProfilerDialog::onStatisticsUpdated(const PelcoD::RttStatistics& stats)
{
    updateKpiCards(stats);
}

void RttProfilerDialog::updateKpiCards(const PelcoD::RttStatistics& stats)
{
    // Color-code Current RTT based on latency
    QString curColor = "#2ecc71";
    if (stats.currentRttMs > 150.0) {
        curColor = "#e74c3c";
    } else if (stats.currentRttMs > 50.0) {
        curColor = "#f39c12";
    }

    lblCurrentRtt->setText(QString("%1 ms").arg(stats.currentRttMs, 0, 'f', 1));
    lblCurrentRtt->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(curColor));

    lblAvgRtt->setText(QString("%1 ms").arg(stats.avgRttMs, 0, 'f', 1));
    lblMinMaxRtt->setText(QString("%1 / %2 ms").arg(stats.minRttMs, 0, 'f', 1).arg(stats.maxRttMs, 0, 'f', 1));
    lblJitterRfc->setText(QString("%1 ms").arg(stats.jitterRfc3550Ms, 0, 'f', 2));
    lblJitterStdDev->setText(QString("%1 ms").arg(stats.stdDevMs, 0, 'f', 2));

    lblLossPercent->setText(QString("%1 %").arg(stats.lossPercent, 0, 'f', 1));
    if (stats.lossPercent > 0.0) {
        lblLossPercent->setStyleSheet("font-size: 16px; font-weight: bold; color: #e74c3c;");
    } else {
        lblLossPercent->setStyleSheet("font-size: 16px; font-weight: bold; color: #2ecc71;");
    }

    lblProbeCounts->setText(QString("%1 / %2 / %3")
                               .arg(stats.successfulProbes)
                               .arg(stats.timedOutProbes)
                               .arg(stats.totalProbes));

    lblPercentiles->setText(QString("P50: %1 | P95: %2 ms")
                               .arg(stats.p50RttMs, 0, 'f', 1)
                               .arg(stats.p95RttMs, 0, 'f', 1));
}

} // namespace PelcoDApp
