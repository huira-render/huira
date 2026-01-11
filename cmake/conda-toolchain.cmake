# Cross-platform conda toolchain

# Disable vcpkg since we're using conda:
set(VCPKG_ENABLE_MANIFESTS OFF)
set(CMAKE_VS_GLOBALS "VcpkgEnableManifest=false")

set(CONDA_PREFIX $ENV{CONDA_PREFIX})

if(NOT CONDA_PREFIX)
    message(FATAL_ERROR "CONDA_PREFIX environment variable not set. Make sure conda environment is activated.")
endif()

# Set the install prefix to the conda environment by default:
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CONDA_PREFIX}" CACHE PATH "Install prefix defaulted to conda environment" FORCE)
    message(STATUS "Install prefix set to conda environment: ${CMAKE_INSTALL_PREFIX}")
endif()

message(STATUS "Using Conda For Dependencies")

# Compiler detection: prefer conda compilers if available, fallback to system
if(UNIX AND NOT APPLE)
    # Linux: Check for conda GCC compilers first
    set(CONDA_GCC "${CONDA_PREFIX}/bin/x86_64-conda-linux-gnu-gcc")
    set(CONDA_GXX "${CONDA_PREFIX}/bin/x86_64-conda-linux-gnu-g++")
    
    if(EXISTS "${CONDA_GCC}" AND EXISTS "${CONDA_GXX}")
        message(STATUS "Using Conda Instaleld Compilers: ${CONDA_GCC}, ${CONDA_GXX}")
        set(CMAKE_C_COMPILER "${CONDA_GCC}")
        set(CMAKE_CXX_COMPILER "${CONDA_GXX}")

        # Clear conda's default cross-compilation flags
        set(CMAKE_EXE_LINKER_FLAGS_INIT "")
        set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
        set(CMAKE_MODULE_LINKER_FLAGS_INIT "")
    else()
        message(STATUS "Using System Compilers")
        find_program(SYSTEM_CC NAMES gcc clang cc)
        find_program(SYSTEM_CXX NAMES g++ clang++ c++)
        
        if(SYSTEM_CC AND SYSTEM_CXX)
            set(CMAKE_C_COMPILER "${SYSTEM_CC}")
            set(CMAKE_CXX_COMPILER "${SYSTEM_CXX}")
            message(STATUS " - Found System Compilers: ${SYSTEM_CC}, ${SYSTEM_CXX}")
        else()
            message(WARNING "No Compilers Found.  Install with Conda, or install build-essential or similar.")
        endif()
    endif()
    
    # Always set conda library paths for linking
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath,${CONDA_PREFIX}/lib -L${CONDA_PREFIX}/lib")
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-rpath,${CONDA_PREFIX}/lib -L${CONDA_PREFIX}/lib")
    set(CMAKE_MODULE_LINKER_FLAGS "-Wl,-rpath,${CONDA_PREFIX}/lib -L${CONDA_PREFIX}/lib")
    
elseif(APPLE)
    # macOS: Detect architecture and check for conda clang compilers
    execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    
    set(CONDA_CLANG "")
    set(CONDA_CLANGXX "")
    
    # Configure Apple Silicon vs x86
    if(ARCH STREQUAL "arm64")
        set(CONDA_CLANG "${CONDA_PREFIX}/bin/arm64-apple-darwin20.0.0-clang")
        set(CONDA_CLANGXX "${CONDA_PREFIX}/bin/arm64-apple-darwin20.0.0-clang++")
    else()
        set(CONDA_CLANG "${CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-clang")
        set(CONDA_CLANGXX "${CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-clang++")
    endif()
    
    # Check if architecture-specific conda compilers exist
    if(EXISTS "${CONDA_CLANG}" AND EXISTS "${CONDA_CLANGXX}")
        message(STATUS "Using Conda Installed Compilers: ${CONDA_CLANG}, ${CONDA_CLANGXX}")
        set(CMAKE_C_COMPILER "${CONDA_CLANG}")
        set(CMAKE_CXX_COMPILER "${CONDA_CLANGXX}")
    else()
        set(CONDA_CLANG "${CONDA_PREFIX}/bin/clang")
        set(CONDA_CLANGXX "${CONDA_PREFIX}/bin/clang++")
        
        message(STATUS "Using System Compilers")
            # Explicitly find and set system compilers to override conda's environment variables
            find_program(SYSTEM_CC NAMES clang gcc cc)
            find_program(SYSTEM_CXX NAMES clang++ g++ c++)
            
            if(SYSTEM_CC AND SYSTEM_CXX)
                set(CMAKE_C_COMPILER "${SYSTEM_CC}")
                set(CMAKE_CXX_COMPILER "${SYSTEM_CXX}")
                message(STATUS " - Found System Compilers: ${SYSTEM_CC}, ${SYSTEM_CXX}")
            else()
                message(WARNING "No Compilers Found.  Install with Conda, or install Xcode command line tools.")
            endif()
    endif()
endif()

# Add conda paths to CMake search paths
list(APPEND CMAKE_PREFIX_PATH ${CONDA_PREFIX})
list(APPEND CMAKE_LIBRARY_PATH ${CONDA_PREFIX}/lib)
list(APPEND CMAKE_INCLUDE_PATH ${CONDA_PREFIX}/include)

# Windows-specific paths (conda uses Library subdirectory on Windows)
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH ${CONDA_PREFIX}/Library)
    list(APPEND CMAKE_LIBRARY_PATH ${CONDA_PREFIX}/Library/lib)
    list(APPEND CMAKE_INCLUDE_PATH ${CONDA_PREFIX}/Library/include)
    list(APPEND CMAKE_PROGRAM_PATH ${CONDA_PREFIX}/Library/bin)
endif()

# Set specific package directories for packages that do provide CMake configs
set(TBB_DIR ${CONDA_PREFIX}/lib/cmake/TBB)
set(FFTW3_DIR ${CONDA_PREFIX}/lib/cmake/fftw3)
set(FFTW3f_DIR ${CONDA_PREFIX}/lib/cmake/fftw3)
set(FFTW3l_DIR ${CONDA_PREFIX}/lib/cmake/fftw3)

# Windows uses different paths for cmake files
if(WIN32)
    set(TBB_DIR ${CONDA_PREFIX}/Library/lib/cmake/TBB)
    set(FFTW3_DIR ${CONDA_PREFIX}/Library/lib/cmake/fftw3)
    set(FFTW3f_DIR ${CONDA_PREFIX}/Library/lib/cmake/fftw3)
    set(FFTW3l_DIR ${CONDA_PREFIX}/Library/lib/cmake/fftw3)
endif()

# Set pkg-config path for conda packages
if(UNIX)
    set(ENV{PKG_CONFIG_PATH} "${CONDA_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
elseif(WIN32)
    set(ENV{PKG_CONFIG_PATH} "${CONDA_PREFIX}/Library/lib/pkgconfig;$ENV{PKG_CONFIG_PATH}")
endif()

message(STATUS "Using conda environment: ${CONDA_PREFIX}")
message(STATUS "Conda paths added to CMAKE_PREFIX_PATH for package discovery")
