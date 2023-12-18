cmake_minimum_required(VERSION 3.16)
PROJECT (Water)
set(BIN "water")
add_subdirectory("../UILib" "ui")
include_directories(.)
FILE(GLOB EXE_SRC "*.cpp" "*.c")
set(CMAKE_CXX_STANDARD 17)
include_directories("../UILib/inc/")
link_directories("../UILib/extern/glfw3.3.8")
link_directories("../UILib/extern/glew-2.1.0")
add_executable(${BIN} ${EXE_SRC} resource/icons.rc )
target_link_libraries(${BIN} UILib imgui glfw3 opengl32 glew32s ws2_32.lib)
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