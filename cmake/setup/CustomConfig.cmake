if(MSVC)
    message("Windows Detected")

    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_Release ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_Debug OFF)

elseif(UNIX AND NOT APPLE)
    message("Linux Detected")

    # Check for the build type:
    if (NOT (CMAKE_BUILD_TYPE STREQUAL "Release"))
        set(CMAKE_BUILD_TYPE "Debug")
    endif()
    
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

	elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    	set(CMAKE_CXX_FLAGS "-Wall -mtune=native -march=native")
		set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-math-errno -fno-signed-zeros -fno-trapping-math -freciprocal-math -fno-rounding-math -fno-signaling-nans -fexcess-precision=fast -flto=auto")
	endif()

    execute_process(
        COMMAND grep -i microsoft /proc/version
        RESULT_VARIABLE IS_WSL
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(IS_WSL EQUAL 0)
        message("WSL Detected")
        add_definitions(-D__WSL__)
    endif()

elseif(APPLE)
    message("Apple Detected")

    # Check if the linker supports -no_warn_duplicate_libraries
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
    # Clang comprehensive warnings - nearly everything
    add_compile_options(-Weverything)  # Enable ALL warnings, then disable specific ones
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC comprehensive warnings - maximum practical level
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
        # Add extra semicolon detection
        -Wextra-semi
    )
    
    # Check for GCC version-specific warnings
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.0")
        add_compile_options(
            -Walloc-zero
            -Walloca
        )
    endif()
    
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0")
        add_compile_options(-Wcast-align=strict)
    endif()
    
else()
    # Fallback for other compilers
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()


#################################
### Disable Specific Warnings ###
#################################
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        # Style/layout warnings that don't affect correctness
        /wd4514 # Unreferenced inline function has been removed
        /wd4710 # Function not inlined
        /wd4711 # Function selected for automatic inline expansion
        /wd4820 # Bytes padding added after data member
        /wd5045 # Compiler will insert Spectre mitigation
        /wd4371 # Layout of class may have changed from previous compiler version
        /wd5246 # The initialization of a subobject should be wrapped in braces
        /wd4868 # Left-to-right evaluation order of braced initializer list
        
        # Compatibility warnings
        /wd4996 # Function or variable may be unsafe (deprecation warnings)
    )
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Disable style/pedantic warnings that don't affect correctness
    add_compile_options(
        # C++ compatibility warnings
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-c++11-compat
        -Wno-c++14-compat
        -Wno-c++17-compat
        -Wno-c++20-compat
        
        # Style warnings
        -Wno-padded
        -Wno-weak-vtables
        -Wno-exit-time-destructors
        -Wno-global-constructors
        -Wno-switch-enum
        -Wno-covered-switch-default
        -Wno-documentation
        -Wno-documentation-unknown-command
        -Wno-missing-prototypes
        -Wno-newline-eof
        -Wno-reserved-id-macro
        -Wno-disabled-macro-expansion
        -Wno-unsafe-buffer-usage
        
        # Floating point warnings that are often false positives
        -Wno-double-promotion
        -Wno-float-equal
        
        # GNU extension warnings
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-zero-length-array
        -Wno-gnu-statement-expression
        -Wno-gnu-conditional-omitted-operand
        -Wno-gnu-empty-initializer
    )
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        # Disable warnings that are often false positives or too pedantic
        -Wno-double-promotion
        -Wno-float-equal
        -Wno-inline  # Disable inline warnings
        -Wno-system-headers  # Don't warn about system headers
        
        # Compatibility warnings for modern C++
        -Wno-c++11-compat
        -Wno-c++14-compat
        -Wno-c++17-compat
        -Wno-c++20-compat
    )
    
    # Conditionally disable warnings that may not exist in older GCC versions
    include(CheckCXXCompilerFlag)
    
    set(OPTIONAL_DISABLE_WARNINGS
        "suggest-final-types"
        "suggest-final-methods"
    )
    
    foreach(WARNING ${OPTIONAL_DISABLE_WARNINGS})
        check_cxx_compiler_flag("-W${WARNING}" HAS_WARNING_${WARNING})
        if(HAS_WARNING_${WARNING})
            # Only disable if you don't want these suggestions
            # add_compile_options("-Wno-${WARNING}")
        endif()
    endforeach()
endif()

# Function to add target-specific warning suppressions
function(suppress_warnings_for_target target)
    # Usage: suppress_warnings_for_target(my_target)
    # This can be used to suppress warnings for specific third-party libraries
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PRIVATE /W1)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endfunction()