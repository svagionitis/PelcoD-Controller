/// @file MacroPlaybackDialog.cpp
/// @brief Implementation of MacroPlaybackDialog for interactive script authoring and execution.

#include "MacroPlaybackDialog.h"

#include "PelcoDFrame.h"

#include <QColor>
#include <QFileDialog>
#include <QFile>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QTextStream>
#include <QVBoxLayout>

namespace PelcoDApp {

MacroPlaybackDialog::MacroPlaybackDialog(QWidget* parent)
    : QDialog(parent)
    , m_player(std::make_unique<PelcoD::MacroPlayer>())
{
    setupUi();

    m_player->setDispatchCallback([this](const std::vector<std::uint8_t>& frame) {
        const QByteArray bytes(reinterpret_cast<const char*>(frame.data()), static_cast<qsizetype>(frame.size()));
        emit sendFrameRequested(bytes);
    });

    m_player->setStepCallback([this](std::size_t stepIdx, std::size_t totalSteps, const PelcoD::MacroStep& step,
                                  std::size_t currentLoop, std::size_t totalLoops) {
        QMetaObject::invokeMethod(this, [this, stepIdx, totalSteps, step, currentLoop, totalLoops]() {
            onStepExecuted(stepIdx, totalSteps, step, currentLoop, totalLoops);
        }, Qt::QueuedConnection);
    });

    m_player->setStateCallback([this](PelcoD::MacroPlayerState state, const std::string& msg) {
        QMetaObject::invokeMethod(this, [this, state, msg]() {
            onPlayerStateChanged(state, msg);
        }, Qt::QueuedConnection);
    });

    // Provide default starter steps if empty
    PelcoD::MacroSequence initialSeq;
    initialSeq.name = "Quick PTZ Sweep";
    initialSeq.description = "Pan sweep with stop and preset recall";
    initialSeq.repeatCount = 1U;
    initialSeq.steps = {
        { "Pan Left Speed 32", PelcoD::PelcoDFrame::createFrame(1, 0, 0x04, 0x20, 0x00), 200U, false },
        { "Pan Right Speed 32", PelcoD::PelcoDFrame::createFrame(1, 0, 0x02, 0x20, 0x00), 200U, false },
        { "Stop Motion", PelcoD::PelcoDFrame::createFrame(1, 0, 0x00, 0x00, 0x00), 100U, false },
        { "Go To Preset 1", PelcoD::PelcoDFrame::createFrame(1, 0, 0x07, 0x00, 0x01), 500U, false }
    };
    loadSequence(initialSeq);
}

MacroPlaybackDialog::~MacroPlaybackDialog()
{
    if (m_player) {
        m_player->stop();
    }
}

void MacroPlaybackDialog::setupUi()
{
    setWindowTitle(tr("Packet Macro Playback & Hex Scripting"));
    resize(760, 520);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Meta Header Box
    auto* grpMeta = new QGroupBox(tr("Macro Information"), this);
    auto* metaLayout = new QHBoxLayout(grpMeta);
    metaLayout->setSpacing(8);

    auto* lblName = new QLabel(tr("Name:"), this);
    editMacroName = new QLineEdit(this);
    editMacroName->setPlaceholderText(tr("Macro Sequence Name"));

    auto* lblDesc = new QLabel(tr("Description:"), this);
    editMacroDesc = new QLineEdit(this);
    editMacroDesc->setPlaceholderText(tr("Optional notes or test objective"));

    auto* lblRepeat = new QLabel(tr("Repeat:"), this);
    spinRepeat = new QSpinBox(this);
    spinRepeat->setRange(0, 9999);
    spinRepeat->setValue(1);
    spinRepeat->setSpecialValueText(tr("Infinite (0)"));

    auto* lblSpeed = new QLabel(tr("Speed:"), this);
    spinSpeed = new QDoubleSpinBox(this);
    spinSpeed->setRange(0.1, 10.0);
    spinSpeed->setSingleStep(0.25);
    spinSpeed->setValue(1.0);
    spinSpeed->setSuffix("x");

    metaLayout->addWidget(lblName);
    metaLayout->addWidget(editMacroName, 2);
    metaLayout->addWidget(lblDesc);
    metaLayout->addWidget(editMacroDesc, 3);
    metaLayout->addWidget(lblRepeat);
    metaLayout->addWidget(spinRepeat);
    metaLayout->addWidget(lblSpeed);
    metaLayout->addWidget(spinSpeed);

    mainLayout->addWidget(grpMeta);

    // Steps Table
    tableSteps = new QTableWidget(this);
    tableSteps->setColumnCount(4);
    tableSteps->setHorizontalHeaderLabels({ tr("Step #"), tr("Hex Dump"), tr("Delay (ms)"), tr("Description / Label") });
    tableSteps->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableSteps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableSteps->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableSteps->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    tableSteps->verticalHeader()->setVisible(false);
    tableSteps->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableSteps->setAlternatingRowColors(true);

    QPalette tablePal = tableSteps->palette();
    tablePal.setColor(QPalette::Base, QColor("#161b22"));
    tablePal.setColor(QPalette::AlternateBase, QColor("#1c2128"));
    tablePal.setColor(QPalette::Text, QColor("#e6edf3"));
    tablePal.setColor(QPalette::Highlight, QColor("#1f6feb"));
    tablePal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    tableSteps->setPalette(tablePal);

    mainLayout->addWidget(tableSteps);

    // Step modification buttons
    auto* stepBtnLayout = new QHBoxLayout();
    stepBtnLayout->setSpacing(6);

    btnAddStep = new QPushButton(tr("+ Add Step"), this);
    btnRemoveStep = new QPushButton(tr("- Remove Step"), this);
    btnClearSteps = new QPushButton(tr("Clear All"), this);

    btnLoad = new QPushButton(tr("Load Script..."), this);
    btnSave = new QPushButton(tr("Save Script..."), this);

    stepBtnLayout->addWidget(btnAddStep);
    stepBtnLayout->addWidget(btnRemoveStep);
    stepBtnLayout->addWidget(btnClearSteps);
    stepBtnLayout->addStretch();
    stepBtnLayout->addWidget(btnLoad);
    stepBtnLayout->addWidget(btnSave);

    mainLayout->addLayout(stepBtnLayout);

    // Playback control toolbar
    auto* ctrlLayout = new QHBoxLayout();
    ctrlLayout->setSpacing(8);

    btnPlay = new QPushButton(tr("▶ Play"), this);
    btnPlay->setObjectName("btnPrimary");
    btnPlay->setStyleSheet("QPushButton { font-weight: bold; background-color: #238636; color: white; padding: 6px 14px; border-radius: 4px; }");

    btnPause = new QPushButton(tr("⏸ Pause"), this);
    btnPause->setEnabled(false);

    btnStop = new QPushButton(tr("⏹ Stop"), this);
    btnStop->setEnabled(false);

    btnStepNext = new QPushButton(tr("⏭ Step Next"), this);

    ctrlLayout->addWidget(btnPlay);
    ctrlLayout->addWidget(btnPause);
    ctrlLayout->addWidget(btnStop);
    ctrlLayout->addWidget(btnStepNext);
    ctrlLayout->addStretch();

    lblStatus = new QLabel(tr("Ready"), this);
    lblStatus->setStyleSheet("font-weight: bold; color: #58a6ff;");
    ctrlLayout->addWidget(lblStatus);

    mainLayout->addLayout(ctrlLayout);

    // Progress Bar
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setFixedHeight(16);
    mainLayout->addWidget(progressBar);

    // Connections
    connect(btnPlay, &QPushButton::clicked, this, &MacroPlaybackDialog::handlePlay);
    connect(btnPause, &QPushButton::clicked, this, &MacroPlaybackDialog::handlePause);
    connect(btnStop, &QPushButton::clicked, this, &MacroPlaybackDialog::handleStop);
    connect(btnStepNext, &QPushButton::clicked, this, &MacroPlaybackDialog::handleStepNext);

    connect(btnAddStep, &QPushButton::clicked, this, &MacroPlaybackDialog::handleAddStep);
    connect(btnRemoveStep, &QPushButton::clicked, this, &MacroPlaybackDialog::handleRemoveStep);
    connect(btnClearSteps, &QPushButton::clicked, this, &MacroPlaybackDialog::handleClearSteps);

    connect(btnLoad, &QPushButton::clicked, this, &MacroPlaybackDialog::handleLoadScript);
    connect(btnSave, &QPushButton::clicked, this, &MacroPlaybackDialog::handleSaveScript);

    connect(spinSpeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MacroPlaybackDialog::handleSpeedChanged);
    connect(spinRepeat, QOverload<int>::of(&QSpinBox::valueChanged), this, &MacroPlaybackDialog::handleRepeatChanged);
}

void MacroPlaybackDialog::loadSequence(const PelcoD::MacroSequence& sequence)
{
    editMacroName->setText(QString::fromStdString(sequence.name));
    editMacroDesc->setText(QString::fromStdString(sequence.description));
    spinRepeat->setValue(static_cast<int>(sequence.repeatCount));
    populateTableFromSequence(sequence);
    m_player->loadSequence(sequence);
    updateUiForState(PelcoD::MacroPlayerState::Idle);
}

PelcoD::MacroSequence MacroPlaybackDialog::sequence() const
{
    return buildSequenceFromUi();
}

void MacroPlaybackDialog::populateTableFromSequence(const PelcoD::MacroSequence& seq)
{
    tableSteps->setRowCount(0);
    for (std::size_t i = 0; i < seq.steps.size(); ++i) {
        const auto& step = seq.steps[i];
        const int row = tableSteps->rowCount();
        tableSteps->insertRow(row);

        auto* itemIdx = new QTableWidgetItem(QString::number(i + 1));
        itemIdx->setTextAlignment(Qt::AlignCenter);
        itemIdx->setFlags(itemIdx->flags() & ~Qt::ItemIsEditable);

        auto* itemHex = new QTableWidgetItem(QString::fromStdString(PelcoD::PelcoDFrame::toHexString(step.frame)));
        auto* itemDelay = new QTableWidgetItem(QString::number(step.delayMs));
        itemDelay->setTextAlignment(Qt::AlignCenter);
        auto* itemLabel = new QTableWidgetItem(QString::fromStdString(step.label));

        tableSteps->setItem(row, 0, itemIdx);
        tableSteps->setItem(row, 1, itemHex);
        tableSteps->setItem(row, 2, itemDelay);
        tableSteps->setItem(row, 3, itemLabel);
    }
}

PelcoD::MacroSequence MacroPlaybackDialog::buildSequenceFromUi() const
{
    PelcoD::MacroSequence seq;
    seq.name = editMacroName->text().trimmed().toStdString();
    seq.description = editMacroDesc->text().trimmed().toStdString();
    seq.repeatCount = static_cast<std::uint32_t>(spinRepeat->value());

    for (int r = 0; r < tableSteps->rowCount(); ++r) {
        PelcoD::MacroStep step;
        QTableWidgetItem* hexItem = tableSteps->item(r, 1);
        QTableWidgetItem* delayItem = tableSteps->item(r, 2);
        QTableWidgetItem* labelItem = tableSteps->item(r, 3);

        if (hexItem) {
            step.frame = PelcoD::PelcoDFrame::fromHexString(hexItem->text().toStdString());
        }
        if (delayItem) {
            step.delayMs = delayItem->text().toUInt();
            if (step.delayMs == 0) {
                step.delayMs = 50;
            }
        }
        if (labelItem) {
            step.label = labelItem->text().toStdString();
        }

        if (!step.frame.empty()) {
            seq.steps.push_back(std::move(step));
        }
    }

    return seq;
}

void MacroPlaybackDialog::handlePlay()
{
    PelcoD::MacroSequence seq = buildSequenceFromUi();
    std::string err;
    if (!PelcoD::MacroSerializer::validate(seq, &err)) {
        QMessageBox::warning(this, tr("Invalid Macro"), tr("Macro sequence is invalid:\n%1").arg(QString::fromStdString(err)));
        return;
    }

    if (m_player->state() != PelcoD::MacroPlayerState::Paused) {
        m_player->loadSequence(seq);
    }
    m_player->setSpeedMultiplier(spinSpeed->value());
    m_player->start();
    updateUiForState(PelcoD::MacroPlayerState::Playing);
}

void MacroPlaybackDialog::handlePause()
{
    m_player->pause();
    updateUiForState(PelcoD::MacroPlayerState::Paused);
}

void MacroPlaybackDialog::handleStop()
{
    m_player->stop();
    updateUiForState(PelcoD::MacroPlayerState::Stopped);
    progressBar->setValue(0);
}

void MacroPlaybackDialog::handleStepNext()
{
    if (m_player->state() == PelcoD::MacroPlayerState::Idle ||
        m_player->state() == PelcoD::MacroPlayerState::Stopped ||
        m_player->state() == PelcoD::MacroPlayerState::Completed) {
        PelcoD::MacroSequence seq = buildSequenceFromUi();
        m_player->loadSequence(seq);
    }
    m_player->stepNext();
    updateUiForState(PelcoD::MacroPlayerState::Paused);
}

void MacroPlaybackDialog::handleAddStep()
{
    const int row = tableSteps->rowCount();
    tableSteps->insertRow(row);

    auto* itemIdx = new QTableWidgetItem(QString::number(row + 1));
    itemIdx->setTextAlignment(Qt::AlignCenter);
    itemIdx->setFlags(itemIdx->flags() & ~Qt::ItemIsEditable);

    auto* itemHex = new QTableWidgetItem("FF 01 00 00 00 00 01");
    auto* itemDelay = new QTableWidgetItem("100");
    itemDelay->setTextAlignment(Qt::AlignCenter);
    auto* itemLabel = new QTableWidgetItem(tr("Stop Motion"));

    tableSteps->setItem(row, 0, itemIdx);
    tableSteps->setItem(row, 1, itemHex);
    tableSteps->setItem(row, 2, itemDelay);
    tableSteps->setItem(row, 3, itemLabel);
    tableSteps->selectRow(row);
}

void MacroPlaybackDialog::handleRemoveStep()
{
    const int curRow = tableSteps->currentRow();
    if (curRow >= 0) {
        tableSteps->removeRow(curRow);
        // Re-index step numbers
        for (int r = 0; r < tableSteps->rowCount(); ++r) {
            if (auto* it = tableSteps->item(r, 0)) {
                it->setText(QString::number(r + 1));
            }
        }
    }
}

void MacroPlaybackDialog::handleClearSteps()
{
    if (tableSteps->rowCount() > 0 &&
        QMessageBox::question(this, tr("Clear Steps"), tr("Remove all steps from the macro?")) == QMessageBox::Yes) {
        tableSteps->setRowCount(0);
    }
}

void MacroPlaybackDialog::handleLoadScript()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Load Macro Script"), QString(),
        tr("Macro Files (*.json *.hex *.txt);;JSON Macro (*.json);;Hex Script (*.hex *.txt);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1").arg(file.errorString()));
        return;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    try {
        PelcoD::MacroSequence seq;
        if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
            seq = PelcoD::MacroSerializer::fromJson(content.toStdString());
        } else {
            seq = PelcoD::MacroSerializer::fromScript(content.toStdString());
        }
        loadSequence(seq);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, tr("Parse Error"), tr("Failed to parse macro script:\n%1").arg(ex.what()));
    }
}

