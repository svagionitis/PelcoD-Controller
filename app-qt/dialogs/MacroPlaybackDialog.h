#pragma once

/// @file MacroPlaybackDialog.h
/// @brief Interactive GUI dialog for authoring, loading, saving, and executing command macros with timing controls.

#include "MacroPlayer.h"
#include "MacroScript.h"

#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

namespace PelcoDApp {

/// @class MacroPlaybackDialog
/// @brief Dialog for managing and playing timed Pelco-D command sequences.
class MacroPlaybackDialog : public QDialog {
    Q_OBJECT

public:
    explicit MacroPlaybackDialog(QWidget* parent = nullptr);
    ~MacroPlaybackDialog() override;

    /// @brief Loads an initial macro sequence into the editor table and engine.
    /// @param[in] sequence Macro sequence to edit/play.
    void loadSequence(const PelcoD::MacroSequence& sequence);

    /// @brief Gets the current sequence configured in the editor.
    /// @return MacroSequence struct.
    [[nodiscard]] PelcoD::MacroSequence sequence() const;

signals:
    /// @brief Emitted when a macro step dispatches a frame.
    /// @param[in] frame Raw frame bytes to transmit.
    void sendFrameRequested(const QByteArray& frame);

private slots:
    void handlePlay();
    void handlePause();
    void handleStop();
    void handleStepNext();
    void handleAddStep();
    void handleRemoveStep();
    void handleClearSteps();
    void handleLoadScript();
    void handleSaveScript();
    void handleSpeedChanged(double speed);
    void handleRepeatChanged(int repeat);

    void onStepExecuted(std::size_t stepIdx, std::size_t totalSteps, const PelcoD::MacroStep& step,
        std::size_t currentLoop, std::size_t totalLoops);
    void onPlayerStateChanged(PelcoD::MacroPlayerState state, const std::string& message);

private:
    void setupUi();
    void updateUiForState(PelcoD::MacroPlayerState state);
    void populateTableFromSequence(const PelcoD::MacroSequence& seq);
    [[nodiscard]] PelcoD::MacroSequence buildSequenceFromUi() const;

    QLineEdit* editMacroName { nullptr };
    QLineEdit* editMacroDesc { nullptr };
    QSpinBox* spinRepeat { nullptr };
    QDoubleSpinBox* spinSpeed { nullptr };

    QTableWidget* tableSteps { nullptr };
    QPushButton* btnAddStep { nullptr };
    QPushButton* btnRemoveStep { nullptr };
    QPushButton* btnClearSteps { nullptr };

    QPushButton* btnPlay { nullptr };
    QPushButton* btnPause { nullptr };
    QPushButton* btnStop { nullptr };
    QPushButton* btnStepNext { nullptr };

    QPushButton* btnLoad { nullptr };
    QPushButton* btnSave { nullptr };

    QProgressBar* progressBar { nullptr };
    QLabel* lblStatus { nullptr };

    std::unique_ptr<PelcoD::MacroPlayer> m_player;
};

} // namespace PelcoDApp
