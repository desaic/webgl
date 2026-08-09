cmake_minimum_required(VERSION 3.16)
set(BIN "cpu_pack")
set(BENCH_BIN "cpu_pack_bench")
set(MELONE_TEST_BIN "melone_test")
PROJECT (CpuPack)
FILE(GLOB CORE_SRC CONFIGURE_DEPENDS "*.cpp" "*.c" "quickhull/*.cpp")
# main.cpp, benchmarks.cpp, and melone_test.cpp each define main(), so they
# stay out of the shared library and link into their own executable.
list(REMOVE_ITEM CORE_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/melone_test.cpp")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for MSVC (Windows)
if(MSVC)
  # Debug flags
  set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /RTC1 /MTd /D_DEBUG")
  set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /RTC1 /MTd /D_DEBUG")

  # Release flags
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /DNDEBUG /MT")
  set(CMAKE_C_FLAGS_RELEASE "/O2 /Oi /GL /DNDEBUG /MT")

  # Link-time optimization for release
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
  # Debug flags for GCC/Clang (Linux)
  set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -D_DEBUG")
  set(CMAKE_C_FLAGS_DEBUG "-g -O0 -D_DEBUG")

  # Release flags for GCC/Clang (Linux)
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
  set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

include_directories(".")
add_subdirectory("../Math" "Math")
add_subdirectory("../Mesh" "Mesh")
add_subdirectory("../ImageIO" "ImageIO")
add_subdirectory("../SparseSDF" "SparseSDF")
add_subdirectory("../MeshSimplification" "MeshSimplification")

include_directories("../Math/inc/")
include_directories("../Mesh/inc")
include_directories("../ImageIO")
include_directories("./pocketfft/")
include_directories("../Util")
include_directories("../SparseSDF")
include_directories("../MeshSimplification")
include_directories("./quickhull")

add_library(pack_core STATIC ${CORE_SRC})
target_include_directories(pack_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/pocketfft"
  "${CMAKE_CURRENT_SOURCE_DIR}/quickhull")
target_link_libraries(pack_core Mesh MathLib ImageIO SparseSDF Util MeshSimplification)

add_executable(${BIN} main.cpp)
target_link_libraries(${BIN} pack_core)

add_executable(${BENCH_BIN} benchmarks.cpp)
target_link_libraries(${BENCH_BIN} pack_core)

# melone_two.obj test case: packs fruits_1 into a ~14 cm melon container
# using medium large, medium small, and small groups.
add_executable(${MELONE_TEST_BIN} melone_test.cpp)
target_link_libraries(${MELONE_TEST_BIN} pack_core)
