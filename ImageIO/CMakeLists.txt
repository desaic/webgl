cmake_minimum_required(VERSION 3.0)
project(ImageIO)
set(CMAKE_CXX_STANDARD 20)
file(GLOB_RECURSE SRC "*.cpp" "*.h")
add_compile_options("/std:c++latest")
include_directories("../Math/inc")
include_directories("./")
add_library(ImageIO STATIC ${SRC})