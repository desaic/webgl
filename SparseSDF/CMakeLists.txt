PROJECT (SparseSDF)
cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 20)
FILE(GLOB LIB_SRC "*.cpp" "*.c" EXCLUDE "*/main.cpp")
list(REMOVE_ITEM LIB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
add_library("SparseSDF" STATIC ${LIB_SRC})
#add_subdirectory("../Math" "Math")
include_directories("../Math/inc")
include_directories("../Mesh/inc")
include_directories("../ImageIO")
include_directories("../Util")
if (NOT TARGET Mesh)
  message(STATUS "add mesh lib")
  add_subdirectory("../Mesh" "Mesh")
endif()
if (NOT TARGET Util)
add_subdirectory("../Util/" "Util")
endif()
add_executable(sdf_test "main.cpp")
target_link_libraries(sdf_test SparseSDF Mesh MathLib Util)
