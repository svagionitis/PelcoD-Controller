/// @file NmeaStreamAccumulator.cpp
/// @brief Implementation of thread-safe NMEA stream accumulator.

#include "NmeaStreamAccumulator.h"
#include "NmeaTagBlockParser.h"

#include <algorithm>

namespace Nmea {

NmeaStreamAccumulator::NmeaStreamAccumulator(std::size_t maxBufferSize)
    : m_maxBufferSize { (maxBufferSize >= 128U) ? maxBufferSize : DefaultMaxBufferSize }
{
    m_buffer.reserve(512U);
}

std::vector<std::string> NmeaStreamAccumulator::push(const std::vector<std::uint8_t>& data, bool validateChecksum)
{
    return push(data.data(), data.size(), validateChecksum);
}

std::vector<std::string> NmeaStreamAccumulator::push(const std::uint8_t* data, std::size_t size, bool validateChecksum)
{
    if (data == nullptr || size == 0U) {
        return {};
    }
    const std::string_view sv(reinterpret_cast<const char*>(data), size);
    return push(sv, validateChecksum);
}

namespace {

    bool isValidCandidate(const std::string& candidate)
    {
        if (candidate.empty()) {
            return false;
        }
        if (candidate.front() == '\\') {
            NmeaTagBlock tb {};
            std::string_view remainder {};
            if (!NmeaTagBlockParser::parse(candidate, tb, remainder)) {
                return false;
            }
            return NmeaChecksum::validate(remainder);
        }
        return NmeaChecksum::validate(candidate);
    }

} // namespace

std::vector<std::string> NmeaStreamAccumulator::push(std::string_view text, bool validateChecksum)
{
    std::scoped_lock lock(m_mutex);
    std::vector<std::string> sentences {};

    m_buffer.append(text);

    // Overflow protection: if no start delimiter or no end delimiter within max size, discard oldest bytes
    if (m_buffer.size() > m_maxBufferSize) {
        const auto start = m_buffer.find_first_of("$!\\");
        if (start != std::string::npos && start > 0U) {
            m_buffer.erase(0U, start);
        } else if (start == std::string::npos) {
            m_buffer.clear();
            return sentences;
        } else if (m_buffer.size() > m_maxBufferSize) {
            // Delimiter was at 0, but buffer is still oversized without newline; prune
            m_buffer.erase(0U, m_buffer.size() - 256U);
        }
    }

    while (!m_buffer.empty()) {
        const auto start = m_buffer.find_first_of("$!\\");
        if (start == std::string::npos) {
            m_buffer.clear();
            break;
        }
        if (start > 0U) {
            m_buffer.erase(0U, start);
        }

        const auto newline = m_buffer.find('\n');
        if (newline == std::string::npos) {
            // Incomplete sentence, wait for more data
            break;
        }

        // Complete candidate sentence [0 .. newline]
        std::string candidate = m_buffer.substr(0U, newline + 1U);
        m_buffer.erase(0U, newline + 1U);

        if (!validateChecksum || isValidCandidate(candidate)) {
            sentences.push_back(std::move(candidate));
        }
    }

    return sentences;
}

void NmeaStreamAccumulator::clear()
{
    std::scoped_lock lock(m_mutex);
    m_buffer.clear();
}

std::size_t NmeaStreamAccumulator::size() const
{
    std::scoped_lock lock(m_mutex);
    return m_buffer.size();
}

std::size_t NmeaStreamAccumulator::maxBufferSize() const noexcept
{
    return m_maxBufferSize;
}

} // namespace Nmea
