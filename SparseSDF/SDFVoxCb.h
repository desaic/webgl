#ifndef SDF_VOX_CB
#define SDF_VOX_CB

#include "VoxCallback.h"
#include "AdapSDF.h"
#include "TrigMesh.h"

struct SDFVoxCb : public VoxCallback {
  
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  virtual void BeginTrig(size_t trigIdx);
  /// free any triangle specific data.
  virtual void EndTrig(size_t trigIdx);
  TrigMesh* m = nullptr;
  Array3D8u* grid = nullptr;
  AdapSDF* sdf = nullptr;
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