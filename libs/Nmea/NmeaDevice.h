#pragma once

/// @file NmeaDevice.h
/// @brief Asynchronous thread-safe controller managing NMEA 0183 / AIS physical and network streams.

#include "AisDecoder.h"
#include "AisTypes.h"
#include "NmeaSentenceParser.h"
#include "NmeaStreamAccumulator.h"
#include "NmeaTypes.h"
#include "Transport/ITransport.h"
#include "arbiter/NmeaSensorArbiter.h"
#include "environment/ThermalTuningAdvisor.h"
#include "route/NmeaRouteManager.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Nmea {

/// @class NmeaDevice
/// @brief Device controller wrapping Transport::ITransport, providing stream accumulation,
///        real-time NMEA/AIS telemetry parsing, target caching, and thread-safe callback dispatching.
class NmeaDevice {
public:
    using NavCallback = std::function<void(const NmeaNavSnapshot&)>;
    using RadarCallback = std::function<void(const TtmData&)>;
    using AisCallback = std::function<void(const AisVesselTarget&)>;
    using RawSentenceCallback = std::function<void(std::string_view sentence, bool isTx)>;
    using RsdCallback = std::function<void(const RsdData&)>;
    using ApbCallback = std::function<void(const ApbData&)>;
    using MwvCallback = std::function<void(const MwvData&)>;
    using HdgCallback = std::function<void(const HdgData&)>;
    using RmbCallback = std::function<void(const RmbData&)>;
    using RteCallback = std::function<void(const RteData&)>;
    using WplCallback = std::function<void(const WplData&)>;
    using MtwCallback = std::function<void(const MtwData&)>;
    using MmbCallback = std::function<void(const MmbData&)>;
    using MdaCallback = std::function<void(const MdaData&)>;
    using EnvironmentCallback = std::function<void(const NmeaEnvironmentSnapshot&)>;
    using ThermalAdviceCallback = std::function<void(const ThermalTuningAdvice&)>;
    using AttitudeCallback = std::function<void(const AttitudeData&)>;
    using GsaCallback = std::function<void(const GsaData&)>;
    using GsvCallback = std::function<void(const GsvData&)>;
    using ZdaCallback = std::function<void(const ZdaData&)>;
    using VbwCallback = std::function<void(const VbwData&)>;
    using VhwCallback = std::function<void(const VhwData&)>;
    using DptCallback = std::function<void(const DptData&)>;
    using DbtCallback = std::function<void(const DbtData&)>;

    /// @brief Constructs an NmeaDevice wrapping the given physical or network transport.
    /// @param[in] transport Shared pointer to underlying transport (Serial, UDP, TCP).
    /// @param[in] maxAccumulatorBuffer Maximum inbound stream buffer capacity before forced flush.
    explicit NmeaDevice(std::shared_ptr<Transport::ITransport> transport, std::size_t maxAccumulatorBuffer = 4096U);

    virtual ~NmeaDevice();

    // Non-copyable, non-movable
    NmeaDevice(const NmeaDevice&) = delete;
    NmeaDevice& operator=(const NmeaDevice&) = delete;
    NmeaDevice(NmeaDevice&&) = delete;
    NmeaDevice& operator=(NmeaDevice&&) = delete;

    /// @brief Starts communication, attaches callbacks to transport, and opens the channel.
    /// @return True if transport is open and ready.
    [[nodiscard]] bool start();

    /// @brief Stops communication, detaches callbacks, and closes the transport channel.
    void stop();

    /// @brief Checks whether the underlying transport is currently connected and open.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Accesses the underlying transport channel.
    [[nodiscard]] std::shared_ptr<Transport::ITransport> transport() const noexcept;

    /// @brief Transmits a formatted NMEA sentence over the transport medium.
    /// @param[in] sentence Sentence payload (with or without checksum).
    /// @param[in] appendChecksum True to automatically calculate and append *HH\r\n if missing.
    /// @return True if sentence was accepted and transmitted by the transport.
    [[nodiscard]] bool sendSentence(std::string_view sentence, bool appendChecksum = true);

    // --- State and Target Queries ---

