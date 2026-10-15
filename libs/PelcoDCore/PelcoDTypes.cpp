/// @file PelcoDTypes.cpp
/// @brief Implementation of Pelco-D protocol type conversions and string parsers.

#include "PelcoDTypes.h"

#include <string_view>

namespace PelcoD {

QueryKind parseQueryKind(std::string_view tag) noexcept
{
    if (tag.empty()) {
        return QueryKind::None;
    }

    constexpr std::string_view kProbePrefix { "RttProbe:" };
    if (tag.rfind(kProbePrefix, 0) == 0) {
        tag.remove_prefix(kProbePrefix.size());
    }

    for (const auto& entry : kQueryKindTagTable) {
        if (entry.tag == tag) {
            return entry.kind;
        }
    }

    return QueryKind::Custom;
}

} // namespace PelcoD
