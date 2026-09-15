/// @file QRttProfiler.cpp
/// @brief Implementation of Qt 6 adapter wrapping PelcoD::RttProfiler.

#include "QRttProfiler.h"

#include <QMetaObject>
#include <fstream>

namespace PelcoDQt {

QRttProfiler::QRttProfiler(std::shared_ptr<PelcoD::PelcoDDevice> device, QObject* parent)
    : QObject(parent)
    , m_core(std::make_unique<PelcoD::RttProfiler>(std::move(device)))
{
    wireCallbacks();
}

QRttProfiler::~QRttProfiler()
{
    if (m_core) {
        m_core->stop();
    }
}

void QRttProfiler::wireCallbacks()
{
    if (!m_core) {
        return;
    }

    m_core->setSampleCallback([this](const PelcoD::RttSample& sample, const PelcoD::RttStatistics& stats) {
        QMetaObject::invokeMethod(
            this,
            [this, sample, stats]() {
                emit sampleRecorded(sample, stats);
                emit statisticsUpdated(stats);
            },
            Qt::QueuedConnection);
    });

    m_core->setStatisticsCallback([this](const PelcoD::RttStatistics& stats) {
        QMetaObject::invokeMethod(
            this, [this, stats]() { emit statisticsUpdated(stats); }, Qt::QueuedConnection);
    });

    m_core->setStateChangedCallback([this](bool isRunning) {
        QMetaObject::invokeMethod(
            this, [this, isRunning]() { emit stateChanged(isRunning); }, Qt::QueuedConnection);
    });

    m_core->setFinishedCallback([this](const PelcoD::RttStatistics& stats) {
        QMetaObject::invokeMethod(
            this, [this, stats]() { emit profilingFinished(stats); }, Qt::QueuedConnection);
    });
}

bool QRttProfiler::isRunning() const
{
    return m_core ? m_core->isRunning() : false;
}

PelcoD::RttStatistics QRttProfiler::statistics() const
{
    return m_core ? m_core->getStatistics() : PelcoD::RttStatistics {};
}

std::vector<PelcoD::RttSample> QRttProfiler::history() const
{
    return m_core ? m_core->getHistory() : std::vector<PelcoD::RttSample> {};
}

void QRttProfiler::setDevice(std::shared_ptr<PelcoD::PelcoDDevice> device)
{
    if (m_core) {
        m_core->setDevice(std::move(device));
    }
}

bool QRttProfiler::startBurst(int count, int intervalMs, const QString& queryTag)
{
    if (!m_core) {
        return false;
    }
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::ActiveBurst;
    cfg.burstCount = static_cast<std::uint32_t>(std::max(1, count));
    cfg.intervalMs = static_cast<std::uint32_t>(std::max(10, intervalMs));
    cfg.probeQueryTag = queryTag.toStdString();
    return m_core->start(cfg);
}

bool QRttProfiler::startContinuous(int intervalMs, const QString& queryTag)
{
    if (!m_core) {
        return false;
    }
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::ActiveContinuous;
    cfg.intervalMs = static_cast<std::uint32_t>(std::max(10, intervalMs));
    cfg.probeQueryTag = queryTag.toStdString();
    return m_core->start(cfg);
}

bool QRttProfiler::startPassive()
{
    if (!m_core) {
        return false;
    }
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    return m_core->start(cfg);
}

void QRttProfiler::stop()
{
    if (m_core) {
        m_core->stop();
    }
}

void QRttProfiler::reset()
{
    if (m_core) {
        m_core->reset();
    }
}

bool QRttProfiler::exportToCsv(const QString& filePath)
{
    if (!m_core) {
        return false;
    }
    std::ofstream ofs(filePath.toStdString());
    if (!ofs.is_open()) {
        return false;
    }
    return m_core->exportCsv(ofs);
}

bool QRttProfiler::exportToJson(const QString& filePath)
{
    if (!m_core) {
        return false;
    }
    std::ofstream ofs(filePath.toStdString());
    if (!ofs.is_open()) {
        return false;
    }
    return m_core->exportJson(ofs);
}

} // namespace PelcoDQt
