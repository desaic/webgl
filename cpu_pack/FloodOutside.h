#pragma once
#include <cstdint>
#include "Array3D.h"

// returns a grid of labels
Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh);

// sets outside voxels to 0 and inside voxel to insideVal.
// insideVal must not be 0. 
// Does not preserve original non zero values.
void FloodOutside8u(Array3D8u &vox, uint8_t boundaryVal, uint8_t interiorVal);
