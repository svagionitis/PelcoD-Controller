/// @file TrafficInspectorWidget.cpp
/// @brief Implementation of live protocol traffic inspector and hex injector.

#include "TrafficInspectorWidget.h"

#include "PelcoDFrame.h"
#include "TransportStats.h"
#include "app-pelcod-qt/dialogs/MacroPlaybackDialog.h"

#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>

#include <limits>

namespace PelcoDApp {

TrafficInspectorWidget::TrafficInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void TrafficInspectorWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // Toolbar controls
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    auto* lblFilter = new QLabel(tr("Filter:"), this);
    cmbFilter = new QComboBox(this);
    cmbFilter->addItem(tr("All Frames"), 0);
    cmbFilter->addItem(tr("TX Only (Outbound)"), 1);
    cmbFilter->addItem(tr("RX Only (Inbound)"), 2);

    chkAutoScroll = new QCheckBox(tr("Auto-scroll"), this);
    chkAutoScroll->setChecked(true);

    btnClear = new QPushButton(tr("Clear Log"), this);
    btnMacros = new QPushButton(tr("Macros..."), this);
    btnMacros->setToolTip(tr("Open Packet Macro Playback and Hex Scripting"));

    toolbarLayout->addWidget(lblFilter);
    toolbarLayout->addWidget(cmbFilter);
    toolbarLayout->addWidget(chkAutoScroll);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(btnMacros);
    toolbarLayout->addWidget(btnClear);

    mainLayout->addLayout(toolbarLayout);

    // Table Widget
    tableInspector = new QTableWidget(this);
    tableInspector->setColumnCount(4);
    tableInspector->setHorizontalHeaderLabels({ tr("Time"), tr("Dir"), tr("Hex Dump"), tr("Description") });
    tableInspector->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    tableInspector->verticalHeader()->setVisible(false);
    tableInspector->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableInspector->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableInspector->setAlternatingRowColors(true);

    QPalette tablePal = tableInspector->palette();
    tablePal.setColor(QPalette::Base, QColor("#161b22"));
    tablePal.setColor(QPalette::AlternateBase, QColor("#1c2128"));
    tablePal.setColor(QPalette::Text, QColor("#e6edf3"));
    tablePal.setColor(QPalette::Highlight, QColor("#1f6feb"));
    tablePal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    tableInspector->setPalette(tablePal);

    mainLayout->addWidget(tableInspector);

    // Transport and kernel diagnostics bar
    auto* statsFrame = new QFrame(this);
    statsFrame->setFrameShape(QFrame::StyledPanel);
    statsFrame->setStyleSheet(
        "QFrame { background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 2px; }");
    auto* statsLayout = new QVBoxLayout(statsFrame);
    statsLayout->setContentsMargins(6, 4, 6, 4);
    statsLayout->setSpacing(2);

    lblTransportStats = new QLabel(tr("Transport: Idle | TX: 0 B (0 pkts) | RX: 0 B (0 pkts) | Errors: 0"), this);
    lblTransportStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #58a6ff;");
    statsLayout->addWidget(lblTransportStats);

    lblKernelStats = new QLabel(tr("Kernel Telemetry: Awaiting active transport channel..."), this);
    lblKernelStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #8b949e;");
    statsLayout->addWidget(lblKernelStats);

    lblProtocolStats = new QLabel(
        tr("Pelco-D Telemetry: Queries: 0 Sent / 0 Ok / 0 T/O / 0 Retry | CRC Err: 0 | Sync Hunt Drops: 0 B"), this);
    lblProtocolStats->setStyleSheet("font-family: monospace; font-size: 11px; color: #e3b341;");
    statsLayout->addWidget(lblProtocolStats);

    mainLayout->addWidget(statsFrame);

    // Raw hex injection bar
    auto* sendLayout = new QHBoxLayout();
    sendLayout->setSpacing(8);

    auto* lblInject = new QLabel(tr("Inject Hex:"), this);
    editRawHex = new QLineEdit(this);
    editRawHex->setPlaceholderText(tr("e.g. FF 01 00 04 20 00 25"));

    btnSendRaw = new QPushButton(tr("Send"), this);
    btnSendRaw->setObjectName("btnPrimary");

    sendLayout->addWidget(lblInject);
    sendLayout->addWidget(editRawHex);
    sendLayout->addWidget(btnSendRaw);

    mainLayout->addLayout(sendLayout);

    // Connections
    connect(btnClear, &QPushButton::clicked, this, &TrafficInspectorWidget::clearLog);
    connect(btnMacros, &QPushButton::clicked, this, &TrafficInspectorWidget::handleOpenMacros);
    connect(btnSendRaw, &QPushButton::clicked, this, &TrafficInspectorWidget::handleSendClicked);
    connect(editRawHex, &QLineEdit::returnPressed, this, &TrafficInspectorWidget::handleSendClicked);
    connect(cmbFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &TrafficInspectorWidget::handleFilterChanged);
}

