set(HUIRA_EXAMPLE_NAMES
    
)

foreach(example IN LISTS HUIRA_EXAMPLE_NAMES)
    string(REGEX REPLACE "^[0-9]+_" "" source_name "${example}")
    add_executable(${example} ${example}/${source_name}.cpp)
    target_link_libraries(${example} PRIVATE huira)
endforeach()
