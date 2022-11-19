cmake_minimum_required(VERSION 3.0)
project(ImguiTest)
set(BIN ImguiTest)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
file(GLOB SRC "*.cpp" "*.h")

include_directories("extern/glfw3.3.8")
link_directories("extern/glfw3.3.8")

include_directories("extern/glew-2.1.0")
link_directories("extern/glew-2.1.0")

include_directories("extern/imgui")
add_subdirectory("extern/imgui")
include_directories("extern/imgui/backends")

add_compile_options("/std:c++latest")
add_executable(${BIN}
  ${SRC}
  resource/icons.rc
  )

target_link_libraries(${BIN} imgui glfw3 glew32s opengl32)

# copy fonts over
# @todo(mike) more consistent place for all this kind of stuff
set(FONT_FOLDER fonts)
add_custom_command(TARGET ${BIN} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${FONT_FOLDER}" "$<TARGET_FILE_DIR:${BIN}>/${FONT_FOLDER}")
install(DIRECTORY ${FONT_FOLDER} TYPE BIN) # to install dir