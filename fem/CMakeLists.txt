cmake_minimum_required(VERSION 3.16)
set(BIN "FemUI")
PROJECT (FemUI)
add_subdirectory("../UILib" "ui")
include_directories(.)
FILE(GLOB EXE_SRC "*.cpp" "*.c")
set(CMAKE_CXX_STANDARD 17)
include_directories("../UILib/inc/")
link_directories("../UILib/extern/glfw3.3.8")
link_directories("../UILib/extern/glew-2.1.0")

option(USE_NANOVDB "test openvdb" OFF)
if (USE_NANOVDB)
  set(NANOVDB_INC_DIR "F:/github/openvdb/nanovdb/")
  include_directories(${NANOVDB_INC_DIR})
  add_compile_definitions(USE_NANOVDB)
endif()

add_executable(${BIN} ${EXE_SRC} resource/icons.rc)
target_link_libraries(${BIN} UILib imgui glfw3 opengl32 glew32s)

set(FONT_FOLDER fonts)
add_custom_command(TARGET ${BIN} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/${FONT_FOLDER}" "${CMAKE_BINARY_DIR}/${FONT_FOLDER}")

install(DIRECTORY ${FONT_FOLDER} TYPE BIN) # to install dir

add_custom_command(
    TARGET ${BIN} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy_directory "${CMAKE_SOURCE_DIR}/resource/" "${CMAKE_BINARY_DIR}/resource"
    COMMENT "Copying resource folder to output directory"
)
add_custom_command(
    TARGET ${BIN} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy_directory "${CMAKE_SOURCE_DIR}/glsl/" "${CMAKE_BINARY_DIR}/glsl"
    COMMENT "Copying glsl folder to output directory"
)