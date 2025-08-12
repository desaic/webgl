cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(BIN "winding")
PROJECT (Winding)
include_directories("./")

FILE(GLOB EXE_SRC "*.cpp" "*.c")
add_executable(${BIN} ${EXE_SRC})