/// @file SightlineTrafficInspectorWidget.cpp
/// @brief Implementation of Sightline SLA traffic inspector widget.

#include "SightlineTrafficInspectorWidget.h"

#include <SightlineCore/SightlineCrc8.h>
#include <SightlineCore/SightlineProtocolParser.h>
#include <SightlineCore/SightlineTypes.h>

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

SightlineTrafficInspectorWidget::SightlineTrafficInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void SightlineTrafficInspectorWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Top control bar
    auto* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(tr("Filter:"), this));

    cmbFilter = new QComboBox(this);
    cmbFilter->addItem(tr("All Traffic"));
    cmbFilter->addItem(tr("TX Only"));
    cmbFilter->addItem(tr("RX Only"));
    topLayout->addWidget(cmbFilter);

    chkAutoScroll = new QCheckBox(tr("Auto Scroll"), this);
    chkAutoScroll->setChecked(true);
    topLayout->addWidget(chkAutoScroll);

    topLayout->addStretch();

    btnClear = new QPushButton(tr("Clear"), this);
    topLayout->addWidget(btnClear);

    mainLayout->addLayout(topLayout);

    // Table inspector
    tableInspector = new QTableWidget(this);
    tableInspector->setColumnCount(6);
    tableInspector->setHorizontalHeaderLabels(
        { tr("Timestamp"), tr("Dir"), tr("Message"), tr("Len"), tr("CRC"), tr("Hex Payload") });
    tableInspector->horizontalHeader()->setStretchLastSection(true);
    tableInspector->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tableInspector->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tableInspector->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableInspector->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(tableInspector);

    // Stats bar
    auto* statsLayout = new QHBoxLayout();
    lblTransportStats = new QLabel(tr("Bandwidth: TX 0 B/s | RX 0 B/s | Total TX: 0 B | Total RX: 0 B"), this);
    lblKernelStats = new QLabel(tr("Kernel Socket: RX Queue: 0 B | RX Buf: - | TX Buf: -"), this);
    statsLayout->addWidget(lblTransportStats);
    statsLayout->addSpacing(20);
    statsLayout->addWidget(lblKernelStats);
    statsLayout->addStretch();
    mainLayout->addLayout(statsLayout);

    // Hex Injector bottom bar
    auto* injectLayout = new QHBoxLayout();
    injectLayout->addWidget(new QLabel(tr("Send Hex:"), this));
    editRawHex = new QLineEdit(this);
    editRawHex->setPlaceholderText(tr("e.g. 51 AC 02 00 00 (Sightline framed packet)"));
    injectLayout->addWidget(editRawHex);

    btnSendRaw = new QPushButton(tr("Send"), this);
    injectLayout->addWidget(btnSendRaw);
    mainLayout->addLayout(injectLayout);

    // Signals
    connect(btnClear, &QPushButton::clicked, this, &SightlineTrafficInspectorWidget::clearLog);
    connect(btnSendRaw, &QPushButton::clicked, this, &SightlineTrafficInspectorWidget::handleSendClicked);
    connect(editRawHex, &QLineEdit::returnPressed, this, &SightlineTrafficInspectorWidget::handleSendClicked);
    connect(cmbFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &SightlineTrafficInspectorWidget::handleFilterChanged);
}

