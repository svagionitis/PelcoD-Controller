#pragma once

/// @file DayNightTab.h
/// @brief Day/Night switching, scheduling, optical filter, and IR bandpass tab.

#include "FujinonTypes.h"
#include "QPelcoDFujinonDevice.h"

#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QWidget>

namespace FujinonApp {

/// @class DayNightTab
/// @brief Tab providing Day/Night switching modes, thresholds, schedules, and IR cut/pass filters.
class DayNightTab : public QWidget {
    Q_OBJECT

public:
    explicit DayNightTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent = nullptr);
    ~DayNightTab() override = default;

public slots:
    /// @brief Update indicators from latest Fujinon telemetry snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    void handleDayNightModeChanged(int index);
    void handleDayNightExChanged(int index);
    void handleSetDayToNightThreshold();
    void handleSetNightToDayThreshold();
    void handleSetAutoDelay();
    void handleSetDayStartTime();
    void handleSetNightStartTime();
    void handleDayFilterChanged(int index);
    void handleNightFilterChanged(int index);
    void handleIrWavelengthChanged(int index);
    void handleExternalDnChanged(int index);
    void handleQueryDayNight();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    // Modes
    QComboBox* cmbDayNight { nullptr };
    QComboBox* cmbDayNightEx { nullptr };

    // Thresholds & Delays
    QSpinBox* spinDayToNight { nullptr };
    QPushButton* btnSetDayToNight { nullptr };
    QSpinBox* spinNightToDay { nullptr };
    QPushButton* btnSetNightToDay { nullptr };
    QSpinBox* spinAutoDelay { nullptr };
    QPushButton* btnSetAutoDelay { nullptr };

    // Schedules
    QTimeEdit* timeDayStart { nullptr };
    QPushButton* btnSetDayStart { nullptr };
    QTimeEdit* timeNightStart { nullptr };
    QPushButton* btnSetNightStart { nullptr };

    // Optical Filters & IR
    QComboBox* cmbDayFilter { nullptr };
    QComboBox* cmbNightFilter { nullptr };
    QComboBox* cmbIrWavelength { nullptr };
    QComboBox* cmbExternalDn { nullptr };

    QPushButton* btnQueryDayNight { nullptr };
};

} // namespace FujinonApp
