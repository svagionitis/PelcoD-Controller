/// @file TestTransportMultiplexer.cpp
/// @brief Unit tests for TransportMultiplexer fan-out and multi-channel isolation.

#include "BaseTransport.h"
#include "TransportMultiplexer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace {

class DummyTransport final : public Transport::BaseTransport {
public:
    DummyTransport() = default;
    ~DummyTransport() override = default;

    bool open() override
    {
        m_open.store(true);
        notifyState(Transport::TransportState::Connected, "Connected");
        return true;
    }

    void close() override
    {
        m_open.store(false);
        notifyState(Transport::TransportState::Disconnected, "Closed");
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    bool sendData(const std::vector<std::uint8_t>& data) override
    {
        if (!m_open.load()) {
            return false;
        }
        std::scoped_lock lock(m_txMutex);
        m_sent.push_back(data);
        return true;
    }

    void simulateRx(const std::vector<std::uint8_t>& data)
    {
        invokeDataCallback(data);
    }

    void simulateState(Transport::TransportState state, const std::string& msg)
    {
        notifyState(state, msg);
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getSent() const
    {
        std::scoped_lock lock(m_txMutex);
        return m_sent;
    }

private:
    std::atomic<bool> m_open { true };
    mutable std::mutex m_txMutex {};
    std::vector<std::vector<std::uint8_t>> m_sent {};
};

TEST(TransportMultiplexerTest, MultiChannelFanOut)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    auto chan2 = mux->createChannel();
    EXPECT_EQ(mux->channelCount(), 2U);

    std::vector<std::uint8_t> rx1;
    std::vector<std::uint8_t> rx2;
    chan1->setDataCallback([&](const std::vector<std::uint8_t>& data) { rx1 = data; });
    chan2->setDataCallback([&](const std::vector<std::uint8_t>& data) { rx2 = data; });

    const std::vector<std::uint8_t> testPayload { 0xFF, 0x01, 0x00, 0x59, 0x12, 0x34, 0x9F };
    dummy->simulateRx(testPayload);

    EXPECT_EQ(rx1, testPayload);
    EXPECT_EQ(rx2, testPayload);
}

TEST(TransportMultiplexerTest, ChannelDestructionCleansUp)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    {
        auto chan2 = mux->createChannel();
        EXPECT_EQ(mux->channelCount(), 2U);
    }
    // chan2 is destroyed
    EXPECT_EQ(mux->channelCount(), 1U);

    std::vector<std::uint8_t> rx1;
    chan1->setDataCallback([&](const std::vector<std::uint8_t>& data) { rx1 = data; });

    const std::vector<std::uint8_t> testPayload { 0x01, 0x02, 0x03 };
    EXPECT_NO_THROW({ dummy->simulateRx(testPayload); });
    EXPECT_EQ(rx1, testPayload);
}

TEST(TransportMultiplexerTest, ChannelTxForwarding)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    auto chan2 = mux->createChannel();

    const std::vector<std::uint8_t> tx1 { 0xAA, 0xBB };
    const std::vector<std::uint8_t> tx2 { 0xCC, 0xDD };

    EXPECT_TRUE(chan1->sendData(tx1));
    EXPECT_TRUE(chan2->sendData(tx2));

    const auto sent = dummy->getSent();
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0], tx1);
    EXPECT_EQ(sent[1], tx2);
}

TEST(TransportMultiplexerTest, StateChangedFanOut)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    auto chan2 = mux->createChannel();

    std::atomic<int> stateChanges1 { 0 };
    std::atomic<int> stateChanges2 { 0 };

    chan1->setStateCallback([&](Transport::TransportState, const std::string&) { ++stateChanges1; });
    chan2->setStateCallback([&](Transport::TransportState, const std::string&) { ++stateChanges2; });

    dummy->simulateState(Transport::TransportState::Error, "Bus parity error");

    EXPECT_EQ(stateChanges1.load(), 1);
    EXPECT_EQ(stateChanges2.load(), 1);
}

} // namespace
