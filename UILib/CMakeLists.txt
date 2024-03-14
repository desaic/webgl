cmake_minimum_required(VERSION 3.0)
project(UILib)
set(BIN ImguiExample)
set(CMAKE_CXX_STANDARD 20)
file(GLOB_RECURSE SRC "*.cpp" "*.h")
include_directories("../Math/inc")
include_directories("extern/imgui")
include_directories("extern/glfw3.3.8")
include_directories("extern/glew-2.1.0")
link_directories("extern/glew-2.1.0")
add_subdirectory("extern/imgui")
include_directories("extern/imgui/backends")
link_directories("extern/glfw3.3.8")
add_compile_options("/std:c++latest")
include_directories("inc")

list(REMOVE_ITEM SRC "main.cpp")
add_subdirectory("../Math/" math)
add_library("UILib" ${SRC})
  
add_executable(${BIN}
  "main.cpp"
  resource/icons.rc
)
  
target_link_libraries(${BIN} UILib imgui glfw3 opengl32 glew32s MathLib)

# copy fonts over
# @todo(mike) more consistent place for all this kind of stuff
set(FONT_FOLDER fonts)
add_custom_command(TARGET ${BIN} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${FONT_FOLDER}" "$<TARGET_FILE_DIR:${BIN}>/${FONT_FOLDER}")
install(DIRECTORY ${FONT_FOLDER} TYPE BIN) # to install dir

add_custom_command(
    TARGET ${BIN} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy_directory "${CMAKE_SOURCE_DIR}/glsl/" "${CMAKE_BINARY_DIR}/glsl"
    COMMENT "Copying glsl folder to output directory"
)