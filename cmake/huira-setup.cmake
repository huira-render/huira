include(GNUInstallDirs)

add_library(huira INTERFACE)
add_library(huira::huira ALIAS huira)

target_include_directories(huira
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(huira INTERFACE cxx_std_20)


set(HUIRA_DATA_DIR_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(HUIRA_DATA_DIR_INSTALL "${CMAKE_INSTALL_FULL_DATADIR}/huira/data")

target_compile_definitions(huira INTERFACE
    $<BUILD_INTERFACE:HUIRA_DEFAULT_DATA_DIR="${HUIRA_DATA_DIR_BUILD}">
    $<INSTALL_INTERFACE:HUIRA_DEFAULT_DATA_DIR="${HUIRA_DATA_DIR_INSTALL}">
)


find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    find_path(GLM_INCLUDE_DIR glm/glm.hpp REQUIRED)
    add_library(glm::glm INTERFACE IMPORTED)
    target_include_directories(glm::glm INTERFACE ${GLM_INCLUDE_DIR})
endif()

find_package(CSPICE REQUIRED)

find_package(assimp CONFIG REQUIRED)

find_package(TBB CONFIG REQUIRED)

find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(CFITSIO REQUIRED)
# Conda-forge's cfitsio includes 'm' in its link interface, which doesn't
# exist on Windows (math functions are in the CRT). Remove it.
if(WIN32 AND TARGET CFITSIO::CFITSIO)
    get_target_property(_cfitsio_libs CFITSIO::CFITSIO INTERFACE_LINK_LIBRARIES)
    if(_cfitsio_libs)
        list(REMOVE_ITEM _cfitsio_libs "m")
        set_target_properties(CFITSIO::CFITSIO PROPERTIES INTERFACE_LINK_LIBRARIES "${_cfitsio_libs}")
    endif()
endif()

target_link_libraries(huira INTERFACE
    glm::glm
    CSPICE::cspice
    assimp::assimp
    TBB::tbb
    TBB::tbbmalloc
    CFITSIO::CFITSIO
    libjpeg-turbo::turbojpeg
    PNG::PNG
)
