cmake_minimum_required(VERSION 3.16)

#########################################
### Define Options and Initial Values ###
#########################################
option(HUIRA_EXAMPLES "Build Examples" OFF)
option(HUIRA_TESTS "Build Tests" OFF)
option(HUIRA_DOCS "Build Documentation" OFF)

option(HUIRA_BUILD_DEV "Build Development Programs" ON)

option(HUIRA_PREBUILD_SPECIALIZATIONS "Build Common Specializations" OFF)

# Load modules from cmake/ directory
list(APPEND CMAKE_MODULE_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/find"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/setup"
)
include(CustomVcpkgSetup)

#############################
### Configure the project ###
#############################
project(Huira LANGUAGES CXX VERSION 0.8.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Huira version ${PROJECT_VERSION}")
add_compile_definitions(HUIRA_VERSION="${PROJECT_VERSION}")

# Ensure if Debug definitions:
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(NDEBUG)
endif()

if(NOT HUIRA_PREBUILD_SPECIALIZATIONS)
    add_compile_definitions(HUIRA_HEADER_ONLY)
endif()

# Configure compiler and warnings:
include(CustomConfig)

# Set all outputs to go to the binary directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


##########################
### Add Subdirectories ###
##########################
# Add the included directories:
include_directories(include)

# Add source directories:
add_subdirectory(src/huira)


#####################################################
### Add additional subdirs based on configuration ###
#####################################################
if(HUIRA_EXAMPLES)
    message("Building Examples")
    add_subdirectory(examples/)
endif()

if(HUIRA_TESTS)
    message("Building Tests")
    enable_testing()
    add_subdirectory(tests)
endif()

if(HUIRA_DOCS)
    message("Building Documentation")
    add_subdirectory(docs)
endif()

# Include all included Scratch directories:
if (HUIRA_BUILD_DEV)
    message("Building Development Code")
    file(GLOB subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/dev/ ${CMAKE_CURRENT_SOURCE_DIR}/dev/*/)
    foreach(subdir ${subdirs})
        if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dev/${subdir})
            message(" - Building:  dev/${subdir}")
            add_subdirectory(dev/${subdir})
        endif()
    endforeach()
endif()