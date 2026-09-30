#pragma once

/// @file TrafficLogModel.h
/// @brief Qt abstract list model exposing real-time packet inspection logs to QML.

#include <QAbstractListModel>
#include <QString>
#include <vector>

struct TrafficLogEntry {
    QString timestamp {};
    bool isTx { false };
    QString messageName {};
    int length { 0 };
    bool crcOk { false };
    QString hexPayload {};
};

class TrafficLogModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum LogRoles {
        TimestampRole = Qt::UserRole + 1,
        IsTxRole,
        MessageNameRole,
        LengthRole,
        CrcOkRole,
        HexPayloadRole
    };

    /// @brief Construct a new TrafficLogModel.
    /// @details Initializes log model with bounded capacity.
    /// @param parent Optional parent QObject.
    /// @param maxEntries Maximum entries to hold before circular dropping.
    explicit TrafficLogModel(QObject* parent = nullptr, std::size_t maxEntries = 500U);

    /// @brief Destructor.
    ~TrafficLogModel() override = default;

    /// @brief Get row count of logs.
    /// @details Returns number of entries currently stored.
    /// @param parent Parent model index.
    /// @return Number of entries.
    Q_INVOKABLE [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// @brief Retrieve model data for a role.
    /// @details Formats timestamp, hex payload, or flags.
    /// @param index Model index.
    /// @param role Target role.
    /// @return QVariant representing role data.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// @brief Expose role names to QML.
    /// @details Maps enum values to QML strings.
    /// @return Hash mapping roles to property names.
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// @brief Append a new traffic entry to the log.
    /// @details Adds entry, trimming oldest if capacity is exceeded.
    /// @param entry New log entry.
    void addEntry(const TrafficLogEntry& entry);

    /// @brief Clear all traffic log entries.
    /// @details Purges all stored logs and refreshes views.
    Q_INVOKABLE void clearLog();

private:
    std::size_t m_maxEntries { 500U };
    std::vector<TrafficLogEntry> m_entries {};
};
