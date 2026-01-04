if(DEFINED Python_EXECUTABLE)
    get_filename_component(Python_ROOT_DIR "${Python_EXECUTABLE}" DIRECTORY)
    get_filename_component(Python_ROOT_DIR "${Python_ROOT_DIR}" DIRECTORY)
endif()

set(PYBIND11_FINDPYTHON ON)

# FetchContent is used because vcpkg will attempt to install python, rather than linking to system version
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.13.6
)
FetchContent_MakeAvailable(pybind11)