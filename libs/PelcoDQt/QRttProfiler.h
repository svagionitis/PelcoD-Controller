#pragma once

/// @file QRttProfiler.h
/// @brief Qt 6 adapter wrapping PelcoD::RttProfiler with signals and slots.

#include "PelcoDDevice.h"
#include "RttProfiler.h"

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

namespace PelcoDQt {

/// @class QRttProfiler
/// @brief Qt QObject adapter for PelcoD::RttProfiler providing event loop marshaled telemetry signals.
class QRttProfiler : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a QRttProfiler associated with a device controller.
    /// @param[in] device Pointer to PelcoDDevice controller.
    /// @param[in] parent Qt parent QObject.
    explicit QRttProfiler(std::shared_ptr<PelcoD::PelcoDDevice> device = nullptr, QObject* parent = nullptr);

    /// @brief Destructor stopping active profiling.
    ~QRttProfiler() override;

    // Non-copyable, non-movable
    QRttProfiler(const QRttProfiler&) = delete;
    QRttProfiler& operator=(const QRttProfiler&) = delete;
    QRttProfiler(QRttProfiler&&) = delete;
    QRttProfiler& operator=(QRttProfiler&&) = delete;

    /// @brief Check if profiler is actively gathering telemetry.
    /// @return True if running; false otherwise.
    [[nodiscard]] bool isRunning() const;

    /// @brief Retrieve snapshot of aggregate statistics.
    /// @return Current RttStatistics record.
    [[nodiscard]] PelcoD::RttStatistics statistics() const;

    /// @brief Retrieve snapshot of recent samples in rolling history.
    /// @return Vector of RttSample items.
    [[nodiscard]] std::vector<PelcoD::RttSample> history() const;

    /// @brief Assign or rebind underlying device controller.
    /// @param[in] device Shared pointer to device controller.
    void setDevice(std::shared_ptr<PelcoD::PelcoDDevice> device);

public slots:
    /// @brief Initiates an active burst of probes.
    /// @param[in] count Number of probes to dispatch.
    /// @param[in] intervalMs Spacing between probe queries in milliseconds.
    /// @param[in] queryTag Command identifier tag.
    /// @return True if burst started successfully.
    bool startBurst(int count = 20, int intervalMs = 200, const QString& queryTag = "QueryPan");

    /// @brief Initiates continuous periodic active probing.
    /// @param[in] intervalMs Spacing between probe queries in milliseconds.
    /// @param[in] queryTag Command identifier tag.
    /// @return True if continuous probe started successfully.
    bool startContinuous(int intervalMs = 200, const QString& queryTag = "QueryPan");

    /// @brief Initiates passive monitoring of existing telemetry queries.
    /// @return True if passive monitoring started successfully.
    bool startPassive();

    /// @brief Stops profiling and unregisters callbacks.
    void stop();

    /// @brief Resets accumulated counters, statistics, and history.
    void reset();

    /// @brief Export collected history and metrics to a CSV file.
    /// @param[in] filePath Target filesystem path.
    /// @return True on success; false on file write error.
    bool exportToCsv(const QString& filePath);

    /// @brief Export collected history and metrics to a JSON file.
    /// @param[in] filePath Target filesystem path.
    /// @return True on success; false on file write error.
    bool exportToJson(const QString& filePath);

signals:
    /// @brief Emitted when a probe sample is ingested.
    /// @param[in] sample Recorded sample record.
    /// @param[in] stats Updated statistics snapshot.
    void sampleRecorded(const PelcoD::RttSample& sample, const PelcoD::RttStatistics& stats);

    /// @brief Emitted when statistical summary metrics are updated.
    /// @param[in] stats Updated statistics snapshot.
    void statisticsUpdated(const PelcoD::RttStatistics& stats);

    /// @brief Emitted on operational running state transitions.
    /// @param[in] isRunning True if actively profiling; false otherwise.
    void stateChanged(bool isRunning);

    /// @brief Emitted when an active burst probe finishes.
    /// @param[in] stats Final statistics summary snapshot.
    void profilingFinished(const PelcoD::RttStatistics& stats);

private:
    void wireCallbacks();

    std::unique_ptr<PelcoD::RttProfiler> m_core;
};

} // namespace PelcoDQt
