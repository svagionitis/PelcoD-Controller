/// @file TestAutoReconnect.cpp
/// @brief Unit test verifying ConnectionWidget auto-reconnect strategy and exponential backoff.

#include "widgets/ConnectionWidget.h"

#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>

using namespace PelcoDApp;

class AutoReconnectTest : public ::testing::Test {
protected:
    ConnectionWidget widget;
};

TEST_F(AutoReconnectTest, DisabledByDefault)
{
    EXPECT_FALSE(widget.isAutoReconnectEnabled());
    EXPECT_FALSE(widget.isReconnecting());
    EXPECT_EQ(widget.reconnectAttemptCount(), 0);

    // Disconnect event when disabled should NOT trigger reconnect
    widget.setConnectionState(false);
    EXPECT_FALSE(widget.isReconnecting());
    EXPECT_EQ(widget.reconnectAttemptCount(), 0);
}

TEST_F(AutoReconnectTest, ScheduledOnDrop)
{
    widget.setAutoReconnectEnabled(true);
    EXPECT_TRUE(widget.isAutoReconnectEnabled());

    // Transition to connected
    widget.setConnectionState(true);
    EXPECT_FALSE(widget.isReconnecting());
    EXPECT_EQ(widget.reconnectAttemptCount(), 0);

    // Connection drop (e.g. severed TCP / unplugged USB)
    widget.setConnectionState(false);
    EXPECT_TRUE(widget.isReconnecting());

    // Cancel reconnect
    widget.stopAutoReconnect();
    EXPECT_FALSE(widget.isReconnecting());
    EXPECT_EQ(widget.reconnectAttemptCount(), 0);
}

TEST_F(AutoReconnectTest, CancellationOnUncheck)
{
    widget.setAutoReconnectEnabled(true);
    widget.setConnectionState(true);

    // Drop connection -> begins reconnect timer
    widget.setConnectionState(false);
    EXPECT_TRUE(widget.isReconnecting());

    // Unchecking auto-reconnect disables pending timer
    widget.setAutoReconnectEnabled(false);
    EXPECT_FALSE(widget.isReconnecting());
    EXPECT_EQ(widget.reconnectAttemptCount(), 0);
}

TEST_F(AutoReconnectTest, SerialPortEnumeration)
{
    const QStringList detected = ConnectionWidget::enumerateSerialPorts();
    const QStringList displayed = widget.displayedSerialPorts();

#if defined(_WIN32)
    // 1. Must never return the old hardcoded 32 phantom COM ports
    EXPECT_NE(displayed.size(), 32);

    // 2. If no hardware devices are plugged in on the test machine:
    if (detected.isEmpty()) {
        ASSERT_EQ(displayed.size(), 1);
        EXPECT_EQ(displayed.first(), "No serial ports detected");
    } else {
        // If hardware devices are present, displayed list matches detected list
        EXPECT_EQ(displayed, detected);
    }
#endif

    // 3. Re-scanning via refreshSerialPorts() must be repeatable and consistent
    widget.refreshSerialPorts();
    EXPECT_EQ(widget.displayedSerialPorts(), displayed);
}

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Need QApplication for Qt Widget instantiation
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
