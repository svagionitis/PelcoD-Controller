#include "LweChannelManager.h"
#include "NmeaTagBlockParser.h"

namespace Nmea::Lwe {

class LweChannelManager::AggregatedTransport : public Transport::BaseTransport {
public:
    explicit AggregatedTransport(LweChannelManager* manager)
        : m_manager(manager)
    {
    }

    ~AggregatedTransport() override
    {
        close();
    }

    bool open() override
    {
        if (m_manager == nullptr) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_manager->m_mutex);
        bool anyOpened = false;
        for (auto& ch : m_manager->m_channels) {
            if (ch->open()) {
                anyOpened = true;
            }
        }

        if (anyOpened) {
            m_isOpen.store(true);
            notifyState(Transport::TransportState::Connected, "");
            return true;
        }
        notifyState(Transport::TransportState::Error, "Failed to open any multicast channels");
        return false;
    }

    void close() override
    {
        m_isOpen.store(false);
        if (m_manager != nullptr) {
            std::lock_guard<std::mutex> lock(m_manager->m_mutex);
            for (auto& ch : m_manager->m_channels) {
                ch->close();
            }
        }
        notifyState(Transport::TransportState::Disconnected, "");
    }

    bool isOpen() const noexcept override
    {
        return m_isOpen.load();
    }

    bool sendData(const std::vector<std::uint8_t>& data) override
    {
        if (m_manager == nullptr) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_manager->m_mutex);
        bool anySent = false;
        for (auto& ch : m_manager->m_channels) {
            if (ch->isOpen() && ch->sendData(data)) {
                anySent = true;
            }
        }
        return anySent;
    }

    void forwardData(const std::vector<std::uint8_t>& data)
    {
        invokeDataCallback(data);
    }

private:
    LweChannelManager* m_manager { nullptr };
    std::atomic<bool> m_isOpen { false };
};

LweChannelManager::LweChannelManager()
    : m_aggregatedTransport(std::make_shared<AggregatedTransport>(this))
{
}

LweChannelManager::~LweChannelManager()
{
    leaveAll();
}

bool LweChannelManager::joinTransmissionGroup(TransmissionGroup tg, const std::string& interfaceIp)
{
    return joinCustomGroup(getTransmissionGroupIp(tg), getTransmissionGroupPort(tg), interfaceIp);
}

bool LweChannelManager::joinCustomGroup(const std::string& groupIp, std::uint16_t port, const std::string& interfaceIp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if channel already exists
    for (const auto& ch : m_channels) {
        if (ch->getGroupAddress() == groupIp && ch->getPort() == port) {
            return true;
        }
    }

    auto channel = std::make_shared<LweMulticastTransport>(groupIp, port, interfaceIp);
    channel->setDataCallback([this](const std::vector<std::uint8_t>& data) { handleIncomingChannelData(data); });

    m_channels.push_back(channel);

    // If aggregated transport is already open, open this new channel immediately
    if (m_aggregatedTransport && m_aggregatedTransport->isOpen()) {
        (void)channel->open();
    }

    return true;
}

void LweChannelManager::leaveAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& ch : m_channels) {
        ch->close();
    }
    m_channels.clear();
}

std::size_t LweChannelManager::channelCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_channels.size();
}

void LweChannelManager::setSourceFilter(const std::vector<std::string>& allowedSystemIds)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sourceWhitelist.clear();
    m_sourceWhitelist.insert(allowedSystemIds.begin(), allowedSystemIds.end());
    m_useSourceFilter = true;
}

void LweChannelManager::clearSourceFilter()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sourceWhitelist.clear();
    m_useSourceFilter = false;
}

std::shared_ptr<Transport::ITransport> LweChannelManager::getAggregatedTransport()
{
    return m_aggregatedTransport;
}

void LweChannelManager::handleIncomingChannelData(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return;
    }

    // Source filtering by Tag Block system ID
    bool allow = true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_useSourceFilter) {
            const std::string_view raw(reinterpret_cast<const char*>(data.data()), data.size());
            NmeaTagBlock block {};
            std::string_view remainder {};
            if (NmeaTagBlockParser::parse(raw, block, remainder)) {
                if (m_sourceWhitelist.find(block.sourceId) == m_sourceWhitelist.end()) {
                    allow = false;
                }
            }
        }
    }

    if (allow && m_aggregatedTransport) {
        m_aggregatedTransport->forwardData(data);
    }
}

} // namespace Nmea::Lwe
