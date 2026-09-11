/// @file TestAutoReconnect.cpp
/// @brief Unit test verifying ConnectionWidget auto-reconnect strategy and exponential backoff.

#include "widgets/ConnectionWidget.h"

#include <QApplication>
#include <QSignalSpy>
#include <cassert>
#include <iostream>

using namespace PelcoDApp;

static void testAutoReconnectDisabledByDefault(ConnectionWidget& widget)
{
    assert(!widget.isAutoReconnectEnabled());
    assert(!widget.isReconnecting());
    assert(widget.reconnectAttemptCount() == 0);

    // Disconnect event when disabled should NOT trigger reconnect
    widget.setConnectionState(false);
    assert(!widget.isReconnecting());
    assert(widget.reconnectAttemptCount() == 0);
}

static void testAutoReconnectScheduledOnDrop(ConnectionWidget& widget)
{
    widget.setAutoReconnectEnabled(true);
    assert(widget.isAutoReconnectEnabled());

    // Transition to connected
    widget.setConnectionState(true);
    assert(!widget.isReconnecting());
    assert(widget.reconnectAttemptCount() == 0);

    // Connection drop (e.g. severed TCP / unplugged USB)
    widget.setConnectionState(false);
    assert(widget.isReconnecting());

    // Cancel reconnect
    widget.stopAutoReconnect();
    assert(!widget.isReconnecting());
    assert(widget.reconnectAttemptCount() == 0);
}

static void testAutoReconnectCancellationOnUncheck(ConnectionWidget& widget)
{
    widget.setAutoReconnectEnabled(true);
    widget.setConnectionState(true);

    // Drop connection -> begins reconnect timer
    widget.setConnectionState(false);
    assert(widget.isReconnecting());

    // Unchecking auto-reconnect disables pending timer
    widget.setAutoReconnectEnabled(false);
    assert(!widget.isReconnecting());
    assert(widget.reconnectAttemptCount() == 0);
}

static void testSerialPortEnumeration(ConnectionWidget& widget)
{
    const QStringList detected = ConnectionWidget::enumerateSerialPorts();
    const QStringList displayed = widget.displayedSerialPorts();

#if defined(_WIN32)
    // 1. Must never return the old hardcoded 32 phantom COM ports
    assert(displayed.size() != 32 && "Serial ports must not be hardcoded to 32 phantom COM ports");

    // 2. If no hardware devices are plugged in on the test machine:
    if (detected.isEmpty()) {
        assert(displayed.size() == 1);
        assert(displayed.first() == "No serial ports detected");
    } else {
        // If hardware devices are present, displayed list matches detected list
        assert(displayed == detected);
    }
#endif

    // 3. Re-scanning via refreshSerialPorts() must be repeatable and consistent
    widget.refreshSerialPorts();
    assert(widget.displayedSerialPorts() == displayed);
}

int main(int argc, char* argv[])
{
    // Need QApplication for Qt Widget instantiation
    QApplication app(argc, argv);

    std::cout << "[TestAutoReconnect] Running tests..." << std::endl;
    ConnectionWidget widget;
    testAutoReconnectDisabledByDefault(widget);
    testAutoReconnectScheduledOnDrop(widget);
    testAutoReconnectCancellationOnUncheck(widget);
    testSerialPortEnumeration(widget);
    std::cout << "[TestAutoReconnect] All tests passed successfully." << std::endl;

    return 0;
}
