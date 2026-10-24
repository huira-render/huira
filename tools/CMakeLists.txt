# Developer tools: programs that build the data files huira publishes on media.huira.space, and
# tables that are part of its source. They are not installed and not part of any package; build
# them with -DHUIRA_BUILD_TOOLS=ON.
add_subdirectory(build_cook_torrance_albedo)
add_subdirectory(build_star_catalog)
