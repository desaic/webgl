cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)
file(GLOB_RECURSE SRC "src/*.cpp" "inc/*.h")
add_compile_options("/std:c++latest")
include_directories("inc")
include_directories("../Math/inc")
include_directories("../cdt/")
add_library("Mesh" STATIC ${SRC})