    /// @brief Retrieves the latest unified own-ship navigation snapshot.
    [[nodiscard]] NmeaNavSnapshot navSnapshot() const;

    /// @brief Retrieves a list of all currently tracked ARPA radar targets.
    [[nodiscard]] std::vector<TtmData> activeRadarTargets() const;

    /// @brief Retrieves telemetry for a specific radar target number.
    /// @param[in] targetNumber Target tracking number (00..99).
    /// @return TtmData struct if active, std::nullopt otherwise.
    [[nodiscard]] std::optional<TtmData> radarTarget(std::uint32_t targetNumber) const;

    /// @brief Retrieves a list of all currently active AIS vessel targets.
    [[nodiscard]] std::vector<AisVesselTarget> activeAisTargets() const;

    /// @brief Retrieves telemetry for a specific vessel MMSI.
    /// @param[in] mmsi 9-digit Maritime Mobile Service Identity.
    /// @return AisVesselTarget struct if active, std::nullopt otherwise.
    [[nodiscard]] std::optional<AisVesselTarget> aisTarget(std::uint32_t mmsi) const;

    /// @brief Prunes radar and AIS targets that have not received telemetry within their respective TTLs.
    /// @param[in] radarTtl Maximum elapsed time before marking radar track Lost and pruning.
    /// @param[in] aisTtl Maximum elapsed time before pruning vessel track.
    void pruneStaleTargets(std::chrono::milliseconds radarTtl = std::chrono::seconds(10),
        std::chrono::milliseconds aisTtl = std::chrono::seconds(300));

    // --- Subscription Management (Copy-on-Write) ---

    /// @brief Registers a subscriber callback for unified own-ship navigation updates.
    /// @param[in] cb Callback invoked on GGA/RMC/HDT/THS/XDR sentences.
    /// @return Unique subscription ID for unregistering.
    std::size_t addNavCallback(NavCallback cb);

    /// @brief Unregisters a navigation subscriber callback.
    /// @param[in] id Subscription ID returned by addNavCallback.
    void removeNavCallback(std::size_t id);

    /// @brief Registers a subscriber callback for ARPA radar target updates.
    /// @param[in] cb Callback invoked on TTM/TLL sentences.
    /// @return Unique subscription ID for unregistering.
    std::size_t addRadarCallback(RadarCallback cb);

    /// @brief Unregisters a radar target subscriber callback.
    /// @param[in] id Subscription ID returned by addRadarCallback.
    void removeRadarCallback(std::size_t id);

    /// @brief Registers a subscriber callback for AIS vessel target updates.
    /// @param[in] cb Callback invoked when a complete AIS message is decoded.
    /// @return Unique subscription ID for unregistering.
    std::size_t addAisCallback(AisCallback cb);

    /// @brief Unregisters an AIS subscriber callback.
    /// @param[in] id Subscription ID returned by addAisCallback.
    void removeAisCallback(std::size_t id);

    using EmergencyBeaconCallback = std::function<void(const AisEmergencyAlert&)>;

    /// @brief Registers a subscriber callback for AIS-SART, MOB, and EPIRB distress beacon alerts.
    /// @param[in] cb Callback invoked immediately when an emergency beacon or safety broadcast is received.
    /// @return Unique subscription ID for unregistering.
    std::size_t addEmergencyBeaconCallback(EmergencyBeaconCallback cb);

    /// @brief Unregisters an emergency beacon callback.
    /// @param[in] id Subscription ID returned by addEmergencyBeaconCallback.
    void removeEmergencyBeaconCallback(std::size_t id);

    /// @brief Retrieves list of currently active emergency distress beacons.
    [[nodiscard]] std::vector<AisEmergencyAlert> activeEmergencyBeacons() const;

    /// @brief Retrieves an active emergency beacon by MMSI if present.
    [[nodiscard]] std::optional<AisEmergencyAlert> emergencyBeacon(std::uint32_t mmsi) const;

    /// @brief Registers a subscriber callback for raw inbound and outbound NMEA sentences.
    /// @param[in] cb Callback invoked on every transmitted or received sentence.
    /// @return Unique subscription ID for unregistering.
    std::size_t addRawCallback(RawSentenceCallback cb);

