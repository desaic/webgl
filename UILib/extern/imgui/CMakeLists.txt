set(LIB imgui)
file(GLOB SRCS "*.cpp" "*.h")
include_directories(backends)
add_library(${LIB}
  ${SRCS}
  "backends/imgui_impl_opengl3.h"
  "backends/imgui_impl_opengl3.cpp"
  "backends/imgui_impl_glfw.h"
  "backends/imgui_impl_glfw.cpp"
)

if(WIN32)
  target_link_libraries(${LIB} opengl32)
else()
  target_link_libraries(${LIB} GL)
endif()
target_include_directories(${LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