void MacroPlaybackDialog::handleSaveScript()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Macro Script"), QString(),
        tr("JSON Macro (*.json);;Hex Script (*.hex);;Plain Text (*.txt)"));

    if (filePath.isEmpty()) {
        return;
    }

    PelcoD::MacroSequence seq = buildSequenceFromUi();
    std::string text;
    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        text = PelcoD::MacroSerializer::toJson(seq);
    } else {
        text = PelcoD::MacroSerializer::toScript(seq);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not save file: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(text);
    file.close();
}

void MacroPlaybackDialog::handleSpeedChanged(double speed)
{
    if (m_player) {
        m_player->setSpeedMultiplier(speed);
    }
}

void MacroPlaybackDialog::handleRepeatChanged(int /*repeat*/)
{
    // Repeat change takes effect on next sequence load or start
}

void MacroPlaybackDialog::onStepExecuted(std::size_t stepIdx, std::size_t totalSteps, const PelcoD::MacroStep& /*step*/,
    std::size_t currentLoop, std::size_t totalLoops)
{
    if (stepIdx < static_cast<std::size_t>(tableSteps->rowCount())) {
        tableSteps->selectRow(static_cast<int>(stepIdx));
    }

    if (totalSteps > 0) {
        const int pct = static_cast<int>(((stepIdx + 1) * 100) / totalSteps);
        progressBar->setValue(pct);
    }

    QString statusText;
    if (totalLoops == 0) {
        statusText = tr("Running: Step %1/%2 (Loop %3/∞)").arg(stepIdx + 1).arg(totalSteps).arg(currentLoop);
    } else {
        statusText = tr("Running: Step %1/%2 (Loop %3/%4)").arg(stepIdx + 1).arg(totalSteps).arg(currentLoop).arg(totalLoops);
    }
    lblStatus->setText(statusText);
}

