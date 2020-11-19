PROJECT (MeshProc)

include_directories(include)
FILE(GLOB EXE_SRC "src/*.cpp" "src/*.c")
set(CMAKE_CXX_STANDARD 17)
add_executable(meshproc ${EXE_SRC} )
target_link_libraries(meshproc ws2_32.lib)
