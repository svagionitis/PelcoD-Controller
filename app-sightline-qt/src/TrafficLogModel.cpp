/// @file TrafficLogModel.cpp
/// @brief Implementation of TrafficLogModel for QML views.

#include "TrafficLogModel.h"

TrafficLogModel::TrafficLogModel(QObject* parent, std::size_t maxEntries)
    : QAbstractListModel(parent)
    , m_maxEntries(maxEntries)
{
}

int TrafficLogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant TrafficLogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= m_entries.size()) {
        return {};
    }

    const auto& entry = m_entries[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TimestampRole:
        return entry.timestamp;
    case IsTxRole:
        return entry.isTx;
    case MessageNameRole:
        return entry.messageName;
    case LengthRole:
        return entry.length;
    case CrcOkRole:
        return entry.crcOk;
    case HexPayloadRole:
        return entry.hexPayload;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrafficLogModel::roleNames() const
{
    return { { TimestampRole, "timestamp" }, { IsTxRole, "isTx" }, { MessageNameRole, "messageName" },
        { LengthRole, "frameLength" }, { CrcOkRole, "crcOk" }, { HexPayloadRole, "hexPayload" } };
}

void TrafficLogModel::addEntry(const TrafficLogEntry& entry)
{
    if (m_entries.size() >= m_maxEntries) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_entries.erase(m_entries.begin());
        endRemoveRows();
    }

    const int newRow = static_cast<int>(m_entries.size());
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_entries.push_back(entry);
    endInsertRows();
}

void TrafficLogModel::clearLog()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}
