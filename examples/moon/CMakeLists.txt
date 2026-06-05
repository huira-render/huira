add_executable(moon main.cpp)
target_link_libraries(moon PRIVATE huira)

find_package(GDAL REQUIRED)
find_library(LZ4_LIBRARY NAMES lz4 REQUIRED)
target_link_libraries(moon PRIVATE GDAL::GDAL ${LZ4_LIBRARY})
