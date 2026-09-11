/// @file OsdScreenTab.cpp
/// @brief Implementation of OSD labeling and alarm acknowledgment tab.

#include "OsdScreenTab.h"
#include "ProtocolBuilder.h"

namespace PelcoDApp {

OsdScreenTab::OsdScreenTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
}

void OsdScreenTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // Group 1: On-Screen Text Display
    auto* grpText = new QGroupBox(tr("On-Screen Display Text (Columns 0 - 39)"), this);
    auto* layoutText = new QVBoxLayout(grpText);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel(tr("Start Column:")));
    spinStartCol = new QSpinBox();
    spinStartCol->setRange(0, 39);
    spinStartCol->setValue(0);
    row1->addWidget(spinStartCol);

    row1->addWidget(new QLabel(tr("Text String:")));
    editText = new QLineEdit();
    editText->setMaxLength(40);
    editText->setText("PELCO-D CAMERA 01");
    row1->addWidget(editText);

    btnSendText = new QPushButton(tr("Write to Screen"));
    btnSendText->setObjectName("btnPrimary");
    row1->addWidget(btnSendText);

    layoutText->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    btnClearScreen = new QPushButton(tr("Clear Screen Display"));
    btnClearScreen->setObjectName("btnDanger");
    row2->addWidget(btnClearScreen);
    row2->addStretch();
    layoutText->addLayout(row2);

    mainLayout->addWidget(grpText);

    // Group 2: Alarm Acknowledgment
    auto* grpAlarm = new QGroupBox(tr("Alarm Handling (Alarms 1 - 8)"), this);
    auto* layoutAlarm = new QHBoxLayout(grpAlarm);

    layoutAlarm->addWidget(new QLabel(tr("Alarm ID:")));
    spinAlarmId = new QSpinBox();
    spinAlarmId->setRange(1, 8);
    spinAlarmId->setValue(1);
    layoutAlarm->addWidget(spinAlarmId);

    btnAckAlarm = new QPushButton(tr("Acknowledge Alarm"));
    layoutAlarm->addWidget(btnAckAlarm);
    layoutAlarm->addStretch();

    mainLayout->addWidget(grpAlarm);
    mainLayout->addStretch();

    // Signal Connections
    connect(btnSendText, &QPushButton::clicked, this, &OsdScreenTab::handleSendText);
    connect(btnClearScreen, &QPushButton::clicked, this, &OsdScreenTab::handleClearScreen);
    connect(btnAckAlarm, &QPushButton::clicked, this, &OsdScreenTab::handleAckAlarm);
}

void OsdScreenTab::handleSendText()
{
    const QString text = editText->text();
    int col = spinStartCol->value();

    for (const QChar& ch : text) {
        if (col > 39) {
            break;
        }
        const auto frame = PelcoD::ProtocolBuilder::buildWriteChar(
            m_device->currentStatus().address, static_cast<std::uint8_t>(col), ch.toLatin1());
        const QByteArray bytes(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
        m_device->sendRawHex(bytes);
        ++col;
    }
}

void OsdScreenTab::handleClearScreen()
{
    const auto frame = PelcoD::ProtocolBuilder::buildClearScreen(m_device->currentStatus().address);
    const QByteArray bytes(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
    m_device->sendRawHex(bytes);
}

void OsdScreenTab::handleAckAlarm()
{
    const auto frame = PelcoD::ProtocolBuilder::buildAlarmAck(
        m_device->currentStatus().address, static_cast<std::uint8_t>(spinAlarmId->value()));
    const QByteArray bytes(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
    m_device->sendRawHex(bytes);
}

} // namespace PelcoDApp
