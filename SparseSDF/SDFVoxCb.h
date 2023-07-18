#ifndef SDF_VOX_CB
#define SDF_VOX_CB

#include <unordered_map>

#include "AdapSDF.h"
#include "TrigMesh.h"
#include "VoxCallback.h"
#include "PointTrigDist.h"

struct SDFVoxCb : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  virtual void BeginTrig(size_t trigIdx) override;
  /// free any triangle specific data.
  virtual void EndTrig(size_t trigIdx) override;
  TrigMesh* m = nullptr;
  AdapSDF* sdf = nullptr;

  std::unordered_map<size_t, TrigFrame> trigInfo;
};

// computes values at the fine grid
struct SDFFineVoxCb : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  virtual void BeginTrig(size_t trigIdx) override;
  /// free any triangle specific data.
  virtual void EndTrig(size_t trigIdx) override;
  TrigMesh* m = nullptr;
  AdapSDF* sdf = nullptr;

  std::unordered_map<size_t, TrigFrame> trigInfo;
};

// builds intersecting triangle list of
//  coarse voxels around coarse vertices.
struct TrigListVoxCb : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  virtual void BeginTrig(size_t trigIdx) override;
  /// free any triangle specific data.
  virtual void EndTrig(size_t trigIdx) override;
  TrigMesh* m = nullptr;
  Array3D8u* grid = nullptr;
  AdapSDF* sdf = nullptr;

  std::unordered_map<size_t, TrigFrame> trigInfo;
};
#endif