void MacroPlaybackDialog::onPlayerStateChanged(PelcoD::MacroPlayerState state, const std::string& /*message*/)
{
    updateUiForState(state);
    if (state == PelcoD::MacroPlayerState::Completed) {
        lblStatus->setText(tr("Completed all steps"));
        progressBar->setValue(100);
    } else if (state == PelcoD::MacroPlayerState::Stopped) {
        lblStatus->setText(tr("Stopped"));
    } else if (state == PelcoD::MacroPlayerState::Paused) {
        lblStatus->setText(tr("Paused"));
    }
}

void MacroPlaybackDialog::updateUiForState(PelcoD::MacroPlayerState state)
{
    const bool isPlaying = (state == PelcoD::MacroPlayerState::Playing);
    const bool isPaused = (state == PelcoD::MacroPlayerState::Paused);

    btnPlay->setEnabled(!isPlaying);
    btnPause->setEnabled(isPlaying);
    btnStop->setEnabled(isPlaying || isPaused);
    btnStepNext->setEnabled(!isPlaying);

    tableSteps->setEnabled(!isPlaying);
    btnAddStep->setEnabled(!isPlaying);
    btnRemoveStep->setEnabled(!isPlaying);
    btnClearSteps->setEnabled(!isPlaying);
    btnLoad->setEnabled(!isPlaying);
}

} // namespace PelcoDApp
