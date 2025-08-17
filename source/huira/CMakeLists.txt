# Include FetchContent module
include(FetchContent)

# Suppress deprecation warnings from third-party libraries
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# Create the Huira interface library:
add_library(huira INTERFACE)


if(UNIX AND NOT APPLE)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
        target_link_libraries(huira INTERFACE stdc++fs)
    endif()
endif()


#######################
### Install Targets ###
#######################
install(TARGETS huira
    EXPORT HuiraTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Define all headers and implementation files for installation:
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING 
        PATTERN "*.h" 
        PATTERN "*.hpp"
        PATTERN "*.ipp"
)

# Export the targets
install(EXPORT HuiraTargets
    FILE HuiraTargets.cmake
    NAMESPACE huira::
    DESTINATION ${HUIRA_INSTALL_CMAKEDIR}
)