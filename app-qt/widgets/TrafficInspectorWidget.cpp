/// @file TrafficInspectorWidget.cpp
/// @brief Implementation of live protocol traffic inspector and hex injector.

#include "TrafficInspectorWidget.h"

#include "PelcoDFrame.h"

#include <QDateTime>
#include <QFont>
#include <QHeaderView>
#include <QLabel>

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

    toolbarLayout->addWidget(lblFilter);
    toolbarLayout->addWidget(cmbFilter);
    toolbarLayout->addWidget(chkAutoScroll);
    toolbarLayout->addStretch();
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

} // namespace PelcoDApp