    /// @brief Unregisters a raw sentence subscriber callback.
    /// @param[in] id Subscription ID returned by addRawCallback.
    void removeRawCallback(std::size_t id);

    /// @brief Registers a subscriber callback for radar system data / cursor (RSD).
    std::size_t addRsdCallback(RsdCallback cb);
    void removeRsdCallback(std::size_t id);

    /// @brief Registers a subscriber callback for autopilot route navigation (APB).
    std::size_t addApbCallback(ApbCallback cb);
    void removeApbCallback(std::size_t id);

    /// @brief Registers a subscriber callback for wind speed and angle (MWV).
    std::size_t addMwvCallback(MwvCallback cb);
    void removeMwvCallback(std::size_t id);

    /// @brief Registers a subscriber callback for heading, deviation and variation (HDG).
    std::size_t addHdgCallback(HdgCallback cb);
    void removeHdgCallback(std::size_t id);

    /// @brief Retrieves the latest radar system data / cursor telemetry.
    [[nodiscard]] std::optional<RsdData> lastRsd() const;

    /// @brief Retrieves the latest autopilot route navigation telemetry.
    [[nodiscard]] std::optional<ApbData> lastApb() const;

    /// @brief Retrieves the latest wind speed and angle telemetry.
    [[nodiscard]] std::optional<MwvData> lastMwv() const;

    /// @brief Retrieves the latest heading, deviation and variation telemetry.
    [[nodiscard]] std::optional<HdgData> lastHdg() const;

    /// @brief Registers a subscriber callback for recommended minimum navigation info (RMB).
    std::size_t addRmbCallback(RmbCallback cb);
    void removeRmbCallback(std::size_t id);

    /// @brief Registers a subscriber callback for route messages (RTE).
    std::size_t addRteCallback(RteCallback cb);
    void removeRteCallback(std::size_t id);

    /// @brief Registers a subscriber callback for waypoint locations (WPL).
    std::size_t addWplCallback(WplCallback cb);
    void removeWplCallback(std::size_t id);

    /// @brief Registers a subscriber callback for mean water temperature (MTW).
    std::size_t addMtwCallback(MtwCallback cb);
    void removeMtwCallback(std::size_t id);

    /// @brief Registers a subscriber callback for barometric pressure (MMB).
    std::size_t addMmbCallback(MmbCallback cb);
    void removeMmbCallback(std::size_t id);

    /// @brief Registers a subscriber callback for meteorological composite (MDA).
    std::size_t addMdaCallback(MdaCallback cb);
    void removeMdaCallback(std::size_t id);

    /// @brief Registers a subscriber callback for aggregated environmental snapshot updates.
    std::size_t addEnvironmentCallback(EnvironmentCallback cb);
    void removeEnvironmentCallback(std::size_t id);

    /// @brief Registers a subscriber callback for thermal camera tuning advice updates.
    std::size_t addThermalAdviceCallback(ThermalAdviceCallback cb);
    void removeThermalAdviceCallback(std::size_t id);

    /// @brief Registers a subscriber callback for dynamic vessel attitude updates.
    /// @param[in] cb Callback invoked on PASHR, PFEC GPatt, or XDR attitude sentences.
    /// @return Unique subscription ID for unregistering.
    std::size_t addAttitudeCallback(AttitudeCallback cb);

    /// @brief Unregisters an attitude subscriber callback.
    /// @param[in] id Subscription ID returned by addAttitudeCallback.
    void removeAttitudeCallback(std::size_t id);

    /// @brief Registers a subscriber callback for GNSS DOP and active satellites (GSA).
    std::size_t addGsaCallback(GsaCallback cb);
    void removeGsaCallback(std::size_t id);

    /// @brief Registers a subscriber callback for GNSS satellites in view (GSV).
    std::size_t addGsvCallback(GsvCallback cb);
    void removeGsvCallback(std::size_t id);

    /// @brief Registers a subscriber callback for UTC time and date (ZDA).
    std::size_t addZdaCallback(ZdaCallback cb);
    void removeZdaCallback(std::size_t id);

    /// @brief Registers a subscriber callback for dual ground/water speed (VBW).
    std::size_t addVbwCallback(VbwCallback cb);
    void removeVbwCallback(std::size_t id);

