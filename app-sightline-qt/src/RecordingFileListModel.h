#pragma once

/// @file RecordingFileListModel.h
/// @brief Qt abstract list model exposing remote storage directory listings to QML.

#include <SightlineCore/modules/SightlineRecording.h>

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <vector>

/// @class RecordingFileListModel
/// @brief Manages remote files catalog on MicroSD/USB storage for QML views.
class RecordingFileListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum FileRoles {
        FilenameRole = Qt::UserRole + 1,
        FileSizeBytesRole,
        FormattedSizeRole,
        TimestampUsRole,
        FormattedDateRole,
        IsPinnedRole,
        FormatTypeRole,
        FormatStringRole
    };

    /// @brief Construct a new RecordingFileListModel.
    /// @param[in] parent Optional parent QObject.
    explicit RecordingFileListModel(QObject* parent = nullptr);

    /// @brief Destructor.
    ~RecordingFileListModel() override = default;

    /// @brief Get the row count of files.
    /// @param[in] parent Parent model index.
    /// @return Number of entries in model.
    Q_INVOKABLE [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// @brief Retrieve model data for a role.
    /// @param[in] index Model index.
    /// @param[in] role Target data role.
    /// @return QVariant representing role data.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// @brief Expose role names to QML.
    /// @return Hash mapping roles to property names.
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// @brief Update internal file entries.
    /// @param[in] entries Vector of directory listing entries.
    void updateEntries(const std::vector<Sightline::DirListEntry>& entries);

    /// @brief Appends additional file entries (pagination).
    /// @param[in] entries Vector of directory listing entries to append.
    void appendEntries(const std::vector<Sightline::DirListEntry>& entries);

    /// @brief Toggles or sets pin status for an entry by filename.
    /// @param[in] filename Target filename.
    /// @param[in] pinned New pin status.
    void setFilePinned(const QString& filename, bool pinned);

    /// @brief Removes an entry from the list by filename.
    /// @param[in] filename Target filename.
    void removeEntry(const QString& filename);

    /// @brief Clears all entries from the model.
    Q_INVOKABLE void clear();

    /// @brief Formats raw bytes into human-readable string (KB, MB, GB).
    /// @param[in] bytes File size in bytes.
    /// @return Formatted human readable size string.
    [[nodiscard]] static QString formatFileSize(quint64 bytes);

    /// @brief Formats microsecond epoch timestamp to readable date/time string.
    /// @param[in] timestampUs Microseconds since UNIX epoch.
    /// @return ISO-like formatted date/time string.
    [[nodiscard]] static QString formatTimestamp(quint64 timestampUs);

    /// @brief Formats format type enum to display text.
    /// @param[in] formatType Format type index.
    /// @return Human-readable format name (e.g. "MPEG-TS", "MP4", "JPEG").
    [[nodiscard]] static QString formatTypeName(quint8 formatType);

private:
    std::vector<Sightline::DirListEntry> m_entries {};
};
