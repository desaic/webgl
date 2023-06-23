#pragma once
#include "Vec3.h"

class TrigMesh;

struct GridCell {
  static const unsigned NUM_PT = 8;
  // voxel corner coordinates
  //   4----5
  // 6----7
  //
  //   0----1
  // 2----3
   Vec3f p[NUM_PT];
   float val[NUM_PT];	//value of the function at this grid corner
   GridCell() {
     for (unsigned i = 0; i < NUM_PT; i++) {
       val[i] = 1e2f;
     }
   }
};

///\param mesh output a mini mesh for cell
void MarchCube(GridCell &cell, float level, TrigMesh*mesh);
