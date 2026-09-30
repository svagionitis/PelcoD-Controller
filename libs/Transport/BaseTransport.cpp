/// @file BaseTransport.cpp
/// @brief Implementation of BaseTransport callback and lifecycle management.

#include "BaseTransport.h"

namespace Transport {

void BaseTransport::setDataCallback(DataReceivedCallback callback)
{
    std::scoped_lock lock(m_callbackMutex);
    m_dataCallback = std::move(callback);
}

void BaseTransport::setStateCallback(StateChangedCallback callback)
{
    std::scoped_lock lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

void BaseTransport::notifyState(TransportState state, const std::string& errorMsg)
{
    StateChangedCallback callback;
    {
        std::scoped_lock lock(m_callbackMutex);
        callback = m_stateCallback;
    }
    if (callback) {
        callback(state, errorMsg);
    }
}

void BaseTransport::invokeDataCallback(const std::vector<std::uint8_t>& data)
{
    recordBytesReceived(data.size());

    DataReceivedCallback callback;
    {
        std::scoped_lock lock(m_callbackMutex);
        callback = m_dataCallback;
    }
    if (callback) {
        callback(data);
    }
}

void BaseTransport::stopReadThread()
{
    m_running.store(false);
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
}

TransportStatsSnapshot BaseTransport::getStats() const
{
    TransportStatsSnapshot snapshot {};
    populateGenericStats(snapshot);
    return snapshot;
}

void BaseTransport::resetStats() noexcept
{
    m_bytesSent.store(0U);
    m_bytesReceived.store(0U);
    m_packetsSent.store(0U);
    m_packetsReceived.store(0U);
    m_txErrorCount.store(0U);
    m_rxErrorCount.store(0U);
    m_reconnectCount.store(0U);

    std::scoped_lock lock(m_timeMutex);
    m_lastTxTime = {};
    m_lastRxTime = {};
    m_rateCalcTime = {};
    m_lastRateBytesSent = 0U;
    m_lastRateBytesReceived = 0U;
    m_txRateBps = 0.0;
    m_rxRateBps = 0.0;
}

void BaseTransport::recordBytesSent(std::size_t bytes) noexcept
{
    m_bytesSent.fetch_add(static_cast<std::uint64_t>(bytes));
    m_packetsSent.fetch_add(1U);

    std::scoped_lock lock(m_timeMutex);
    m_lastTxTime = std::chrono::steady_clock::now();
}

void BaseTransport::recordBytesReceived(std::size_t bytes) noexcept
{
    m_bytesReceived.fetch_add(static_cast<std::uint64_t>(bytes));
    m_packetsReceived.fetch_add(1U);

    std::scoped_lock lock(m_timeMutex);
    m_lastRxTime = std::chrono::steady_clock::now();
}

void BaseTransport::recordTxError() noexcept
{
    m_txErrorCount.fetch_add(1U);
}

void BaseTransport::recordRxError() noexcept
{
    m_rxErrorCount.fetch_add(1U);
}

void BaseTransport::recordReconnect() noexcept
{
    m_reconnectCount.fetch_add(1U);
}

void BaseTransport::populateGenericStats(TransportStatsSnapshot& snapshot) const
{
    snapshot.generic.bytesSent = m_bytesSent.load();
    snapshot.generic.bytesReceived = m_bytesReceived.load();
    snapshot.generic.packetsSent = m_packetsSent.load();
    snapshot.generic.packetsReceived = m_packetsReceived.load();
    snapshot.generic.txErrorCount = m_txErrorCount.load();
    snapshot.generic.rxErrorCount = m_rxErrorCount.load();
    snapshot.generic.reconnectCount = m_reconnectCount.load();

    std::scoped_lock lock(m_timeMutex);
    snapshot.generic.lastTxTime = m_lastTxTime;
    snapshot.generic.lastRxTime = m_lastRxTime;

    const auto now = std::chrono::steady_clock::now();
    if (m_rateCalcTime.time_since_epoch().count() == 0) {
        m_rateCalcTime = now;
        m_lastRateBytesSent = snapshot.generic.bytesSent;
        m_lastRateBytesReceived = snapshot.generic.bytesReceived;
    } else {
        const auto elapsed = std::chrono::duration<double>(now - m_rateCalcTime).count();
        if (elapsed >= 0.25) {
            const double txDiff = (snapshot.generic.bytesSent >= m_lastRateBytesSent)
                ? static_cast<double>(snapshot.generic.bytesSent - m_lastRateBytesSent)
                : 0.0;
            const double rxDiff = (snapshot.generic.bytesReceived >= m_lastRateBytesReceived)
                ? static_cast<double>(snapshot.generic.bytesReceived - m_lastRateBytesReceived)
                : 0.0;
            m_txRateBps = txDiff / elapsed;
            m_rxRateBps = rxDiff / elapsed;
            m_rateCalcTime = now;
            m_lastRateBytesSent = snapshot.generic.bytesSent;
            m_lastRateBytesReceived = snapshot.generic.bytesReceived;
        }
    }
    snapshot.generic.txBytesPerSec = m_txRateBps;
    snapshot.generic.rxBytesPerSec = m_rxRateBps;
}

} // namespace Transport
