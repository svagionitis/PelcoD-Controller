/// @file ViscaTrafficInspectorWidget.cpp
/// @brief Implementation of live VISCA protocol traffic monitor.

#include "ViscaTrafficInspectorWidget.h"

#include <QColor>
#include <QDateTime>
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

} // namespace ViscaApp
