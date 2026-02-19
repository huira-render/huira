########################################
### Architecture / Optimization Flags ###
########################################

# Detect native arch flags (used for both this project and vcpkg triplet overlay)
set(HUIRA_ARCH_C_FLAGS "")
set(HUIRA_ARCH_CXX_FLAGS "")

if(HUIRA_NATIVE_ARCH)
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MSVC: /arch:AVX2 or /arch:AVX512 — detect what's available
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("/arch:AVX512" MSVC_HAS_AVX512)
        check_cxx_compiler_flag("/arch:AVX2" MSVC_HAS_AVX2)

        if(MSVC_HAS_AVX512)
            set(HUIRA_ARCH_C_FLAGS "/arch:AVX512")
            set(HUIRA_ARCH_CXX_FLAGS "/arch:AVX512")
            message(STATUS "Native arch: AVX-512 (MSVC)")
        elseif(MSVC_HAS_AVX2)
            set(HUIRA_ARCH_C_FLAGS "/arch:AVX2")
            set(HUIRA_ARCH_CXX_FLAGS "/arch:AVX2")
            message(STATUS "Native arch: AVX2 (MSVC)")
        else()
            message(STATUS "Native arch: baseline (MSVC, no AVX2/AVX512 detected)")
        endif()

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # GCC/Clang: -march=native works on both Linux and macOS
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)

        if(HAS_MARCH_NATIVE)
            set(HUIRA_ARCH_C_FLAGS "-march=native -mtune=native")
            set(HUIRA_ARCH_CXX_FLAGS "-march=native -mtune=native")
            message(STATUS "Native arch: -march=native -mtune=native")
        else()
            message(STATUS "Native arch: -march=native not supported by compiler")
        endif()
    endif()

    # Apply to this project's targets
    if(HUIRA_ARCH_CXX_FLAGS)
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C>:${HUIRA_ARCH_C_FLAGS}>
            $<$<COMPILE_LANGUAGE:CXX>:${HUIRA_ARCH_CXX_FLAGS}>
        )
    endif()
else()
    message(STATUS "Native arch optimizations disabled (HUIRA_NATIVE_ARCH=OFF)")
endif()


#####################################
### Platform / Build Type Config  ###
#####################################
if(MSVC)
    message(STATUS "Platform: Windows (MSVC)")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

elseif(UNIX AND NOT APPLE)
    message(STATUS "Platform: Linux")

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

        # Fast-math subset: relaxed FP without full -ffast-math 
        # (avoids -ffinite-math-only which can miscompile NaN checks)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 \
-fno-math-errno -fno-signed-zeros -fno-trapping-math \
-freciprocal-math -fno-rounding-math -fno-signaling-nans \
-fexcess-precision=fast")
        # Note: LTO is handled by CMAKE_INTERPROCEDURAL_OPTIMIZATION above.
        # No need for explicit -flto=auto in flags.
    endif()

    # WSL detection
    execute_process(
        COMMAND grep -i microsoft /proc/version
        RESULT_VARIABLE IS_WSL
        OUTPUT_QUIET ERROR_QUIET
    )
    if(IS_WSL EQUAL 0)
        message(STATUS "WSL Detected")
        add_definitions(-D__WSL__)
    endif()

elseif(APPLE)
    message(STATUS "Platform: Apple")

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 \
-fno-math-errno -fno-signed-zeros -fno-trapping-math \
-freciprocal-math -fno-rounding-math -fno-signaling-nans \
-fexcess-precision=fast")
    endif()

    include(CheckLinkerFlag)
    check_linker_flag(CXX "-Wl,-no_warn_duplicate_libraries" LINKER_SUPPORTS_NO_WARN_DUPLICATE_LIBRARIES)
    if(LINKER_SUPPORTS_NO_WARN_DUPLICATE_LIBRARIES)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no_warn_duplicate_libraries")
    endif()
endif()


#####################################
### Enable Comprehensive Warnings ###
#####################################
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        /Wall
        /we4715 # Not all control paths return a value (treat as error)
        /we4700 # Uninitialized variable used (treat as error)
        /w14263 # Virtual function override without 'override' keyword
        /w14265 # Class has virtual functions but destructor is not virtual
        /w14287 # Unsigned/negative constant mismatch
        /w14289 # Loop control variable used outside loop
        /w14296 # Expression is always true/false
        /w14311 # Pointer truncation from type to type
        /w14545 # Expression before comma has no effect
        /w14546 # Function call before comma missing argument list
        /w14547 # Operator before comma has no effect
        /w14549 # Operator before comma has no effect
        /w14555 # Expression has no effect
        /w14619 # Pragma warning: there is no warning number
        /w14640 # Thread unsafe static member initialization
        /w14826 # Conversion is sign-extended
        /w14905 # Wide string literal cast to LPSTR
        /w14906 # String literal cast to LPWSTR
        /w14928 # Illegal copy-initialization
    )

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Weverything)

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wcast-align -Wcast-qual
        -Wconversion -Wsign-conversion
        -Wctor-dtor-privacy
        -Wdisabled-optimization
        -Wformat=2
        -Winit-self
        -Wlogical-op
        -Wmissing-declarations
        -Wmissing-include-dirs
        -Wnoexcept
        -Wold-style-cast
        -Woverloaded-virtual
        -Wredundant-decls
        -Wshadow
        -Wsign-promo
        -Wstrict-null-sentinel
        -Wstrict-overflow=5
        -Wswitch-default
        -Wundef
        -Wunused
        -Wuninitialized
        -Wunreachable-code
        -Wduplicated-cond
        -Wduplicated-branches
        -Wrestrict
        -Wnull-dereference
        -Wimplicit-fallthrough
        -Wsuggest-override
        -Wsuggest-final-types
        -Wsuggest-final-methods
        -Wextra-semi
    )

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.0")
        add_compile_options(-Walloc-zero -Walloca)
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0")
        add_compile_options(-Wcast-align=strict)
    endif()

else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()


#################################
### Disable Specific Warnings ###
#################################
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        /wd4514 /wd4710 /wd4711 /wd4820 /wd5045 /wd4371 /wd5246 /wd4868
        /wd4996
    )

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-c++11-compat -Wno-c++14-compat
        -Wno-c++17-compat -Wno-c++20-compat
        -Wno-padded -Wno-weak-vtables
        -Wno-exit-time-destructors -Wno-global-constructors
        -Wno-switch-enum -Wno-covered-switch-default
        -Wno-documentation -Wno-documentation-unknown-command
        -Wno-missing-prototypes -Wno-newline-eof
        -Wno-reserved-id-macro -Wno-disabled-macro-expansion
        -Wno-unsafe-buffer-usage
        -Wno-double-promotion -Wno-float-equal
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-zero-length-array -Wno-gnu-statement-expression
        -Wno-gnu-conditional-omitted-operand -Wno-gnu-empty-initializer
        -Wno-poison-system-directories
    )

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wno-double-promotion -Wno-float-equal
        -Wno-inline -Wno-system-headers -Wno-noexcept
        -Wno-c++11-compat -Wno-c++14-compat
        -Wno-c++17-compat -Wno-c++20-compat
    )

    include(CheckCXXCompilerFlag)
    set(OPTIONAL_DISABLE_WARNINGS "suggest-final-types" "suggest-final-methods")
    foreach(WARNING ${OPTIONAL_DISABLE_WARNINGS})
        check_cxx_compiler_flag("-W${WARNING}" HAS_WARNING_${WARNING})
    endforeach()
endif()


##########################################
### Per-target warning suppression     ###
##########################################
function(suppress_warnings_for_target target)
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PRIVATE /W1)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endfunction()
