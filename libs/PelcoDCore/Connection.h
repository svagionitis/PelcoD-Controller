#pragma once

/// @file Connection.h
/// @brief RAII Connection and ScopedConnection classes for callback lifecycle management.

#include <cstdint>
#include <functional>
#include <utility>

namespace PelcoD {

/// @brief Numeric identifier for registered callbacks.
using CallbackId = std::uint64_t;

/// @class Connection
/// @brief Represents a connection to a callback or event subscriber.
/// @details Allows disconnecting the callback explicitly. Thread-safe and idempotent.
class Connection {
public:
    /// @brief Default constructor for an empty/inactive connection.
    Connection() noexcept = default;

    /// @brief Constructs an active connection with a custom disconnect routine.
    /// @param[in] disconnectFn Callback invoked when disconnect() is called.
    explicit Connection(std::function<void()> disconnectFn)
        : m_disconnect { std::move(disconnectFn) }
    {
    }

    /// @brief Default copy constructor.
    Connection(const Connection&) = default;

    /// @brief Default copy assignment operator.
    Connection& operator=(const Connection&) = default;

    /// @brief Default move constructor.
    Connection(Connection&&) noexcept = default;

    /// @brief Default move assignment operator.
    Connection& operator=(Connection&&) noexcept = default;

    ~Connection() = default;

    /// @brief Disconnects the associated callback.
    /// @details Safe to call multiple times or on an uninitialized connection. Idempotent.
    void disconnect()
    {
        std::function<void()> fn = std::exchange(m_disconnect, nullptr);
        if (fn) {
            fn();
        }
    }

    /// @brief Checks if this connection is currently active.
    /// @return True if connected; false otherwise.
    [[nodiscard]] bool isConnected() const noexcept
    {
        return static_cast<bool>(m_disconnect);
    }

private:
    std::function<void()> m_disconnect {};
};

/// @class ScopedConnection
/// @brief RAII connection manager that automatically disconnects upon destruction.
/// @details Non-copyable, movable. Ideal for binding callback lifetime to a viewer or dialog lifetime.
class ScopedConnection {
public:
    /// @brief Default constructor for an unassigned scoped connection.
    ScopedConnection() noexcept = default;

    /// @brief Takes ownership of an existing connection.
    /// @param[in] conn Connection to manage.
    explicit ScopedConnection(Connection conn) noexcept
        : m_conn { std::move(conn) }
    {
    }

    /// @brief Destructor automatically disconnects the managed connection.
    ~ScopedConnection()
    {
        disconnect();
    }

    // Non-copyable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    /// @brief Move constructor transfers ownership of the connection.
    /// @param[in,out] other ScopedConnection to move from.
    ScopedConnection(ScopedConnection&& other) noexcept
        : m_conn { std::move(other.m_conn) }
    {
    }

    /// @brief Move assignment operator disconnects current connection and takes ownership of other.
    /// @param[in,out] other ScopedConnection to move from.
    /// @return Reference to this instance.
    ScopedConnection& operator=(ScopedConnection&& other) noexcept
    {
        if (this != &other) {
            disconnect();
            m_conn = std::move(other.m_conn);
        }
        return *this;
    }

    /// @brief Replaces current connection with a new one, disconnecting the old one.
    /// @param[in] conn New connection to manage.
    /// @return Reference to this instance.
    ScopedConnection& operator=(Connection conn) noexcept
    {
        disconnect();
        m_conn = std::move(conn);
        return *this;
    }

    /// @brief Manually disconnects the managed callback early.
    void disconnect() noexcept
    {
        m_conn.disconnect();
    }

    /// @brief Releases ownership of the connection without disconnecting it.
    /// @return The previously managed Connection object.
    [[nodiscard]] Connection release() noexcept
    {
        Connection released = std::move(m_conn);
        return released;
    }

    /// @brief Checks if the managed connection is currently active.
    /// @return True if connected; false otherwise.
    [[nodiscard]] bool isConnected() const noexcept
    {
        return m_conn.isConnected();
    }

private:
    Connection m_conn {};
};

} // namespace PelcoD
