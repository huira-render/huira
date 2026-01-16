# huiraFindCSPICE.cmake - CSPICE discovery for huira

function(huira_find_cspice)
    # Prevent multiple inclusions
    if(TARGET cspice)
        return()
    endif()

    # Try to find the library
    find_library(CSPICE_LIBRARY
        NAMES cspice
        HINTS
            ${CSPICE_ROOT}
            $ENV{CSPICE_ROOT}
            $ENV{CONDA_PREFIX}
            $ENV{PREFIX}
        PATH_SUFFIXES
            lib
            Library/lib
    )

    # Try to find the include directory
    find_path(CSPICE_INCLUDE_DIR
        NAMES SpiceUsr.h
        HINTS
            ${CSPICE_ROOT}
            $ENV{CSPICE_ROOT}
            $ENV{CONDA_PREFIX}
            $ENV{PREFIX}
        PATH_SUFFIXES
            include/cspice
            include
            Library/include/cspice
            Library/include
    )

    if(NOT CSPICE_LIBRARY OR NOT CSPICE_INCLUDE_DIR)
        message(FATAL_ERROR "CSPICE not found. Install via vcpkg or conda, or set CSPICE_ROOT.")
    endif()

    # Create imported target
    add_library(cspice UNKNOWN IMPORTED GLOBAL)
    
    # Handle include directory structure (same logic as before)
    if(EXISTS "${CSPICE_INCLUDE_DIR}/SpiceUsr.h")
        get_filename_component(CSPICE_PARENT_INCLUDE_DIR "${CSPICE_INCLUDE_DIR}" DIRECTORY)
        set(CSPICE_INTERFACE_INCLUDE "${CSPICE_PARENT_INCLUDE_DIR}")
    else()
        get_filename_component(CSPICE_INTERFACE_INCLUDE "${CSPICE_INCLUDE_DIR}" DIRECTORY)
    endif()
    
    set_target_properties(cspice PROPERTIES
        IMPORTED_LOCATION "${CSPICE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CSPICE_INTERFACE_INCLUDE}"
    )
    
    # Link math library on Unix
    if(UNIX)
        target_link_libraries(cspice INTERFACE m)
    endif()
    
    # Create alias for consistency
    add_library(CSPICE::cspice ALIAS cspice)
    
    message(STATUS "Found CSPICE: ${CSPICE_LIBRARY}")
    message(STATUS "CSPICE include dir: ${CSPICE_INTERFACE_INCLUDE}")
endfunction()
