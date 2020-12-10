PROJECT (MeshProc)

include_directories(include)
include_directories(E:/BULLET_PHYSICS/include/bullet)
include_directories(E:/BULLET_PHYSICS/include/bullet/HACD)
include_directories(E:/BULLET_PHYSICS/include/bullet/ConvexDecomposition)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

FILE(GLOB EXE_SRC "src/*.cpp" "src/*.c")
set(CMAKE_CXX_STANDARD 17)

link_directories("E:/BULLET_PHYSICS/lib")

add_executable(meshproc ${EXE_SRC} )
target_link_libraries(meshproc ws2_32.lib)
set(DebugLibs BulletDynamics_Debug
	BulletCollision_Debug
	LinearMath_Debug
	HACD_Debug
	ConvexDecomposition_Debug)
set(ReleaseLibs BulletDynamics
	BulletCollision
	LinearMath
	HACD
	ConvexDecomposition)
		
target_link_libraries(meshproc
    debug ${DebugLibs}
    optimized ${ReleaseLibs})
