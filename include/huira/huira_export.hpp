#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef HUIRA_STATIC
    #define HUIRA_EXPORT
  #else
    #ifdef huira_EXPORTS  // CMake sets this for shared libs
      #define HUIRA_EXPORT __declspec(dllexport)
    #else
      #define HUIRA_EXPORT __declspec(dllimport)
    #endif
  #endif
#else
  #define HUIRA_EXPORT __attribute__((visibility("default")))
#endif