PROJECT (ScanVis)
cmake_minimum_required(VERSION 2.8.11)

#OpenCV
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )
include_directories( ${PROJECT_SOURCE_DIR}/include )
include_directories( ${PROJECT_SOURCE_DIR}/sdf )
include_directories( ${PROJECT_SOURCE_DIR}/common )

FILE(GLOB EXE_SRC "src/*.cpp" "src/*.c" )
add_executable(scanvis ${EXE_SRC})
set_property(TARGET scanvis PROPERTY CXX_STANDARD 23)
target_link_libraries(scanvis ${OpenCV_LIBS} )
