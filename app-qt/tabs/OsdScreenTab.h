#pragma once

/// @file OsdScreenTab.h
/// @brief On-Screen Display (OSD) text labeling and alarm acknowledgment tab.

#include "QPelcoDDevice.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class OsdScreenTab
/// @brief UI tab for displaying custom ASCII labels on screen and managing alarms.
class OsdScreenTab : public QWidget {
    Q_OBJECT

public:
    explicit OsdScreenTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~OsdScreenTab() override = default;

private slots:
    void handleSendText();
    void handleClearScreen();
    void handleAckAlarm();

private:
    void setupUi();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    QLineEdit* editText { nullptr };
    QSpinBox* spinStartCol { nullptr };
    QPushButton* btnSendText { nullptr };
    QPushButton* btnClearScreen { nullptr };

    QSpinBox* spinAlarmId { nullptr };
    QPushButton* btnAckAlarm { nullptr };
};

} // namespace PelcoDApp
