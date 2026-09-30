/// @file ViscaTrafficInspectorWidget.cpp
/// @brief Implementation of live VISCA protocol traffic monitor.

#include "ViscaTrafficInspectorWidget.h"

#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>

namespace ViscaApp {

ViscaTrafficInspectorWidget::ViscaTrafficInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ViscaTrafficInspectorWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // Toolbar
    auto* toolLayout = new QHBoxLayout();
    toolLayout->setSpacing(8);

    chkAutoScroll = new QCheckBox(tr("Auto-Scroll"), this);
    chkAutoScroll->setChecked(true);

    cmbFilter = new QComboBox(this);
    cmbFilter->addItem(tr("All Traffic"), 0);
    cmbFilter->addItem(tr("TX Only (Host -> Camera)"), 1);
    cmbFilter->addItem(tr("RX Only (Camera -> Host)"), 2);

    btnClear = new QPushButton(tr("Clear Log"), this);

    toolLayout->addWidget(chkAutoScroll);
    toolLayout->addWidget(new QLabel(tr("Filter:"), this));
    toolLayout->addWidget(cmbFilter);
    toolLayout->addStretch();
    toolLayout->addWidget(btnClear);

    mainLayout->addLayout(toolLayout);

    // Inspector Table
    tableInspector = new QTableWidget(this);
    tableInspector->setColumnCount(4);
    tableInspector->setHorizontalHeaderLabels({
        tr("Timestamp"),
        tr("Direction"),
        tr("Hex Payload"),
        tr("Decoded Packet"),
    });

    tableInspector->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    tableInspector->verticalHeader()->setVisible(false);
    tableInspector->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableInspector->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableInspector->setAlternatingRowColors(true);

    mainLayout->addWidget(tableInspector);

    // Transport and kernel diagnostics bar
    auto* statsFrame = new QFrame(this);
    statsFrame->setFrameShape(QFrame::StyledPanel);
    statsFrame->setStyleSheet(
        "QFrame { background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 2px; }");
    auto* statsLayout = new QVBoxLayout(statsFrame);
    statsLayout->setContentsMargins(6, 4, 6, 4);
    statsLayout->setSpacing(2);

    lblTransportStats = new QLabel(tr("VISCA Transport: Idle | TX: 0 B (0 pkts) | RX: 0 B (0 pkts) | Errors: 0"), this);
    lblTransportStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #58a6ff;");
    statsLayout->addWidget(lblTransportStats);

    lblKernelStats = new QLabel(tr("Kernel Telemetry: Awaiting active transport channel..."), this);
    lblKernelStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #8b949e;");
    statsLayout->addWidget(lblKernelStats);

    mainLayout->addWidget(statsFrame);

    // Raw Hex Injection Row
    auto* sendLayout = new QHBoxLayout();
    sendLayout->setSpacing(8);

    auto* lblInject = new QLabel(tr("Inject Hex:"), this);
    editRawHex = new QLineEdit(this);
    editRawHex->setPlaceholderText(tr("e.g. 81 01 04 07 02 FF (Zoom Tele)"));

    btnSendRaw = new QPushButton(tr("Send"), this);
    btnSendRaw->setObjectName("btnPrimary");

    sendLayout->addWidget(lblInject);
    sendLayout->addWidget(editRawHex);
    sendLayout->addWidget(btnSendRaw);

    mainLayout->addLayout(sendLayout);

    // Wire events
    connect(btnClear, &QPushButton::clicked, this, &ViscaTrafficInspectorWidget::clearLog);
    connect(btnSendRaw, &QPushButton::clicked, this, &ViscaTrafficInspectorWidget::handleSendClicked);
    connect(editRawHex, &QLineEdit::returnPressed, this, &ViscaTrafficInspectorWidget::handleSendClicked);
    connect(cmbFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ViscaTrafficInspectorWidget::handleFilterChanged);
}

void ViscaTrafficInspectorWidget::logFrame(bool isTx, const QByteArray& frame, const QString& description)
{
    if ((filterMode == 1 && !isTx) || (filterMode == 2 && isTx)) {
        return;
    }

    const int row = tableInspector->rowCount();
    tableInspector->insertRow(row);

    const QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    const QString dirStr = isTx ? QStringLiteral("TX") : QStringLiteral("RX");
    const QString hexStr = frame.toHex(' ').toUpper();

    auto* itemTime = new QTableWidgetItem(timeStr);
    auto* itemDir = new QTableWidgetItem(dirStr);
    auto* itemHex = new QTableWidgetItem(hexStr);
    auto* itemDesc = new QTableWidgetItem(description);

    const QColor txColor("#58a6ff"); // soft blue
    const QColor rxColor("#3fb950"); // soft green
    const QColor dirColor = isTx ? txColor : rxColor;

    itemDir->setForeground(dirColor);
    itemHex->setFont(QFont("Monospace", 9));
    itemHex->setForeground(QColor("#e6edf3"));

    tableInspector->setItem(row, 0, itemTime);
    tableInspector->setItem(row, 1, itemDir);
    tableInspector->setItem(row, 2, itemHex);
    tableInspector->setItem(row, 3, itemDesc);

    // Cap at 1000 rows
    if (tableInspector->rowCount() > 1000) {
        tableInspector->removeRow(0);
    }

    if (chkAutoScroll->isChecked()) {
        tableInspector->scrollToBottom();
    }
}

