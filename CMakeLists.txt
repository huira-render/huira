cmake_minimum_required(VERSION 3.16)

#########################################
### Define Options and Initial Values ###
#########################################
option(HUIRA_EXAMPLES "Build Examples" OFF)
option(HUIRA_TESTS "Build Tests" OFF)
option(HUIRA_DOCS "Build Documentation" OFF)
option(HUIRA_PYTHON "Build Python Bindings" OFF)
option(HUIRA_LOCAL_DEV "Build Local Development Directory" OFF)

# Load modules from cmake/ directory
list(APPEND CMAKE_MODULE_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/find"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/setup"
)
include(CustomVcpkgSetup)

#############################
### Configure the project ###
#############################
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HUIRA_VERSION)
string(STRIP "${HUIRA_VERSION}" HUIRA_VERSION)
project(huira LANGUAGES CXX VERSION ${HUIRA_VERSION})

message(STATUS "huira version ${PROJECT_VERSION}")
add_compile_definitions(HUIRA_VERSION="${PROJECT_VERSION}")

# Configure compiler and warnings:
include(CustomConfig)

# Set all outputs to go to the binary directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

##########################
### Define the library ###
##########################
include(huira)

#####################################################
### Add additional subdirs based on configuration ###
#####################################################
if(HUIRA_EXAMPLES)
    message(STATUS "Building Examples")
    add_subdirectory(examples)
endif()

if(HUIRA_TESTS)
    message(STATUS "Building Tests")
    enable_testing()
    add_subdirectory(tests)
endif()

if(HUIRA_DOCS)
    message(STATUS "Building Documentation")
    add_subdirectory(docs)
endif()

if(HUIRA_PYTHON)
    message(STATUS "Building Python Bindings")
    add_subdirectory(bindings/python)
endif()

if(HUIRA_LOCAL_DEV)
    message(STATUS "Building Local Development Directory")

    file(GLOB subdirs RELATIVE
        "${CMAKE_CURRENT_SOURCE_DIR}/local/"
        "${CMAKE_CURRENT_SOURCE_DIR}/local/*/"
    )

    foreach(subdir ${subdirs})
        if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/local/${subdir})
            message(STATUS " - Building: local/${subdir}")
            add_subdirectory(local/${subdir})
        endif()
    endforeach()
endif()
