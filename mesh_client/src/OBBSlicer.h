#ifndef OBB_SLICER
#define OBB_SLICER

#include "BBox.h"

#include <functional>

typedef std::function <void(int, int, int)> VoxelCb;

///@param obb in a coordinate where cell size is uniformly 1.
void SliceOBB(OBBox & obb, const VoxelCb cb);

#endif