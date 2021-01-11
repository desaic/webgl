cmake_minimum_required(VERSION 2.8)

PROJECT(GuiLiteTest)
include_directories(include)

FILE(GLOB EXE_SRC "src/*.cpp" "src/*.c" "resource/*.rc")
set(CMAKE_CXX_STANDARD 17)
add_executable(GuiLiteTest ${EXE_SRC} )