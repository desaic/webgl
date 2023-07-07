#ifndef FAST_SWEEP_H
#define FAST_SWEEP_H
#include "Array3D.h"
void FastSweep(Array3D<short> &dist, const Array3D<uint8_t>& frozenCells, Vec3f unit, float band);
#endif