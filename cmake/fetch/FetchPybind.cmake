if(DEFINED Python_EXECUTABLE)
    get_filename_component(Python_ROOT_DIR "${Python_EXECUTABLE}" DIRECTORY)
    get_filename_component(Python_ROOT_DIR "${Python_ROOT_DIR}" DIRECTORY)
endif()

set(PYBIND11_FINDPYTHON ON)
set(HUIRA_PYBIND11_MIN_VERSION 2.12)
set(HUIRA_PYBIND11_FETCH_TAG   v2.13.6)

if(HUIRA_USE_SYSTEM_PYBIND11)
    find_package(pybind11 ${HUIRA_PYBIND11_MIN_VERSION} CONFIG REQUIRED)
    message(STATUS "Using external pybind11 ${pybind11_VERSION}")
else()
    # FetchContent is used because vcpkg will attempt to install python, rather than linking to system version
    include(FetchContent)
    FetchContent_Declare(
      pybind11
      GIT_REPOSITORY https://github.com/pybind/pybind11.git
      GIT_TAG        ${HUIRA_PYBIND11_FETCH_TAG}
    )
    FetchContent_MakeAvailable(pybind11)
    message(STATUS "Using fetched pybind11 ${HUIRA_PYBIND11_FETCH_TAG}")
endif()