    /// @brief Registers a subscriber callback for water speed and heading (VHW).
    std::size_t addVhwCallback(VhwCallback cb);
    void removeVhwCallback(std::size_t id);

    /// @brief Registers a subscriber callback for water depth (DPT).
    std::size_t addDptCallback(DptCallback cb);
    void removeDptCallback(std::size_t id);

    /// @brief Registers a subscriber callback for depth below transducer (DBT).
    std::size_t addDbtCallback(DbtCallback cb);
    void removeDbtCallback(std::size_t id);

    /// @brief Retrieves the shared route manager.
    [[nodiscard]] std::shared_ptr<NmeaRouteManager> routeManager() const noexcept;

    /// @brief Retrieves the shared thermal tuning advisor.
    [[nodiscard]] std::shared_ptr<ThermalTuningAdvisor> thermalAdvisor() const noexcept;

    /// @brief Retrieves the shared sensor redundancy and failover arbiter.
    [[nodiscard]] std::shared_ptr<Arbiter::NmeaSensorArbiter> arbiter() const noexcept;

    /// @brief Retrieves the current environmental snapshot.
    [[nodiscard]] NmeaEnvironmentSnapshot environmentSnapshot() const;

    /// @brief Retrieves the current thermal tuning advice.
    [[nodiscard]] ThermalTuningAdvice thermalAdvice() const;

    /// @brief Retrieves the latest RMB navigation data.
    [[nodiscard]] std::optional<RmbData> lastRmb() const;

    /// @brief Retrieves the latest water temperature data.
    [[nodiscard]] std::optional<MtwData> lastMtw() const;

    /// @brief Retrieves the latest barometric pressure data.
    [[nodiscard]] std::optional<MmbData> lastMmb() const;

    /// @brief Retrieves the latest meteorological composite data.
    [[nodiscard]] std::optional<MdaData> lastMda() const;

    /// @brief Retrieves the latest vessel attitude telemetry.
    [[nodiscard]] std::optional<AttitudeData> lastAttitude() const;

    /// @brief Retrieves the latest GNSS DOP and active satellites.
    [[nodiscard]] std::optional<GsaData> lastGsa() const;

    /// @brief Retrieves the latest UTC time and date.
    [[nodiscard]] std::optional<ZdaData> lastZda() const;

    /// @brief Retrieves the latest dual ground/water speed.
    [[nodiscard]] std::optional<VbwData> lastVbw() const;

    /// @brief Retrieves the latest water speed and heading.
    [[nodiscard]] std::optional<VhwData> lastVhw() const;

    /// @brief Retrieves the latest water depth.
    [[nodiscard]] std::optional<DptData> lastDpt() const;

    /// @brief Retrieves the latest depth below transducer.
    [[nodiscard]] std::optional<DbtData> lastDbt() const;

    /// @brief Ingests simulated or raw sentences directly into the accumulator.
    /// @param[in] rawData Raw byte data.
    void feedRawBytes(const std::vector<std::uint8_t>& rawData);

private:
    void handleIncomingBytes(const std::vector<std::uint8_t>& data);
    void handleTransportState(Transport::TransportState state, const std::string& errorMsg);
    void processSentence(std::string_view sentence);
    void notifyEnvironmentAndThermal();

    template <typename T> struct CallbackList {
        std::size_t nextId { 1U };
        std::shared_ptr<const std::vector<std::pair<std::size_t, T>>> entries {
            std::make_shared<std::vector<std::pair<std::size_t, T>>>()
        };
    };

