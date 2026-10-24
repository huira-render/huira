add_executable(build_cook_torrance_albedo main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(build_cook_torrance_albedo PRIVATE Threads::Threads)
target_compile_features(build_cook_torrance_albedo PRIVATE cxx_std_20)
set_target_properties(build_cook_torrance_albedo PROPERTIES CXX_SCAN_FOR_MODULES OFF)
# The lobe it integrates, cook_torrance_lobe.hpp, uses the standard library only.
target_include_directories(build_cook_torrance_albedo PRIVATE ${PROJECT_SOURCE_DIR}/include)
# The tool writes its table with std::fopen, which the Windows C runtime deprecates in favor of its
# own fopen_s; it is standard C++, so silence that rather than change the tool (which the table's
# fingerprint covers).
if(WIN32)
    target_compile_definitions(build_cook_torrance_albedo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
