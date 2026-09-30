/// @file TrackListModel.cpp
/// @brief Implementation of TrackListModel for QML views.

#include "TrackListModel.h"

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
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return { { TrackIdRole, "trackId" }, { CenterColRole, "centerCol" }, { CenterRowRole, "centerRow" },
        { WidthRole, "trackWidth" }, { HeightRole, "trackHeight" }, { ConfidenceRole, "confidence" },
        { IsPrimaryRole, "isPrimary" }, { VelocityColRole, "velocityCol" }, { VelocityRowRole, "velocityRow" } };
}

void TrackListModel::updateTracks(const std::vector<Sightline::TrackCoordinate>& tracks)
{
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
}

void TrackListModel::clearTracks()
{
    beginResetModel();
    m_tracks.clear();
    endResetModel();
}
