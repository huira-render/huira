##########################
### Installation Setup ###
##########################
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/huira/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/huira)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/huira_impl/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/huira_impl)


# Install kernel data
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/huira/data/)

# Install the library target
install(TARGETS huira
        EXPORT huiraTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install the export set
install(EXPORT huiraTargets
        FILE huiraTargets.cmake
        NAMESPACE huira::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/huira)

# Generate and install the config file
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/huiraConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/huiraConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/huira
)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/huiraConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
        "${CMAKE_BINARY_DIR}/huiraConfig.cmake"
        "${CMAKE_BINARY_DIR}/huiraConfigVersion.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/find/FindCSPICE.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/huira)
