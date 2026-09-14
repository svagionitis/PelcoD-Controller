#pragma once

/// @file Connection.h
/// @brief RAII Connection and ScopedConnection classes for callback lifecycle management.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

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

/// @class ScopedConnectionList
/// @brief RAII container managing multiple Connection objects, automatically disconnecting all upon destruction.
/// @details Move-only container simplifying subscription lifecycle management for components with multiple event
/// listeners.
class ScopedConnectionList {
public:
    /// @brief Default constructor for an empty connection list.
    ScopedConnectionList() noexcept = default;

    /// @brief Destructor automatically disconnects all managed connections.
    ~ScopedConnectionList()
    {
        disconnectAll();
    }

    // Non-copyable
    ScopedConnectionList(const ScopedConnectionList&) = delete;
    ScopedConnectionList& operator=(const ScopedConnectionList&) = delete;

    /// @brief Move constructor transfers ownership of all connections.
    /// @param[in,out] other ScopedConnectionList to move from.
    ScopedConnectionList(ScopedConnectionList&& other) noexcept
        : m_connections { std::move(other.m_connections) }
    {
    }

    /// @brief Move assignment operator disconnects current connections and acquires other's.
    /// @param[in,out] other ScopedConnectionList to move from.
    /// @return Reference to this instance.
    ScopedConnectionList& operator=(ScopedConnectionList&& other) noexcept
    {
        if (this != &other) {
            disconnectAll();
            m_connections = std::move(other.m_connections);
        }
        return *this;
    }

    /// @brief Adds a connection to the list.
    /// @param[in] conn Connection to add.
    void add(Connection conn)
    {
        if (conn.isConnected()) {
            m_connections.push_back(std::move(conn));
        }
    }

    /// @brief Adds a connection to the list via addition-assignment operator.
    /// @param[in] conn Connection to add.
    /// @return Reference to this list.
    ScopedConnectionList& operator+=(Connection conn)
    {
        add(std::move(conn));
        return *this;
    }

    /// @brief Disconnects all managed connections.
    void disconnectAll() noexcept
    {
        for (auto& conn : m_connections) {
            conn.disconnect();
        }
    }

    /// @brief Disconnects all connections and clears the container.
    void clear() noexcept
    {
        disconnectAll();
        m_connections.clear();
    }

    /// @brief Gets the number of managed connections in the list.
    /// @return Count of connections.
    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_connections.size();
    }

    /// @brief Checks if the list has no connections.
    /// @return True if empty; false otherwise.
    [[nodiscard]] bool empty() const noexcept
    {
        return m_connections.empty();
    }

private:
    std::vector<Connection> m_connections {};
};

} // namespace PelcoD
