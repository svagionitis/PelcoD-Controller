#pragma once

/// @file HttplibInclude.h
/// @brief Single include point for the third-party cpp-httplib header.
/// @details cpp-httplib instantiates STL templates (std::transform, std::pair) with implicit
///          signed/unsigned conversions. Under MSVC `/W4 /w14365 /WX` these surface as C4365 inside
///          the STL and break the build, even though the offending code is third-party. This header
///          scopes the suppression to the httplib include only, so project code keeps the full
///          warning set. Include this header instead of `<httplib.h>` everywhere in the project.
/// @note Deviation record: MISRA C++:2023 Rule 4.1.1 / project checklist item 12. The suppression
///       applies to third-party code only and is documented here.

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4365) // signed/unsigned mismatch inside STL instantiated by httplib
#endif

#include <httplib.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