    template <typename T> std::size_t addCallbackInternal(CallbackList<T>& list, std::mutex& mutex, T cb)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const std::size_t id = list.nextId++;
        auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, T>>>(*list.entries);
        newEntries->emplace_back(id, std::move(cb));
        list.entries = newEntries;
        return id;
    }

    template <typename T> void removeCallbackInternal(CallbackList<T>& list, std::mutex& mutex, std::size_t id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, T>>>();
        newEntries->reserve(list.entries->size());
        for (const auto& item : *list.entries) {
            if (item.first != id) {
                newEntries->push_back(item);
            }
        }
        list.entries = newEntries;
    }

    std::shared_ptr<Transport::ITransport> m_transport;
    NmeaStreamAccumulator m_accumulator;
    AisDecoder m_aisDecoder;

    mutable std::mutex m_lifecycleMutex;
    std::atomic<bool> m_running { false };

    // Navigation snapshot state
    mutable std::mutex m_navMutex;
    NmeaNavSnapshot m_navSnapshot {};
    std::optional<AttitudeData> m_lastAttitude {};

    // Target state
    struct RadarTrackEntry {
        TtmData data {};
        std::chrono::steady_clock::time_point lastUpdated {};
    };
    struct AisTrackEntry {
        AisVesselTarget data {};
        std::chrono::steady_clock::time_point lastUpdated {};
    };

    mutable std::mutex m_targetMutex;
    std::map<std::uint32_t, RadarTrackEntry> m_radarTargets {};
    std::map<std::uint32_t, AisTrackEntry> m_aisTargets {};
    std::map<std::uint32_t, AisEmergencyAlert> m_emergencyBeacons {};

    // Maritime sentence state
    mutable std::mutex m_maritimeMutex;
    std::optional<RsdData> m_lastRsd {};
    std::optional<ApbData> m_lastApb {};
    std::optional<MwvData> m_lastMwv {};
    std::optional<HdgData> m_lastHdg {};
    std::optional<RmbData> m_lastRmb {};
    std::optional<MtwData> m_lastMtw {};
    std::optional<MmbData> m_lastMmb {};
    std::optional<MdaData> m_lastMda {};
    std::optional<GsaData> m_lastGsa {};
    std::optional<ZdaData> m_lastZda {};
    std::optional<VbwData> m_lastVbw {};
    std::optional<VhwData> m_lastVhw {};
    std::optional<DptData> m_lastDpt {};
    std::optional<DbtData> m_lastDbt {};

    std::shared_ptr<NmeaRouteManager> m_routeManager { std::make_shared<NmeaRouteManager>() };
    std::shared_ptr<ThermalTuningAdvisor> m_thermalAdvisor { std::make_shared<ThermalTuningAdvisor>() };
    std::shared_ptr<Arbiter::NmeaSensorArbiter> m_arbiter { std::make_shared<Arbiter::NmeaSensorArbiter>() };

    // Subscriptions
    mutable std::mutex m_callbackMutex;
    CallbackList<NavCallback> m_navCallbacks {};
    CallbackList<RadarCallback> m_radarCallbacks {};
    CallbackList<AisCallback> m_aisCallbacks {};
    CallbackList<EmergencyBeaconCallback> m_emergencyCallbacks {};
    CallbackList<RawSentenceCallback> m_rawCallbacks {};
    CallbackList<RsdCallback> m_rsdCallbacks {};
    CallbackList<ApbCallback> m_apbCallbacks {};
    CallbackList<MwvCallback> m_mwvCallbacks {};
    CallbackList<HdgCallback> m_hdgCallbacks {};
    CallbackList<RmbCallback> m_rmbCallbacks {};
    CallbackList<RteCallback> m_rteCallbacks {};
    CallbackList<WplCallback> m_wplCallbacks {};
    CallbackList<MtwCallback> m_mtwCallbacks {};
    CallbackList<MmbCallback> m_mmbCallbacks {};
    CallbackList<MdaCallback> m_mdaCallbacks {};
    CallbackList<EnvironmentCallback> m_envCallbacks {};
    CallbackList<ThermalAdviceCallback> m_thermalAdviceCallbacks {};
    CallbackList<AttitudeCallback> m_attitudeCallbacks {};
    CallbackList<GsaCallback> m_gsaCallbacks {};
    CallbackList<GsvCallback> m_gsvCallbacks {};
    CallbackList<ZdaCallback> m_zdaCallbacks {};
    CallbackList<VbwCallback> m_vbwCallbacks {};
    CallbackList<VhwCallback> m_vhwCallbacks {};
    CallbackList<DptCallback> m_dptCallbacks {};
    CallbackList<DbtCallback> m_dbtCallbacks {};
};

} // namespace Nmea
