#ifndef OBB_SLICER
#define OBB_SLICER

#include "BBox.h"

#include <functional>

typedef std::function <void(int, int, int)> VoxelCb();

void SliceOBB(OBBox & obb);

#endif