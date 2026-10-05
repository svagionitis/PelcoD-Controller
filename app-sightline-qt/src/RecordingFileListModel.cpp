/// @file RecordingFileListModel.cpp
/// @brief Implementation of RecordingFileListModel.

#include "RecordingFileListModel.h"

#include <algorithm>

RecordingFileListModel::RecordingFileListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int RecordingFileListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant RecordingFileListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_entries.size())) {
        return {};
    }

    const auto& entry = m_entries[static_cast<std::size_t>(row)];

    switch (role) {
    case FilenameRole:
        return QString::fromUtf8(entry.filename.data(), static_cast<qsizetype>(entry.filename.size()));
    case FileSizeBytesRole:
        return static_cast<qulonglong>(entry.fileSizeBytes);
    case FormattedSizeRole:
        return formatFileSize(entry.fileSizeBytes);
    case TimestampUsRole:
        return static_cast<qulonglong>(entry.timestampUs);
    case FormattedDateRole:
        return formatTimestamp(entry.timestampUs);
    case IsPinnedRole:
        return entry.isPinned;
    case FormatTypeRole:
        return static_cast<int>(entry.formatType);
    case FormatStringRole:
        return formatTypeName(entry.formatType);
    default:
        break;
    }

    return {};
}

QHash<int, QByteArray> RecordingFileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FilenameRole] = "filename";
    roles[FileSizeBytesRole] = "fileSizeBytes";
    roles[FormattedSizeRole] = "formattedSize";
    roles[TimestampUsRole] = "timestampUs";
    roles[FormattedDateRole] = "formattedDate";
    roles[IsPinnedRole] = "isPinned";
    roles[FormatTypeRole] = "formatType";
    roles[FormatStringRole] = "formatString";
    return roles;
}

void RecordingFileListModel::updateEntries(const std::vector<Sightline::DirListEntry>& entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void RecordingFileListModel::appendEntries(const std::vector<Sightline::DirListEntry>& entries)
{
    if (entries.empty()) {
        return;
    }

    const int startRow = static_cast<int>(m_entries.size());
    const int endRow = startRow + static_cast<int>(entries.size()) - 1;

    beginInsertRows(QModelIndex(), startRow, endRow);
    m_entries.insert(m_entries.end(), entries.begin(), entries.end());
    endInsertRows();
}

void RecordingFileListModel::setFilePinned(const QString& filename, bool pinned)
{
    const QByteArray nameBytes = filename.toUtf8();
    const std::string nameStr(nameBytes.constData(), static_cast<std::size_t>(nameBytes.size()));
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].filename == nameStr) {
            m_entries[i].isPinned = pinned;
            const QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, { IsPinnedRole });
            break;
        }
    }
}

void RecordingFileListModel::removeEntry(const QString& filename)
{
    const QByteArray nameBytes = filename.toUtf8();
    const std::string nameStr(nameBytes.constData(), static_cast<std::size_t>(nameBytes.size()));
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].filename == nameStr) {
            beginRemoveRows(QModelIndex(), static_cast<int>(i), static_cast<int>(i));
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
            endRemoveRows();
            break;
        }
    }
}

void RecordingFileListModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QString RecordingFileListModel::formatFileSize(quint64 bytes)
{
    constexpr double c_kilo = 1024.0;
    constexpr double c_mega = 1024.0 * 1024.0;
    constexpr double c_giga = 1024.0 * 1024.0 * 1024.0;

    const double b = static_cast<double>(bytes);
    if (b < c_kilo) {
        return QString("%1 B").arg(bytes);
    }
    if (b < c_mega) {
        return QString("%1 KB").arg(QString::number(b / c_kilo, 'f', 1));
    }
    if (b < c_giga) {
        return QString("%1 MB").arg(QString::number(b / c_mega, 'f', 1));
    }
    return QString("%1 GB").arg(QString::number(b / c_giga, 'f', 2));
}

QString RecordingFileListModel::formatTimestamp(quint64 timestampUs)
{
    if (timestampUs == 0ULL) {
        return QStringLiteral("---");
    }
    const qint64 msecs = static_cast<qint64>(timestampUs / 1000ULL);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
    if (!dt.isValid()) {
        return QStringLiteral("---");
    }
    return dt.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
}

QString RecordingFileListModel::formatTypeName(quint8 formatType)
{
    switch (formatType) {
    case 0U:
        return QStringLiteral("MPEG-TS");
    case 1U:
        return QStringLiteral("MP4 (fMP4)");
    case 2U:
        return QStringLiteral("JPEG Still");
    case 3U:
        return QStringLiteral("PNG Still");
    case 4U:
        return QStringLiteral("SLRAW");
    default:
        return QStringLiteral("Media (%1)").arg(formatType);
    }
}
