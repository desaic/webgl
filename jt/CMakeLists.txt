cmake_minimum_required(VERSION 3.16)
set(BIN "load_jt")
PROJECT (LoadJT)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type" FORCE)
endif()
FILE(GLOB EXE_SRC "*.cpp" "*.c")
set(CMAKE_CXX_STANDARD 20)
include_directories(".")

#LZMA
set(LZMA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/liblzma")
add_subdirectory("../Math" "Math")
add_subdirectory("../Mesh" "Mesh")

include_directories("../Math/inc/")
include_directories("../Mesh/inc")

add_executable(${BIN} ${EXE_SRC})

target_compile_definitions(${BIN} PRIVATE 
    $<$<CONFIG:Debug>:DEBUG_MODE>
    $<$<CONFIG:Release>:RELEASE_MODE>
)

#LZMA
target_include_directories(${BIN} PRIVATE "${LZMA_ROOT}/include")
target_link_directories(${BIN} PRIVATE "${LZMA_ROOT}/lib")

target_link_libraries(${BIN} PRIVATE 
    $<$<CONFIG:Debug>:lzma_d> 
    $<$<CONFIG:Release>:lzma>
    Mesh 
    MathLib
)
