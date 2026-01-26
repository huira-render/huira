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

include(huiraFindCSPICE)
huira_find_cspice()

find_package(assimp CONFIG REQUIRED)

find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

target_link_libraries(huira INTERFACE
    glm::glm
    cspice
    assimp::assimp
    JPEG::JPEG
    PNG::PNG
)
