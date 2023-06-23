PROJECT (vol2mesh)
cmake_minimum_required(VERSION 3.0.0)

FILE(GLOB EXE_SRC "*.cpp" "*.c" )
add_executable(vol2mesh ${EXE_SRC})
set_property(TARGET vol2mesh PROPERTY CXX_STANDARD 17)
