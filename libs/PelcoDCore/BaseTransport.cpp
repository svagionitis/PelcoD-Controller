/// @file BaseTransport.cpp
/// @brief Implementation of BaseTransport callback and lifecycle management.

#include "BaseTransport.h"

namespace PelcoD {

void BaseTransport::setDataCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dataCallback = std::move(callback);
}

void BaseTransport::setStateCallback(StateChangedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

void BaseTransport::notifyState(TransportState state, const std::string& errorMsg)
{
    StateChangedCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        callback = m_stateCallback;
    }
    if (callback) {
        callback(state, errorMsg);
    }
}

void BaseTransport::invokeDataCallback(const std::vector<std::uint8_t>& data)
{
    DataReceivedCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
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

} // namespace PelcoD
