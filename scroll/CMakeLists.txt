cmake_minimum_required(VERSION 3.20)
PROJECT (scroll)
file(GLOB_RECURSE SRC "*.h" "*.cpp")
set(BIN scroll)
include_directories("./libtiff/include/")
include_directories("./libtiff/zlib/include/")
link_directories("./libtiff/lib/")
link_directories("./libtiff/zlib/lib/")

add_executable(${BIN} ${SRC})
set_target_properties(${BIN} PROPERTIES
  EXCLUDE_FROM_ALL TRUE
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED TRUE
)

target_link_libraries( ${BIN} debug tiffd zlibstaticd)
target_link_libraries( ${BIN} optimized tiff zlibstatic)

