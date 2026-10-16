set(HUIRA_EXAMPLE_NAMES
    01_first_render
    02_scene_graph
    03_load_a_model
    04_truth_data
    05_planetary_brdfs
    06_materials_and_textures
    07_clouds_and_atmosphere
)

foreach(example IN LISTS HUIRA_EXAMPLE_NAMES)
    string(REGEX REPLACE "^[0-9]+_" "" source_name "${example}")
    add_executable(${example} ${example}/${source_name}.cpp)
    target_link_libraries(${example} PRIVATE huira)
endforeach()