void ViscaTrafficInspectorWidget::clearLog()
{
    tableInspector->setRowCount(0);
}

void ViscaTrafficInspectorWidget::handleFilterChanged(int index)
{
    filterMode = index;
}

void ViscaTrafficInspectorWidget::handleSendClicked()
{
    const QString text = editRawHex->text().trimmed();
    if (!text.isEmpty()) {
        emit sendRawHexRequested(text.toUtf8());
        editRawHex->clear();
    }
}

void ViscaTrafficInspectorWidget::updateTransportStats(
    const ::Transport::TransportStatsSnapshot& stats, const ::Visca::ViscaProtocolStats& protoStats)
{
    const auto& gen = stats.generic;
    const double txKb = static_cast<double>(gen.bytesSent) / 1024.0;
    const double rxKb = static_cast<double>(gen.bytesReceived) / 1024.0;
    const double txRateKb = gen.txBytesPerSec / 1024.0;
    const double rxRateKb = gen.rxBytesPerSec / 1024.0;

    const char* s1Str = (protoStats.socket1State == Visca::ViscaSocketState::Executing) ? "Exec"
        : (protoStats.socket1State == Visca::ViscaSocketState::AwaitingAck)             ? "WaitAck"
                                                                                        : "Idle";
    const char* s2Str = (protoStats.socket2State == Visca::ViscaSocketState::Executing) ? "Exec"
        : (protoStats.socket2State == Visca::ViscaSocketState::AwaitingAck)             ? "WaitAck"
                                                                                        : "Idle";

    QString genText = tr("VISCA: TX %1 KB (%2 KB/s) | RX %3 KB (%4 KB/s) | S1:%5 (%6) | S2:%7 (%8) | Lat: %9 ms | "
                         "Errs(Syntax:%10, Full:%11, Cancel:%12, Exec:%13, TO:%14)")
                          .arg(txKb, 0, 'f', 1)
                          .arg(txRateKb, 0, 'f', 1)
                          .arg(rxKb, 0, 'f', 1)
                          .arg(rxRateKb, 0, 'f', 1)
                          .arg(s1Str)
                          .arg(protoStats.socket1Processed)
                          .arg(s2Str)
                          .arg(protoStats.socket2Processed)
                          .arg(protoStats.avgTurnaroundMs, 0, 'f', 1)
                          .arg(protoStats.syntaxErrors)
                          .arg(protoStats.bufferFullErrors)
                          .arg(protoStats.cancelledCommands)
                          .arg(protoStats.executionErrors)
                          .arg(protoStats.timeouts);

    if (lblTransportStats) {
        lblTransportStats->setText(genText);
    }

    if (!lblKernelStats) {
        return;
    }

    if (stats.serial.has_value() && stats.serial->supported) {
        const auto& s = *stats.serial;
        QString serText = tr("[Serial Kernel] Driver Queue: In %1 B / Out %2 B | Hardware Err: Frame %3, Overrun %4, "
                             "Parity %5, Break %6 | Line: CTS %7, DSR %8")
                              .arg(s.queuedRxBytes)
                              .arg(s.queuedTxBytes)
                              .arg(s.framingErrors)
                              .arg(s.fifoOverruns)
                              .arg(s.parityErrors)
                              .arg(s.breakCount)
                              .arg(s.ctsHold ? tr("HOLD") : tr("OK"))
                              .arg(s.dsrHold ? tr("HOLD") : tr("OK"));
        lblKernelStats->setText(serText);
        lblKernelStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #7ee787;");
    } else if (stats.tcp.has_value() && stats.tcp->supported) {
        const auto& t = *stats.tcp;
        const double rttMs = static_cast<double>(t.rttUs) / 1000.0;
        const double rttVarMs = static_cast<double>(t.rttVarUs) / 1000.0;
        QString tcpText = tr(
            "[TCP Kernel] RTT: %1 ms (Var: %2 ms) | CWND: %3 pkts | Retrans: %4 | Lost: %5 | SendQ: %6 B | RecvQ: %7 B")
                              .arg(rttMs, 0, 'f', 1)
                              .arg(rttVarMs, 0, 'f', 1)
                              .arg(t.sndCwnd)
                              .arg(t.totalRetrans)
                              .arg(t.lostSegments)
                              .arg(t.queuedTxBytes)
                              .arg(t.queuedRxBytes);
        lblKernelStats->setText(tcpText);
        lblKernelStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #79c0ff;");
    } else if (stats.udp.has_value() && stats.udp->supported) {
        const auto& u = *stats.udp;
        QString udpText = tr("[UDP Kernel] Socket Buffer: RX %1 B / TX %2 B | RecvQ: %3 B | Kernel Drops: %4")
                              .arg(u.socketRxBufferSize)
                              .arg(u.socketTxBufferSize)
                              .arg(u.queuedRxBytes)
                              .arg(u.rxDroppedPackets);
        lblKernelStats->setText(udpText);
        lblKernelStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #d2a8ff;");
    } else {
        lblKernelStats->setText(tr("Kernel Telemetry: Awaiting active transport channel..."));
        lblKernelStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #8b949e;");
    }
}

} // namespace ViscaApp
