/// @file TrackListModel.cpp
/// @brief Implementation of TrackListModel for QML views.

#include "TrackListModel.h"

#include <algorithm>

TrackListModel::TrackListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TrackListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_tracks.size());
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= m_tracks.size()) {
        return {};
    }

    const auto& track = m_tracks[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TrackIdRole:
        return track.trackId;
    case CenterColRole:
        return track.centerCol;
    case CenterRowRole:
        return track.centerRow;
    case WidthRole:
        return track.width;
    case HeightRole:
        return track.height;
    case ConfidenceRole:
        return track.confidence;
    case IsPrimaryRole:
        return track.isPrimary;
    case VelocityColRole:
        return track.velocityCol;
    case VelocityRowRole:
        return track.velocityRow;
    case IsCoastingRole:
        return track.isCoasting;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return { { TrackIdRole, "trackId" }, { CenterColRole, "centerCol" }, { CenterRowRole, "centerRow" },
        { WidthRole, "trackWidth" }, { HeightRole, "trackHeight" }, { ConfidenceRole, "confidence" },
        { IsPrimaryRole, "isPrimary" }, { VelocityColRole, "velocityCol" }, { VelocityRowRole, "velocityRow" },
        { IsCoastingRole, "isCoasting" } };
}

void TrackListModel::updateTracks(const std::vector<Sightline::TrackCoordinate>& tracks)
{
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
}

void TrackListModel::addOrUpdateTrack(const Sightline::TrackCoordinate& track)
{
    const auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
        [targetId = track.trackId](const Sightline::TrackCoordinate& t) { return t.trackId == targetId; });

    if (it != m_tracks.end()) {
        *it = track;
        const auto row = static_cast<int>(std::distance(m_tracks.begin(), it));
        const auto idx = index(row, 0);
        emit dataChanged(idx, idx);
    } else {
        const auto newRow = static_cast<int>(m_tracks.size());
        beginInsertRows(QModelIndex {}, newRow, newRow);
        m_tracks.push_back(track);
        endInsertRows();
    }
}

void TrackListModel::removeTrack(int trackId)
{
    const auto target = static_cast<std::uint8_t>(trackId);
    const auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
        [target](const Sightline::TrackCoordinate& t) { return t.trackId == target; });

    if (it != m_tracks.end()) {
        const auto row = static_cast<int>(std::distance(m_tracks.begin(), it));
        beginRemoveRows(QModelIndex {}, row, row);
        m_tracks.erase(it);
        endRemoveRows();
    }
}

void TrackListModel::setPrimaryTrack(int trackId)
{
    if (m_tracks.empty()) {
        return;
    }
    const auto target = static_cast<std::uint8_t>(trackId);
    bool changed { false };
    for (auto& track : m_tracks) {
        const bool shouldBePrimary = (track.trackId == target);
        if (track.isPrimary != shouldBePrimary) {
            track.isPrimary = shouldBePrimary;
            changed = true;
        }
    }
    if (changed) {
        emit dataChanged(index(0, 0), index(static_cast<int>(m_tracks.size()) - 1, 0), { IsPrimaryRole });
    }
}

void TrackListModel::clearTracks()
{
    beginResetModel();
    m_tracks.clear();
    endResetModel();
}
