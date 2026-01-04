find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    find_path(GLM_INCLUDE_DIR glm/glm.hpp REQUIRED)
    add_library(glm::glm INTERFACE IMPORTED)
    target_include_directories(glm::glm INTERFACE ${GLM_INCLUDE_DIR})
endif()

#############################
### Library Configuration ###
#############################

add_library(huira INTERFACE)
add_library(huira::huira ALIAS huira)

target_include_directories(huira
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(huira INTERFACE cxx_std_20)

target_link_libraries(huira INTERFACE glm::glm)

##########################
### Installation Setup ###
##########################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/huira/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/huira)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/huira_impl/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/huira_impl)

# Install the library target
install(TARGETS huira
        EXPORT HuiraTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install the export set
install(EXPORT HuiraTargets
        FILE HuiraTargets.cmake
        NAMESPACE huira::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/huira)

# Generate and install the config file
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/HuiraConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/HuiraConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/huira
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
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/huira)