void SightlineTrafficInspectorWidget::logFrame(bool isTx, const QByteArray& frame)
{
    if (filterMode == 1 && !isTx) {
        return;
    }
    if (filterMode == 2 && isTx) {
        return;
    }

    const int row = tableInspector->rowCount();
    tableInspector->insertRow(row);

    // 0. Timestamp
    const QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    auto* itemTime = new QTableWidgetItem(timeStr);
    tableInspector->setItem(row, 0, itemTime);

    // 1. Dir
    auto* itemDir = new QTableWidgetItem(isTx ? QStringLiteral("TX") : QStringLiteral("RX"));
    if (isTx) {
        itemDir->setForeground(QBrush(QColor(0, 120, 215)));
    } else {
        itemDir->setForeground(QBrush(QColor(0, 180, 80)));
    }
    tableInspector->setItem(row, 1, itemDir);

    // Convert QByteArray to std::vector<std::uint8_t>
    const std::vector<std::uint8_t> stdPacket(reinterpret_cast<const std::uint8_t*>(frame.constData()),
        reinterpret_cast<const std::uint8_t*>(frame.constData()) + frame.size());

    // 2. Message Name
    const auto id = Sightline::SightlineProtocolParser::identifyMessage(stdPacket);
    const QString msgName = QString::fromUtf8(Sightline::messageIdToString(id).data());
    auto* itemMsg = new QTableWidgetItem(msgName);
    tableInspector->setItem(row, 2, itemMsg);

    // 3. Length
    auto* itemLen = new QTableWidgetItem(QString::number(frame.size()));
    tableInspector->setItem(row, 3, itemLen);

    // 4. CRC verification
    bool crcOk = false;
    if (frame.size() >= 4 && static_cast<std::uint8_t>(frame[0]) == Sightline::HeaderByte1
        && static_cast<std::uint8_t>(frame[1]) == Sightline::HeaderByte2) {
        std::size_t hLen = 3U;
        if ((static_cast<std::uint8_t>(frame[2]) & 0x80U) != 0U) {
            hLen = 4U;
        }
        if (stdPacket.size() > hLen) {
            const std::uint8_t computedCrc
                = Sightline::SightlineCrc8::compute(stdPacket.data() + hLen, stdPacket.size() - hLen - 1U);
            crcOk = (computedCrc == stdPacket.back());
        }
    }
    auto* itemCrc = new QTableWidgetItem(crcOk ? QStringLiteral("OK") : QStringLiteral("ERR"));
    itemCrc->setForeground(crcOk ? QBrush(QColor(0, 180, 80)) : QBrush(QColor(220, 50, 50)));
    tableInspector->setItem(row, 4, itemCrc);

    // 5. Hex Payload
    auto* itemPayload = new QTableWidgetItem(frame.toHex(' ').toUpper());
    tableInspector->setItem(row, 5, itemPayload);

    if (chkAutoScroll->isChecked()) {
        tableInspector->scrollToBottom();
    }
}

void SightlineTrafficInspectorWidget::clearLog()
{
    tableInspector->setRowCount(0);
}

void SightlineTrafficInspectorWidget::updateTransportStats(const Transport::TransportStatsSnapshot& stats)
{
    lblTransportStats->setText(tr("Bandwidth: TX %1 B/s | RX %2 B/s | Total TX: %3 B | Total RX: %4 B")
                                   .arg(stats.generic.txBytesPerSec, 0, 'f', 1)
                                   .arg(stats.generic.rxBytesPerSec, 0, 'f', 1)
                                   .arg(stats.generic.bytesSent)
                                   .arg(stats.generic.bytesReceived));

    if (stats.udp.has_value() && stats.udp->supported) {
        lblKernelStats->setText(tr("Kernel Socket: RX Queue: %1 B | RX Buf: %2 B | TX Buf: %3 B | Drops: %4")
                                    .arg(stats.udp->queuedRxBytes)
                                    .arg(stats.udp->socketRxBufferSize)
                                    .arg(stats.udp->socketTxBufferSize)
                                    .arg(stats.udp->rxDroppedPackets));
    } else {
        lblKernelStats->setText(tr("Kernel Socket: Queues: N/A"));
    }
}

void SightlineTrafficInspectorWidget::handleSendClicked()
{
    const QString text = editRawHex->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    const QByteArray hexData = QByteArray::fromHex(text.toUtf8());
    if (!hexData.isEmpty()) {
        emit sendRawHexRequested(hexData);
    }
}

void SightlineTrafficInspectorWidget::handleFilterChanged(int index)
{
    filterMode = index;
}
