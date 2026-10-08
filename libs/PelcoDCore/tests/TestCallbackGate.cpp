/// @file TestCallbackGate.cpp
/// @brief Unit tests for PelcoD::CallbackGate admission barrier.

#include "CallbackGate.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace {

using namespace std::chrono_literals;

/// @brief A fresh gate admits callbacks; after close() every new admission is rejected.
TEST(CallbackGateTest, GateRejectsAfterClose)
{
    PelcoD::CallbackGate gate {};
    {
        const PelcoD::CallbackGate::Pass pass { gate };
        EXPECT_TRUE(static_cast<bool>(pass));
        EXPECT_EQ(gate.inFlight(), 1U);
    }
    EXPECT_EQ(gate.inFlight(), 0U);
    EXPECT_FALSE(gate.isClosed());

    gate.close();
    EXPECT_TRUE(gate.isClosed());

    const PelcoD::CallbackGate::Pass late { gate };
    EXPECT_FALSE(static_cast<bool>(late));
    EXPECT_EQ(gate.inFlight(), 0U);
}

/// @brief close() blocks until a callback admitted on another thread has left the gate.
TEST(CallbackGateTest, GateCloseWaitsInFlight)
{
    PelcoD::CallbackGate gate {};
    std::promise<void> enteredPromise {};
    auto entered = enteredPromise.get_future();
    std::promise<void> releasePromise {};
    auto release = releasePromise.get_future();

    std::thread callbackThread { [&] {
        const PelcoD::CallbackGate::Pass pass { gate };
        enteredPromise.set_value();
        static_cast<void>(release.wait_for(5s));
    } };
    ASSERT_EQ(entered.wait_for(2s), std::future_status::ready);

    std::atomic<bool> closed { false };
    std::thread closer { [&gate, &closed] {
        gate.close();
        closed.store(true);
    } };

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(closed.load());
    EXPECT_TRUE(gate.isClosed());

    releasePromise.set_value();
    callbackThread.join();
    closer.join();
    EXPECT_TRUE(closed.load());
    EXPECT_EQ(gate.inFlight(), 0U);
}

/// @brief close() from inside a guarded callback (even nested) returns instead of waiting for itself.
TEST(CallbackGateTest, GateReentrantClose)
{
    PelcoD::CallbackGate gate {};
    const PelcoD::CallbackGate::Pass outer { gate };
    ASSERT_TRUE(static_cast<bool>(outer));
    {
        const PelcoD::CallbackGate::Pass inner { gate };
        ASSERT_TRUE(static_cast<bool>(inner));
        gate.close();
        EXPECT_TRUE(gate.isClosed());
        EXPECT_EQ(gate.inFlight(), 2U);
    }
    EXPECT_EQ(gate.inFlight(), 1U);
}

/// @brief A Pass on one gate must not let close() of a different gate skip waiting.
TEST(CallbackGateTest, OtherGatePassDoesNotExempt)
{
    PelcoD::CallbackGate gateA {};
    PelcoD::CallbackGate gateB {};
    const PelcoD::CallbackGate::Pass passA { gateA };
    ASSERT_TRUE(static_cast<bool>(passA));

    gateB.close(); // nothing in flight on B: must return immediately
    EXPECT_TRUE(gateB.isClosed());
    EXPECT_FALSE(gateA.isClosed());
    EXPECT_EQ(gateA.inFlight(), 1U);
}

/// @brief Closing an outer gate from inside an inner gate on the same thread must return immediately.
TEST(CallbackGateTest, NestedDifferentGatesReentrantClose)
{
    PelcoD::CallbackGate outerGate {};
    PelcoD::CallbackGate innerGate {};
    const PelcoD::CallbackGate::Pass outerPass { outerGate };
    ASSERT_TRUE(static_cast<bool>(outerPass));
    {
        const PelcoD::CallbackGate::Pass innerPass { innerGate };
        ASSERT_TRUE(static_cast<bool>(innerPass));

        // Closing outerGate while inside innerPass on the same thread must not deadlock!
        outerGate.close();
        EXPECT_TRUE(outerGate.isClosed());
    }
}

} // namespace
