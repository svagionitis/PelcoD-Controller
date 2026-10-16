#pragma once

/// @file SystemMediaTab.h
/// @brief System setup, RTC sync, SD card recording, movie playback, and log maintenance tab.

#include "FujinonTypes.h"
#include "QPelcoDFujinonDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

namespace FujinonApp {

/// @class SystemMediaTab
/// @brief Tab providing system video mode, SDI formats, address setup, SD recording, and playback.
class SystemMediaTab : public QWidget {
    Q_OBJECT

public:
    explicit SystemMediaTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent = nullptr);
    ~SystemMediaTab() override = default;

public slots:
    /// @brief Update indicators from latest Fujinon telemetry snapshot.
    void updateFujinonStatus(const PelcoD::FujinonStatus& status);

private slots:
    // System & Video Standards
    void handleVideoModeChanged(int index);
    void handleHdFormatChanged(int index);
    void handleReprogramAddress();
    void handleTerminationToggled(bool checked);
    void handleBaudRateChanged(int index);
    void handleLanguageChanged(int index);
    void handleSyncRtc();
    void handleQueryRtc();
    void handleReboot();
    void handleFactoryReset();

    // SD Card & Media Operations
    void handleFormatSD();
    void handleRecordLiveToggled(bool checked);
    void handleEncodeModeChanged(int index);
    void handleSdRecordModeChanged(int index);
    void handleSdCapDispToggled(bool checked);
    void handleRecordLogData();
    void handleQueryLogData();

    // Movie Playback Controls
    void handleSetFirstMovieFile();
    void handlePlayMovieModeChanged(int index);
    void handlePlayMovieToggled(bool checked);
    void handleDeleteMovie();
    void handleQueryMovieCount();
    void handleQueryMovieYear();
    void handleQueryMovieDate();
    void handleQueryMovieTime();

private:
    void setupUi();
    void setupConnections();
    [[nodiscard]] PelcoDQt::QFujinonSX800Device* fujinon() const noexcept;

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    // System
    QComboBox* cmbVideoMode { nullptr };
    QComboBox* cmbHdFormat { nullptr };
    QSpinBox* spinNewAddress { nullptr };
    QPushButton* btnReprogramAddress { nullptr };
    QCheckBox* chkTermination { nullptr };
    QComboBox* cmbBaudRate { nullptr };
    QComboBox* cmbLanguage { nullptr };
    QPushButton* btnSyncRtc { nullptr };
    QPushButton* btnQueryRtc { nullptr };
    QPushButton* btnReboot { nullptr };
    QPushButton* btnFactoryReset { nullptr };

    // SD Card
    QPushButton* btnFormatSD { nullptr };
    QPushButton* btnRecordLive { nullptr };
    QComboBox* cmbEncodeMode { nullptr };
    QComboBox* cmbSdRecordMode { nullptr };
    QCheckBox* chkSdCapacityDisp { nullptr };
    QPushButton* btnRecordLog { nullptr };
    QComboBox* cmbLogType { nullptr };
    QPushButton* btnQueryLog { nullptr };

    // Movie Playback
    QSpinBox* spinMovieFileIndex { nullptr };
    QPushButton* btnSetFirstMovie { nullptr };
    QComboBox* cmbPlayMovieMode { nullptr };
    QPushButton* btnPlayMovie { nullptr };
    QPushButton* btnDeleteMovie { nullptr };
    QPushButton* btnQueryMovieCount { nullptr };
    QPushButton* btnQueryMovieYear { nullptr };
    QPushButton* btnQueryMovieDate { nullptr };
    QPushButton* btnQueryMovieTime { nullptr };
    QLabel* lblMovieInfo { nullptr };
};

} // namespace FujinonApp
