cmake_minimum_required(VERSION 3.0)
project(Rigid)
set(SIM_LIBS 
debug PositionBasedDynamics_d 
debug Simulation_d 
debug Utils_d
optimized PositionBasedDynamics 
optimized Simulation 
optimized Utils)

############################################################
# GenericParameters
############################################################
include_directories("GenericParameters/include")

include_directories("../Math/inc")
include_directories("../Mesh/inc")
if (NOT TARGET MathLib)
  message(STATUS "add math lib")
  add_subdirectory("../Math/" "Math")
endif()
if (NOT TARGET Mesh)
  message(STATUS "add mesh lib")
  add_subdirectory("../Mesh/" "Mesh")
endif()

add_executable(RigidDemo
  RigidDemo.cpp
)

target_link_directories(RigidDemo PRIVATE  "./lib/")
target_link_libraries(RigidDemo ${SIM_LIBS} Mesh MathLib)

include_directories( "./eigen" )
