cmake_minimum_required(VERSION 3.10)

project(cgal_wrapper)
set(CGAL_DIR "F:/CGAL/lib/cmake/CGAL")
set(GMP_DIR "C:/local/gmp/")
set(GMP_INCLUDE_DIR "C:/local/gmp/include")
set(GMP_LIB_DIR "C:/local/gmp/lib")
set(GMP_LIBRARIES "libgmp-10.lib")

set(MPFR_INCLUDE_DIR "C:/local/gmp/include")
set(MPFR_LIBRARIES "libmpfr-4.lib")

# Find CGAL package
find_package(CGAL REQUIRED)
link_directories(${GMP_LIB_DIR})
# Create the static library
add_library(cgal_wrapper STATIC
    cgal_wrapper.cpp
)

# Link CGAL to the library
target_link_libraries(cgal_wrapper PUBLIC CGAL::CGAL)

# Create the executable
add_executable(cgal_test main.cpp
  TrigMesh.cpp
  ObjLoader.cpp)

# Link the executable to the library
target_link_libraries(cgal_test cgal_wrapper)