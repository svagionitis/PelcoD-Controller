#pragma once

/// @file OsdDisplayTab.h
/// @brief On-screen display overlays, framing mode, and OSD menu navigation D-pad tab.

#include "FujinonTypes.h"
#include "QPelcoDFujinonDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QWidget>

namespace FujinonApp {

/// @class OsdDisplayTab
/// @brief Tab providing OSD overlays, corner positioning, date/time formatting, and menu navigation.
class OsdDisplayTab : public QWidget {
    Q_OBJECT

public:
    explicit OsdDisplayTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent = nullptr);
    ~OsdDisplayTab() override = default;

public slots:
    /// @brief Update indicators from latest Fujinon telemetry snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    void handleTimeDisplayToggled(bool checked);
    void handleTimePosChanged(int index);
    void handleTitleDisplayToggled(bool checked);
    void handleTitlePosChanged(int index);
    void handleIdDisplayToggled(bool checked);
    void handleIdPosChanged(int index);
    void handleReticleDisplayToggled(bool checked);

    void handleTimeFormatChanged(int index);
    void handleDateFormatChanged(int index);
    void handleVideoDisplayChanged(int index);

    void handleMenuUp();
    void handleMenuDown();
    void handleMenuLeft();
    void handleMenuRight();
    void handleMenuOk();
    void handleMenuBack();

    void handleQueryDisplay();
    void handleQueryKeySetting();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    // Overlays & Positions
    QCheckBox* chkTimeDisplay { nullptr };
    QComboBox* cmbTimePos { nullptr };
    QCheckBox* chkTitleDisplay { nullptr };
    QComboBox* cmbTitlePos { nullptr };
    QCheckBox* chkIdDisplay { nullptr };
    QComboBox* cmbIdPos { nullptr };
    QCheckBox* chkReticleDisplay { nullptr };

    // Formats
    QComboBox* cmbTimeFormat { nullptr };
    QComboBox* cmbDateFormat { nullptr };
    QComboBox* cmbVideoDisplay { nullptr };

    // Menu D-pad
    QPushButton* btnMenuUp { nullptr };
    QPushButton* btnMenuDown { nullptr };
    QPushButton* btnMenuLeft { nullptr };
    QPushButton* btnMenuRight { nullptr };
    QPushButton* btnMenuOk { nullptr };
    QPushButton* btnMenuBack { nullptr };

    QPushButton* btnQueryDisplay { nullptr };
    QPushButton* btnQueryKeySetting { nullptr };
};

} // namespace FujinonApp
