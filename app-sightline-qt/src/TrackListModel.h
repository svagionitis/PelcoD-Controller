#pragma once

/// @file TrackListModel.h
/// @brief Qt abstract list model exposing active target track coordinates to QML.

#include <SightlineCore/SightlineTypes.h>

#include <QAbstractListModel>
#include <vector>

class TrackListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum TrackRoles {
        TrackIdRole = Qt::UserRole + 1,
        CenterColRole,
        CenterRowRole,
        WidthRole,
        HeightRole,
        ConfidenceRole,
        IsPrimaryRole,
        VelocityColRole,
        VelocityRowRole
    };

    /// @brief Construct a new TrackListModel.
    /// @details Initializes an empty track list model.
    /// @param parent Optional parent QObject.
    explicit TrackListModel(QObject* parent = nullptr);

    /// @brief Destructor.
    ~TrackListModel() override = default;

    /// @brief Get the row count of tracks.
    /// @details Returns number of currently active target tracks.
    /// @param parent Parent model index (unused for flat list).
    /// @return Number of rows in model.
    Q_INVOKABLE [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// @brief Retrieve model data for a role.
    /// @details Returns coordinate, confidence, or velocity based on role.
    /// @param index Model index.
    /// @param role Target data role.
    /// @return QVariant representing role data.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// @brief Expose role names to QML.
    /// @details Maps enum values to QML property strings.
    /// @return Hash mapping roles to property names.
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// @brief Update internal track coordinates list.
    /// @details Replaces active tracks and emits model reset notifications.
    /// @param tracks Vector of track coordinates.
    void updateTracks(const std::vector<Sightline::TrackCoordinate>& tracks);

    /// @brief Insert or modify an individual track in the model.
    /// @param[in] track Track coordinate information.
    void addOrUpdateTrack(const Sightline::TrackCoordinate& track);

    /// @brief Remove a specific track by its identifier.
    /// @param[in] trackId Track identifier to remove.
    Q_INVOKABLE void removeTrack(int trackId);

    /// @brief Set a specific track as primary and downgrade all others.
    /// @param[in] trackId Track identifier to promote.
    Q_INVOKABLE void setPrimaryTrack(int trackId);

    /// @brief Clear all tracks from the model.
    /// @details Resets the list to empty.
    Q_INVOKABLE void clearTracks();

private:
    std::vector<Sightline::TrackCoordinate> m_tracks {};
};
