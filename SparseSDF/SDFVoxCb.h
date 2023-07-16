#ifndef SDF_VOX_CB
#define SDF_VOX_CB

#include "VoxCallback.h"
#include "AdapSDF.h"
#include "TrigMesh.h"

#include <unordered_map>

struct TrigInfo {
  // triangle frame
  Vec3f x, y, z;
};

struct SDFVoxCb : public VoxCallback {
  
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  virtual void BeginTrig(size_t trigIdx);
  /// free any triangle specific data.
  virtual void EndTrig(size_t trigIdx);
  TrigMesh* m = nullptr;
  Array3D8u* grid = nullptr;
  AdapSDF* sdf = nullptr;

  std::unordered_map<size_t, TrigInfo> trigInfo;
};

//computes values at the fine grid
struct SDFFineVoxCb : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  TrigMesh* m = nullptr;
  Array3D8u* grid = nullptr;
  AdapSDF* sdf = nullptr;
};

#endif