#ifndef FAST_SWEEP_H
#define FAST_SWEEP_H
#include "Array3D.h"

/// <summary>
/// 
/// </summary>
/// <param name="dist">distance grid with values in distance unit</param>
/// <param name="voxSize">voxel size in mm</param>
/// <param name="unit">unit for values stored in distance grid in mm</param>
/// <param name="band">narrow band in mm</param>
void FastSweep(Array3D<short>& dist, float voxSize, float unit, float band);

#endif