void TrafficInspectorWidget::logFrame(bool isTx, const QByteArray& frame, const QString& description)
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

    QFont dirFont = itemDir->font();
    dirFont.setBold(true);
    itemDir->setFont(dirFont);
    itemDir->setTextAlignment(Qt::AlignCenter);

    if (isTx) {
        itemDir->setForeground(QColor("#58a6ff")); // Blue for TX
        itemDir->setBackground(QColor(31, 111, 235, 45)); // Subtle blue badge
    } else {
        itemDir->setForeground(QColor("#3fb950")); // High-contrast green for RX
        itemDir->setBackground(QColor(46, 160, 67, 45)); // Subtle green badge
    }

    tableInspector->setItem(row, 0, itemTime);
    tableInspector->setItem(row, 1, itemDir);
    tableInspector->setItem(row, 2, itemHex);
    tableInspector->setItem(row, 3, itemDesc);

    // Keep log table size bounded to 500 rows
    if (tableInspector->rowCount() > 500) {
        tableInspector->removeRow(0);
    }

    if (chkAutoScroll->isChecked()) {
        tableInspector->scrollToBottom();
    }
}

void TrafficInspectorWidget::clearLog()
{
    tableInspector->setRowCount(0);
}

void TrafficInspectorWidget::handleSendClicked()
{
    const QString qtext = editRawHex->text().trimmed();
    if (qtext.isEmpty()) {
        return;
    }

    const std::string text = qtext.toStdString();
    const std::vector<std::uint8_t> bytes = PelcoD::PelcoDFrame::fromHexString(text);
    if (!bytes.empty()) {
        const QByteArray rawBytes(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
        emit sendRawHexRequested(rawBytes);
    }
}

void TrafficInspectorWidget::handleFilterChanged(int index)
{
    filterMode = index;
}

void TrafficInspectorWidget::handleOpenMacros()
{
    auto* dialog = new MacroPlaybackDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // If there are captured TX frames in the log, offer to seed the macro with them
    PelcoD::MacroSequence seq;
    seq.name = "Traffic Log Export";
    seq.description = "Captured frames from live traffic inspector";
    seq.repeatCount = 1U;

    for (int r = 0; r < tableInspector->rowCount(); ++r) {
        auto* dirItem = tableInspector->item(r, 1);
        auto* hexItem = tableInspector->item(r, 2);
        auto* descItem = tableInspector->item(r, 3);
        if (dirItem && dirItem->text() == "TX" && hexItem) {
            PelcoD::MacroStep step;
            step.frame = PelcoD::PelcoDFrame::fromHexString(hexItem->text().toStdString());
            step.delayMs = 100U;
            if (descItem) {
                step.label = descItem->text().toStdString();
            }
            if (!step.frame.empty()) {
                seq.steps.push_back(std::move(step));
            }
        }
    }

    if (!seq.steps.empty()) {
        dialog->loadSequence(seq);
    }

    connect(dialog, &MacroPlaybackDialog::sendFrameRequested, this, &TrafficInspectorWidget::sendRawHexRequested);
    dialog->show();
}

void TrafficInspectorWidget::updateTransportStats(
    const Transport::TransportStatsSnapshot& stats, const PelcoD::PelcoDProtocolStats& protoStats)
{
    const auto& gen = stats.generic;
    const double txKb = static_cast<double>(gen.bytesSent) / 1024.0;
    const double rxKb = static_cast<double>(gen.bytesReceived) / 1024.0;
    const double txRateKb = gen.txBytesPerSec / 1024.0;
    const double rxRateKb = gen.rxBytesPerSec / 1024.0;

    QString genText
        = tr("Transport: TX: %1 KB (%2 KB/s, %3 pkts, %4 err) | RX: %5 KB (%6 KB/s, %7 pkts, %8 err) | Reconnects: %9")
              .arg(txKb, 0, 'f', 1)
              .arg(txRateKb, 0, 'f', 1)
              .arg(gen.packetsSent)
              .arg(gen.txErrorCount)
              .arg(rxKb, 0, 'f', 1)
              .arg(rxRateKb, 0, 'f', 1)
              .arg(gen.packetsReceived)
              .arg(gen.rxErrorCount)
              .arg(gen.reconnectCount);

    if (lblTransportStats) {
        lblTransportStats->setText(genText);
    }

    if (lblProtocolStats) {
        QString protoText
            = tr("Pelco-D Telemetry: Queries: %1 Tx / %2 Ok / %3 T/O / %4 Retry | Bad CRC: %5 | Sync Drops: "
                 "%6 B | RTT: %7 ms (Min: %8, Avg: %9, Max: %10)")
                  .arg(protoStats.queriesSent)
                  .arg(protoStats.queriesCompleted)
                  .arg(protoStats.queryTimeouts)
                  .arg(protoStats.queryRetries)
                  .arg(protoStats.checksumErrors)
                  .arg(protoStats.discardedSyncBytes)
                  .arg(protoStats.lastRttMs, 0, 'f', 1)
                  .arg(protoStats.minRttMs, 0, 'f', 1)
                  .arg(protoStats.avgRttMs, 0, 'f', 1)
                  .arg(protoStats.maxRttMs, 0, 'f', 1);
        lblProtocolStats->setText(protoText);
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

} // namespace PelcoDApp
