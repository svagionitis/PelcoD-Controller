#include "ThermalTuningAdvisor.h"

#include <algorithm>
#include <cmath>

namespace Nmea {

void ThermalTuningAdvisor::ingestMtw(const MtwData& mtw)
{
    if (!mtw.valid) {
        return;
    }

    AdviceCallback adviceCb {};
    SnapshotCallback snapCb {};
    NmeaEnvironmentSnapshot snapCopy {};
    ThermalTuningAdvice adviceCopy {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.waterTemperatureCelsius = mtw.waterTemperatureCelsius;
        m_snapshot.hasWaterTemp = true;
        m_snapshot.timestamp = std::chrono::steady_clock::now();

        if (m_snapshot.airTemperatureCelsius.has_value()) {
            m_snapshot.seaAirDeltaTCelsius = *m_snapshot.airTemperatureCelsius - *m_snapshot.waterTemperatureCelsius;
        }

        recomputeAdviceLocked();
        adviceCb = m_adviceCallback;
        snapCb = m_snapshotCallback;
        snapCopy = m_snapshot;
        adviceCopy = m_currentAdvice;
    }

    if (snapCb) {
        snapCb(snapCopy);
    }
    if (adviceCb) {
        adviceCb(adviceCopy);
    }
}

void ThermalTuningAdvisor::ingestMmb(const MmbData& mmb)
{
    if (!mmb.valid) {
        return;
    }

    AdviceCallback adviceCb {};
    SnapshotCallback snapCb {};
    NmeaEnvironmentSnapshot snapCopy {};
    ThermalTuningAdvice adviceCopy {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 1 bar = 1000 hPa / mbar
        m_snapshot.barometricPressureHpa = mmb.pressureBars * 1000.0;
        m_snapshot.hasPressure = true;
        m_snapshot.timestamp = std::chrono::steady_clock::now();

        recomputeAdviceLocked();
        adviceCb = m_adviceCallback;
        snapCb = m_snapshotCallback;
        snapCopy = m_snapshot;
        adviceCopy = m_currentAdvice;
    }

    if (snapCb) {
        snapCb(snapCopy);
    }
    if (adviceCb) {
        adviceCb(adviceCopy);
    }
}

void ThermalTuningAdvisor::ingestMda(const MdaData& mda)
{
    if (!mda.valid) {
        return;
    }

    AdviceCallback adviceCb {};
    SnapshotCallback snapCb {};
    NmeaEnvironmentSnapshot snapCopy {};
    ThermalTuningAdvice adviceCopy {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (mda.waterTemperatureCelsius.has_value()) {
            m_snapshot.waterTemperatureCelsius = mda.waterTemperatureCelsius;
            m_snapshot.hasWaterTemp = true;
        }
        if (mda.airTemperatureCelsius.has_value()) {
            m_snapshot.airTemperatureCelsius = mda.airTemperatureCelsius;
            m_snapshot.hasAirTemp = true;
        }
        if (mda.relativeHumidityPercent.has_value()) {
            m_snapshot.relativeHumidityPercent = mda.relativeHumidityPercent;
            m_snapshot.hasHumidity = true;
        }
        if (mda.dewPointCelsius.has_value()) {
            m_snapshot.dewPointCelsius = mda.dewPointCelsius;
            m_snapshot.hasDewPoint = true;
        }
        if (mda.barometricPressureBars.has_value()) {
            m_snapshot.barometricPressureHpa = *mda.barometricPressureBars * 1000.0;
            m_snapshot.hasPressure = true;
        }
        if (mda.windSpeedKnots.has_value()) {
            m_snapshot.windSpeedKnots = mda.windSpeedKnots;
            m_snapshot.hasWind = true;
        }
        if (mda.windDirectionTrueDeg.has_value()) {
            m_snapshot.windDirectionTrueDeg = mda.windDirectionTrueDeg;
        }

        if (m_snapshot.airTemperatureCelsius.has_value() && m_snapshot.waterTemperatureCelsius.has_value()) {
            m_snapshot.seaAirDeltaTCelsius = *m_snapshot.airTemperatureCelsius - *m_snapshot.waterTemperatureCelsius;
        }

        m_snapshot.timestamp = std::chrono::steady_clock::now();

        recomputeAdviceLocked();
        adviceCb = m_adviceCallback;
        snapCb = m_snapshotCallback;
        snapCopy = m_snapshot;
        adviceCopy = m_currentAdvice;
    }

    if (snapCb) {
        snapCb(snapCopy);
    }
    if (adviceCb) {
        adviceCb(adviceCopy);
    }
}

void ThermalTuningAdvisor::updateEnvironment(std::optional<double> waterTempC, std::optional<double> airTempC,
    std::optional<double> relHumPercent, std::optional<double> dewPointC, std::optional<double> windSpeedKts)
{
    AdviceCallback adviceCb {};
    SnapshotCallback snapCb {};
    NmeaEnvironmentSnapshot snapCopy {};
    ThermalTuningAdvice adviceCopy {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (waterTempC.has_value()) {
            m_snapshot.waterTemperatureCelsius = waterTempC;
            m_snapshot.hasWaterTemp = true;
        }
        if (airTempC.has_value()) {
            m_snapshot.airTemperatureCelsius = airTempC;
            m_snapshot.hasAirTemp = true;
        }
        if (relHumPercent.has_value()) {
            m_snapshot.relativeHumidityPercent = relHumPercent;
            m_snapshot.hasHumidity = true;
        }
        if (dewPointC.has_value()) {
            m_snapshot.dewPointCelsius = dewPointC;
            m_snapshot.hasDewPoint = true;
        }
        if (windSpeedKts.has_value()) {
            m_snapshot.windSpeedKnots = windSpeedKts;
            m_snapshot.hasWind = true;
        }

        if (m_snapshot.airTemperatureCelsius.has_value() && m_snapshot.waterTemperatureCelsius.has_value()) {
            m_snapshot.seaAirDeltaTCelsius = *m_snapshot.airTemperatureCelsius - *m_snapshot.waterTemperatureCelsius;
        }

        m_snapshot.timestamp = std::chrono::steady_clock::now();

        recomputeAdviceLocked();
        adviceCb = m_adviceCallback;
        snapCb = m_snapshotCallback;
        snapCopy = m_snapshot;
        adviceCopy = m_currentAdvice;
    }

    if (snapCb) {
        snapCb(snapCopy);
    }
    if (adviceCb) {
        adviceCb(adviceCopy);
    }
}

NmeaEnvironmentSnapshot ThermalTuningAdvisor::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

ThermalTuningAdvice ThermalTuningAdvisor::advice() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentAdvice;
}

