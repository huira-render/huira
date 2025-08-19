# Include FetchContent module
include(FetchContent)

# Suppress deprecation warnings from third-party libraries
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)


find_package(glm CONFIG REQUIRED)

#############################
### Library Configuration ###
#############################

# Collect all non-templated source files
set(HUIRA_SOURCES
    
)

# Collect templates with known/limited specializations
set(HUIRA_TEMPLATE_SOURCES
    math/rotation.cpp   
)

# Create the library target - always a regular library since we have .cpp files
if(HUIRA_PREBUILD_SPECIALIZATIONS)
    # Collect template specialization sources (only when prebuilding)
    set(HUIRA_SPECIALIZATION_SOURCES
        
    )

    # Create a compiled library with everything
    add_library(huira 
        ${HUIRA_SOURCES}
        ${HUIRA_TEMPLATE_SOURCES}
        ${HUIRA_SPECIALIZATION_SOURCES}
    )
else()
    # Create a library with core sources and limited-specialization templates
    # More complex templates remain header-only
    add_library(huira 
        ${HUIRA_SOURCES}
        ${HUIRA_TEMPLATE_SOURCES}
    )
    
    # Add the header-only definition for the remaining templates
    target_compile_definitions(huira PUBLIC HUIRA_HEADER_ONLY)
endif()

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(huira PUBLIC HUIRA_STATIC)
endif()

target_link_libraries(huira PUBLIC
    glm::glm
)

# Common configuration for both modes
target_include_directories(huira PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(huira PUBLIC cxx_std_20)

# Set properties for the library
set_target_properties(huira PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Huira
)

# Create an alias for consistent naming
add_library(huira::huira ALIAS huira)

# Configure instrumentation for code coverage
if(HUIRA_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(COVERAGE_FLAGS --coverage -fprofile-arcs -ftest-coverage -g)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(WIN32)
            set(COVERAGE_FLAGS -fprofile-instr-generate -fcoverage-mapping -g)
        else()
            set(COVERAGE_FLAGS --coverage -g)
        endif()
    endif()
    
    target_compile_options(huira PRIVATE ${COVERAGE_FLAGS})
    target_link_options(huira PRIVATE ${COVERAGE_FLAGS})
    
    message(STATUS "Coverage instrumentation added to huira library")
endif()

##########################
### Installation Setup ###
##########################
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers based on build mode
if(HUIRA_PREBUILD_SPECIALIZATIONS)
    # Only install public headers for precompiled mode
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/huira/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/huira
            FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/inline/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/inline
            FILES_MATCHING PATTERN "*.ipp")

else()
    # Install both public headers and implementation for header-only mode
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/huira/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/huira
            FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
    
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/inline/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/inline
            FILES_MATCHING PATTERN "*.ipp")

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/implementation/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/implementation
            FILES_MATCHING PATTERN "*.ipp" PATTERN "*.hpp" PATTERN "*.h")
endif()

# Install the library target
install(TARGETS huira
        EXPORT HuiraTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install the export set
install(EXPORT HuiraTargets
        FILE HuiraTargets.cmake
        NAMESPACE Huira::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Huira)

# Set up variables for the config file
set(HUIRA_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(HUIRA_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Huira)

# Generate and install the config file
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/HuiraConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/HuiraConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Huira
    PATH_VARS HUIRA_INSTALL_INCLUDEDIR HUIRA_INSTALL_CMAKEDIR
)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/HuiraConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
        "${CMAKE_BINARY_DIR}/HuiraConfig.cmake"
        "${CMAKE_BINARY_DIR}/HuiraConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Huira)

# TODO Install helper modules for downstream projects
#install(FILES
#        cmake/
#        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Huira)