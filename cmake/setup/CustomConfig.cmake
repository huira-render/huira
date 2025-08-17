if(MSVC)
    message("Windows Detected")

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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


# Enable Broad Warnings:
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # MSVC comprehensive warnings
    target_compile_options(huira INTERFACE 
        /Wall 
        /w14263 # Enable warning for missing override
        /w14242 # Enable warning for extra semicolons
    )
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang comprehensive warnings
    target_compile_options(huira INTERFACE
        -Wall -Wextra
        -Wconversion -Wsign-conversion -Wcast-qual -Wcast-align
        -Wunused -Wuninitialized -Winit-self
        -Wnull-dereference -Wformat=2
        -Wimplicit-fallthrough
        -Winconsistent-missing-override
    )
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC comprehensive warnings
    target_compile_options(huira INTERFACE 
        -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion -Wcast-qual -Wcast-align
        -Wunused -Wuninitialized -Winit-self -Wlogical-op
        -Wduplicated-cond -Wduplicated-branches -Wrestrict
        -Wnull-dereference -Wformat=2
        -Wimplicit-fallthrough
        -Wsuggest-override
    )
    
else()
    # Fallback for other compilers
    target_compile_options(huira INTERFACE -Wall -Wextra)
endif()


# Disable Specific Warnings:
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(huira INTERFACE 
        # Style/layout warnings that don't affect correctness (ALWAYS OFF):
        /wd5246 # brace initialization (style, not correctness)
        /wd4820 # padding warnings (layout, not correctness)
        /wd4710 # function not inlined (optimization, not correctness)
        /wd4711 # function selected for automatic inline expansion (optimization, not correctness)
        /wd4371 # class layout changes (compiler optimization)
        /wd5045 # Spectre mitigation (security feature, not code issue)
    )
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    include(CheckCXXCompilerFlag)
    
    # Style/layout warnings to disable (ALWAYS OFF)
    set(ALWAYS_DISABLED_WARNINGS
        "newline-eof"
        "double-promotion"
        "covered-switch-default"
        "float-equal"
        "disabled-macro-expansion"
        "unsafe-buffer-usage"
    )
    
    foreach(WARNING ${ALWAYS_DISABLED_WARNINGS})
        check_cxx_compiler_flag("-W${WARNING}" HAS_WARNING_${WARNING})
        if(HAS_WARNING_${WARNING})
            target_compile_options(huira INTERFACE "-Wno-${WARNING}")
        endif()
    endforeach()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(huira INTERFACE 
        -Wno-double-promotion
        -Wno-float-equal
    )
endif()



# Disable backwards compatibility warnings for C++20+ projects
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(huira INTERFACE 
        /wd4996  # Disable deprecation warnings for older C++ features
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    include(CheckCXXCompilerFlag)
    
    set(COMPAT_WARNINGS_TO_DISABLE
        "c++98-compat"
        "c++98-compat-pedantic"
        "c++98-compat-extra-semi"
        "c++98-compat-local-type-template-args"
        "c++11-compat"
        "c++14-compat"
        "c++17-compat"
        "c++20-compat"
        "c++23-compat"
        "pre-c++14-compat"
        "pre-c++17-compat"
        "pre-c++20-compat"
        "c99-extensions"
        "gnu-zero-variadic-macro-arguments"
        "zero-length-array"
        "gnu-statement-expression"
        "gnu-conditional-omitted-operand"
        "gnu-empty-initializer"
    )
    
    foreach(WARNING ${COMPAT_WARNINGS_TO_DISABLE})
        check_cxx_compiler_flag("-W${WARNING}" HAS_WARNING_${WARNING})
        if(HAS_WARNING_${WARNING})
            target_compile_options(huira INTERFACE "-Wno-${WARNING}")
        endif()
    endforeach()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    include(CheckCXXCompilerFlag)
    
    set(GCC_COMPAT_WARNINGS
        "c++11-compat"
        "c++14-compat"
        "c++17-compat"
        "c++20-compat"
    )
    
    foreach(WARNING ${GCC_COMPAT_WARNINGS})
        check_cxx_compiler_flag("-W${WARNING}" HAS_WARNING_${WARNING})
        if(HAS_WARNING_${WARNING})
            target_compile_options(huira INTERFACE "-Wno-${WARNING}")
        endif()
    endforeach()
endif()