void ThermalTuningAdvisor::setAdviceCallback(AdviceCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_adviceCallback = std::move(cb);
}

void ThermalTuningAdvisor::setSnapshotCallback(SnapshotCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotCallback = std::move(cb);
}

void ThermalTuningAdvisor::recomputeAdviceLocked()
{
    ThermalTuningAdvice adv {};
    adv.timestamp = m_snapshot.timestamp;

    // 1. Calculate thermal crossover / washout risk
    if (m_snapshot.seaAirDeltaTCelsius.has_value()) {
        const double absDelta = std::abs(*m_snapshot.seaAirDeltaTCelsius);
        if (absDelta < 3.0) {
            adv.washoutRiskScore = std::clamp((3.0 - absDelta) / 3.0, 0.0, 1.0);
        } else {
            adv.washoutRiskScore = 0.0;
        }
    }

    // 2. Calculate marine fog / condensation risk
    if (m_snapshot.airTemperatureCelsius.has_value() && m_snapshot.dewPointCelsius.has_value()) {
        const double spread = *m_snapshot.airTemperatureCelsius - *m_snapshot.dewPointCelsius;
        const double hum = m_snapshot.relativeHumidityPercent.value_or(70.0);
        if (spread < 3.0 && hum > 75.0) {
            const double spreadFactor = std::clamp((3.0 - spread) / 3.0, 0.0, 1.0);
            const double humFactor = std::clamp((hum - 75.0) / 25.0, 0.0, 1.0);
            adv.fogRiskScore = 0.5 * spreadFactor + 0.5 * humFactor;
        }
    } else if (m_snapshot.relativeHumidityPercent.has_value() && *m_snapshot.relativeHumidityPercent > 90.0) {
        adv.fogRiskScore = std::clamp((*m_snapshot.relativeHumidityPercent - 90.0) / 10.0, 0.0, 1.0);
    }

    // 3. Evaluate Polarity
    if (m_snapshot.waterTemperatureCelsius.has_value() && m_snapshot.airTemperatureCelsius.has_value()) {
        if (*m_snapshot.waterTemperatureCelsius > *m_snapshot.airTemperatureCelsius + 5.0) {
            adv.polarity = ThermalPolarityAdvice::BlackHot;
        } else {
            adv.polarity = ThermalPolarityAdvice::WhiteHot;
        }
    }

    // 4. Select AGC Preset & DDE Strength
    const double windKts = m_snapshot.windSpeedKnots.value_or(0.0);

    if (adv.fogRiskScore >= 0.60) {
        adv.agcPreset = ThermalAgcPreset::MarineFogPenetration;
        adv.ddeStrengthPercent = std::clamp(70.0 + 25.0 * adv.fogRiskScore, 0.0, 100.0);
        adv.rationale = "High humidity / fog condensation detected; applying spatial high-pass and boosted DDE.";
    } else if (adv.washoutRiskScore >= 0.50) {
        adv.agcPreset = ThermalAgcPreset::CrossoverEnhanced;
        adv.ddeStrengthPercent = std::clamp(65.0 + 30.0 * adv.washoutRiskScore, 0.0, 100.0);
        adv.rationale = "Thermal crossover (|T_air - T_water| < 1.5C); applying plateau equalization AGC.";
    } else if (windKts >= 18.0) {
        adv.agcPreset = ThermalAgcPreset::SeaClutterSuppression;
        adv.ddeStrengthPercent = 40.0;
        adv.rationale = "High sea state / whitecap clutter detected; applying temporal wave noise suppression.";
    } else {
        adv.agcPreset = ThermalAgcPreset::DefaultNormal;
        adv.ddeStrengthPercent = 30.0;
        adv.rationale = "Nominal maritime atmospheric conditions; standard dynamic range histogram.";
    }

    m_currentAdvice = adv;
}

} // namespace Nmea
