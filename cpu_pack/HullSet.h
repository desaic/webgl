#pragma once

// convex hull approximations of the fruit (from a convex decomposition, see Vhacd.h and
// bpy_scripts/load_fruit_hulls.py) used for voxel free collision: a hull is a set of planes, so a
// point to hull distance is a few dot products.

#include "BBox.h"
#include "MeshInfo.h"
#include "PointSample.h"
#include "RigidTransform.h"
#include "Vec3.h"

#include <string>
#include <unordered_map>
#include <vector>

struct HullPlane {
  Vec3f n;      // outward unit normal
  float h = 0;  // inside: n . x <= h
};

struct ConvexHull {
  std::vector<HullPlane> planes;
  Box3f box;  // of the uninset hull, so it contains the inset one
};

struct HullSet {
  std::vector<ConvexHull> hulls;
  /// surface points of the inset hulls that are not inside another hull of the set, in the
  /// inertia frame of the item. a moving fruit tests these against its neighbors.
  std::vector<SamplePoint> samples;
  Box3f box;

  /// distance of p (item inertia frame) to the hull set, negative inside. exact inside, an
  /// underestimate outside next to edges and corners. normal (optional) is the outward normal of
  /// the deciding plane. returns maxDist (and no normal) if p is farther than maxDist from
  /// every hull.
  float Distance(const Vec3f &p, float maxDist, Vec3f *normal = nullptr) const;
};

/// loads a .hulls.txt file (cpu_pack --vhacd format), moves the hulls from the input frame into
/// the inertia frame of the item with rb, and insets every hull by `inset` cm (the planes move
/// inward). sampleSpacing: spacing of the surface samples. false if the file is missing or empty.
bool LoadHullSet(const std::string &file, const RigidBody &rb, float inset, float sampleSpacing,
                 HullSet &out);

/// writes the hulls of a .hulls.txt file as an obj, one object per hull, placed with tran (inertia
/// frame pose of the instance). no inset.
bool SaveHullsObj(const std::string &file, const RigidBody &rb, const RigidTransform &tran,
                  const std::string &outFile);

/// hull sets per item index, loaded on demand from dir/<item name>.hulls.txt.
class HullLibrary {
 public:
  HullLibrary(const std::string &dir, float inset, float sampleSpacing)
      : dir_(dir), inset_(inset), spacing_(sampleSpacing) {}
  /// null if the item has no hull file.
  const HullSet *Get(unsigned itemIdx, const MeshInfo &item);

 private:
  std::string dir_;
  float inset_, spacing_;
  std::unordered_map<unsigned, HullSet> sets_;
  std::unordered_map<unsigned, bool> missing_;
};
