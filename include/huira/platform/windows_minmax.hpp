
// Protect against Windows min/max macros for header-only library
#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Also undef them if they're already defined
#undef min
#undef max
#endif
