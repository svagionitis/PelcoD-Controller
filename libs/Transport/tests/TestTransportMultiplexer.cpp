/// @file TestTransportMultiplexer.cpp
/// @brief Unit tests for TransportMultiplexer fan-out and multi-channel isolation.

#include "BaseTransport.h"
#include "TransportMultiplexer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
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

    bool setBaudRate(std::uint32_t baudRate) override
    {
        m_baud = baudRate;
        return true;
    }

    [[nodiscard]] std::uint32_t getBaudRate() const noexcept override
    {
        return m_baud;
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getSent() const
    {
        std::scoped_lock lock(m_txMutex);
        return m_sent;
    }

private:
    std::atomic<bool> m_open { true };
    std::uint32_t m_baud { 9600U };
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

/// @brief Verify M11: changing baud rate is rejected when multiple virtual channels share the multiplexer.
TEST(TransportMultiplexerTest, RejectBaudRateChangeWhenMultipleChannelsActive)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    auto chan2 = mux->createChannel();

    EXPECT_FALSE(chan1->setBaudRate(19200U));
    EXPECT_FALSE(chan2->setBaudRate(38400U));

    // When only one channel remains active, baud rate changes are permitted
    chan2.reset();
    EXPECT_TRUE(chan1->setBaudRate(19200U));
}

/// @brief Verify SEI CERT ERR50-CPP/ERR51-CPP: throwing callback in one channel does not escape or crash peer channels.
TEST(TransportMultiplexerTest, ThrowingCallbackDoesNotCrashMultiplexer)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    auto chan2 = mux->createChannel();

    chan1->setDataCallback(
        [](const std::vector<std::uint8_t>&) { throw std::runtime_error("Simulated subscriber crash"); });

    std::vector<std::uint8_t> rx2;
    chan2->setDataCallback([&rx2](const std::vector<std::uint8_t>& data) { rx2 = data; });

    const std::vector<std::uint8_t> testPayload { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };

    // Invocation must NOT throw out of simulateRx and chan2 must receive the payload
    EXPECT_NO_THROW({ dummy->simulateRx(testPayload); });
    EXPECT_EQ(rx2, testPayload);
}

/// @brief Verify virtual channel close() is isolated and leaves peer channels and underlying transport operational.
TEST(TransportMultiplexerTest, ChannelCloseIsIsolated)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);

    auto chan1 = mux->createChannel();
    auto chan2 = mux->createChannel();

    EXPECT_TRUE(chan1->isOpen());
    EXPECT_TRUE(chan2->isOpen());
    EXPECT_TRUE(dummy->isOpen());

    std::vector<std::uint8_t> rx1;
    std::vector<std::uint8_t> rx2;
    chan1->setDataCallback([&rx1](const std::vector<std::uint8_t>& data) { rx1 = data; });
    chan2->setDataCallback([&rx2](const std::vector<std::uint8_t>& data) { rx2 = data; });

    // Close channel 1
    chan1->close();
    EXPECT_FALSE(chan1->isOpen());
    EXPECT_TRUE(chan2->isOpen());
    EXPECT_TRUE(dummy->isOpen());

    const std::vector<std::uint8_t> testPayload { 0xFF, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02 };
    dummy->simulateRx(testPayload);

    EXPECT_TRUE(rx1.empty()); // chan1 must receive nothing after close
    EXPECT_EQ(rx2, testPayload); // chan2 must receive data normally
}

/// @brief Verify finding A5 / channel barrier: setDataCallback(nullptr) or close() waits for in-flight callbacks.
TEST(TransportMultiplexerTest, ChannelDisconnectBlocksUntilInFlightDone)
{
    auto dummy = std::make_shared<DummyTransport>();
    auto mux = std::make_shared<Transport::TransportMultiplexer>(dummy);
    auto chan = mux->createChannel();

    std::promise<void> enteredPromise;
    auto enteredFuture = enteredPromise.get_future();
    std::promise<void> releasePromise;
    auto releaseFuture = releasePromise.get_future().share();
    std::atomic<bool> callbackFinished { false };

    chan->setDataCallback([&enteredPromise, releaseFuture, &callbackFinished](const std::vector<std::uint8_t>&) {
        enteredPromise.set_value();
        static_cast<void>(releaseFuture.wait_for(std::chrono::seconds { 2 }));
        callbackFinished.store(true);
    });

    const std::vector<std::uint8_t> payload { 0x01, 0x02, 0x03 };
    std::thread rxThread([dummy, payload]() { dummy->simulateRx(payload); });

    // Wait until callback is running
    ASSERT_EQ(enteredFuture.wait_for(std::chrono::seconds { 2 }), std::future_status::ready);

    std::atomic<bool> disconnectDone { false };
    std::thread disconnector([&chan, &disconnectDone]() {
        chan->setDataCallback(nullptr);
        disconnectDone.store(true);
    });

    // Verify disconnector is blocking while callback is in flight
    std::this_thread::sleep_for(std::chrono::milliseconds { 80 });
    EXPECT_FALSE(disconnectDone.load());

    // Release callback
    releasePromise.set_value();

    disconnector.join();
    rxThread.join();

    EXPECT_TRUE(disconnectDone.load());
    EXPECT_TRUE(callbackFinished.load());
}

} // namespace
