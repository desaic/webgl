cmake_minimum_required(VERSION 3.16)
set(BIN "load_jt")
PROJECT (LoadJT)
FILE(GLOB EXE_SRC "*.cpp" "*.c")
set(CMAKE_CXX_STANDARD 20)
include_directories(".")

#LZMA
set(LZMA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/liblzma")

add_executable(${BIN} ${EXE_SRC})

#LZMA
target_include_directories(${BIN} PRIVATE "${LZMA_ROOT}/include")
target_link_directories(${BIN} PRIVATE "${LZMA_ROOT}/lib")

target_link_libraries(${BIN} PRIVATE lzma)
