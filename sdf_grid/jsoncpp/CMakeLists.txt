cmake_minimum_required(VERSION 3.15)

option(DOUBLE_CONVERSION_VENDORED "Use vendored double-conversion library" ON)

    # Add double-conversion library
    add_library(double-conversion STATIC
        double-conversion/bignum.cc
        double-conversion/bignum-dtoa.cc
        double-conversion/cached-powers.cc
        double-conversion/double-to-string.cc
        double-conversion/fast-dtoa.cc
        double-conversion/fixed-dtoa.cc
        double-conversion/string-to-double.cc
        double-conversion/strtod.cc
    )
    target_include_directories(double-conversion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Main JSON library
# BUILD_SHARED_LIBS is a standard CMake variable that
# controls whether libraries are built as shared or static
add_library(json
    json.cpp
)
target_link_libraries(json PRIVATE double-conversion)
set_target_properties(json PROPERTIES PUBLIC_HEADER "json.h")
