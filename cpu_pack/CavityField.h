#pragma once

#include "AdapSDF.h"
#include "Array3D.h"
#include "BBox.h"
#include "PackingScene.h"
#include "Vec3.h"

#include <vector>

/// local occupancy window used to find and score cavities.
/// a cavity is free space inside the container, within `band` cm of the wall, that a ball of
/// radius R cannot reach (narrow deep valleys, undercuts below overhangs), and that can still hold
/// a ball of radius minClear.
struct CavityParams {
  float dx = 0.4f;
  float band = 8.0f;
  float R = 1.0f;          // tool ball and shaft radius (cm)
  float coneDeg = 70.0f;   // tool axis may tilt this far from the outward direction
  float minClear = 1.0f;   // an anchor needs a free ball this big (cm)
  float sealClear = 0.5f;  // a passage needs this clearance to count as open (cm)
  float objClear = 0.3f;   // unreachable voxels count in the objective above this clearance (cm)
};

struct CavityComp {
  Vec3f center;       // weighted centroid (cm)
  Vec3f axis;         // principal axis, unit
  Vec3f deepest;      // voxel center with the largest clearance
  float clearance = 0;  // clearance at deepest (cm)
  float volume = 0;   // cm3
  float score = 0;    // weighted volume, nearer the wall counts more
  unsigned voxels = 0;
};

struct CavityWindow {
  Vec3f origin;
  float dx = 0.4f;
  Vec3u size;
  Array3D8u solid;  // placed fruit, exclusion
  Array3D8u wall;   // outside the container
  Array3D<float> depth;  // distance below the container wall (cm), 0 outside
  Array3D<float> sd;     // signed container distance, positive outside (the outside is open air)

  Vec3f Center(unsigned i, unsigned j, unsigned k) const {
    return origin + dx * Vec3f(i + 0.5f, j + 0.5f, k + 0.5f);
  }
};

/// builds a window over box, stamping every instance except skipId (~0u: none).
void BuildCavityWindow(PackingScene &scene, const Box3f &box, const CavityParams &cp, unsigned skipId,
                       CavityWindow &w);

/// stamps one fruit pose into w.solid. returns the number of voxels that were already solid.
unsigned StampFruit(PackingScene &scene, unsigned itemId, const RigidTransform &tran, CavityWindow &w,
                    Array3D8u &solid, unsigned *newVox = nullptr);

/// cavities of the window. marks every kept cavity voxel in mask (same size as w) with its
/// component id + 1 when mask is not null. only components whose center lies in core (if given)
/// are returned; total weighted volume of all kept components goes to totalScore.
std::vector<CavityComp> FindCavities(const CavityWindow &w, const Array3D8u &solid, const CavityParams &cp,
                                     const Box3f *core, float *totalScore,
                                     Array3D<unsigned> *labels = nullptr);

/// exact squared-distance transform helper: distance (cm) from every voxel to the nearest voxel
/// where seed != 0. voxels outside the window are not seeds.
void DistanceToSeeds(const Array3D8u &seed, float dx, Array3D<float> &dist);

/// unreachable, air-connected cavity voxels (cav), clearance (D), and free voxels the tool does reach.
void BuildCavMask(const CavityWindow &w, const Array3D8u &solid, const CavityParams &cp, Array3D<float> &D,
                  Array3D8u &cav, Array3D8u &reachFree);
