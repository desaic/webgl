cmake_minimum_required(VERSION 3.16)
set(BIN "cpu_pack")
PROJECT (CpuPack)
FILE(GLOB EXE_SRC "*.cpp" "*.c")
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

include_directories("../Math/inc/")
include_directories("../Mesh/inc")
include_directories("../ImageIO")
include_directories("./pocketfft/")
include_directories("../Util")
include_directories("../SparseSDF")

add_executable(${BIN} ${EXE_SRC})
target_link_libraries(${BIN} Mesh MathLib ImageIO SparseSDF Util)
