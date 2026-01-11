#pragma once

// Macro for suppressing unreachable code warnings around exception definitions
#ifdef _MSC_VER
#define HUIRA_SUPPRESS_UNREACHABLE_BEGIN \
        __pragma(warning(push)) \
        __pragma(warning(disable: 4702))
#define HUIRA_SUPPRESS_UNREACHABLE_END \
        __pragma(warning(pop))
#else
#define HUIRA_SUPPRESS_UNREACHABLE_BEGIN
#define HUIRA_SUPPRESS_UNREACHABLE_END
#endif