PROJECT (Util)
cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 20)
FILE(GLOB LIB_SRC "*.cpp" "*.c" EXCLUDE "*/main.cpp")
list(REMOVE_ITEM LIB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
message("Util lib src ${LIB_SRC}")
add_library("Util" STATIC ${LIB_SRC})
