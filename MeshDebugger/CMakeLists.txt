cmake_minimum_required(VERSION 3.16)
set(BIN "MeshDebugger")
PROJECT (MeshDebugger)
add_subdirectory("../Math" "Math")
add_subdirectory("../Mesh" "Mesh")
add_subdirectory("../Util" "Util")
add_subdirectory("../UILib" "ui")
add_subdirectory("../SparseSDF" "SparseSDF")
add_subdirectory("../ZRayCast" "ZRayCast")
include_directories("../MeshSimplification")
add_subdirectory("../MeshSimplification" "MeshSimplification")
include_directories(.)
FILE(GLOB EXE_SRC "*.cpp" "*.c")
set(CMAKE_CXX_STANDARD 17)
include_directories("../Math/inc/")
include_directories("../Mesh/inc")
include_directories("../ImageIO")
include_directories("../UILib/inc")
include_directories("../Util")
include_directories("../SparseSDF")
include_directories("../ZRayCast")
include_directories("../tiny_bvh")
link_directories("../UILib/extern/glfw3.3.8")
link_directories("../UILib/extern/glew-2.1.0")

option(USE_NANOVDB "test openvdb" OFF)
if (USE_NANOVDB)
  set(NANOVDB_INC_DIR "F:/github/openvdb/nanovdb/")
  include_directories(${NANOVDB_INC_DIR})
  add_compile_definitions(USE_NANOVDB)
endif()
 
include_directories("../cgal_wrapper/")
include_directories("../cdt/")
add_executable(${BIN} ${EXE_SRC} resource/icons.rc)

target_link_directories(${BIN} PRIVATE "../cgal_wrapper/build/")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_libraries(${BIN} cgal_wrapperd)
else()
  target_link_libraries(${BIN} cgal_wrapper)
endif()

target_link_libraries(${BIN} UILib imgui glfw3 opengl32 glew32s SparseSDF ZRayCast Mesh ImageIO Util MathLib MeshSimplification)

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