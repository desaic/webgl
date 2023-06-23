PROJECT (SparseSDF)
cmake_minimum_required(VERSION 3.0.0)

FILE(GLOB EXE_SRC "*.cpp" "*.c" )
add_executable(SparseSDF ${EXE_SRC})
set_property(TARGET SparseSDF PROPERTY CXX_STANDARD 17)
