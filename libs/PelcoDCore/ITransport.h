#pragma once

/// @file ITransport.h
/// @brief Compatibility forwarder pointing to protocol-agnostic Transport library.

#include <Transport/ITransport.h>
#include <Transport/TransportTypes.h>

namespace PelcoD {
using TransportState = ::Transport::TransportState;
using ITransport = ::Transport::ITransport;
} // namespace PelcoD
