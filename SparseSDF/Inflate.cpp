#include "Inflate.h"
#include "AdapDF.h"
#include "Stopwatch.h"
#include "AdapUDF.h"
#include "Array3DUtils.h"

#include <deque>

void ComputeCoarseDist(AdapDF* df) {
  df->ClearTemporaryArrays();
  Utils::Stopwatch timer;
  timer.Start();
  df->BuildTrigList(df->mesh_);
  float ms = timer.ElapsedMS();
  timer.Start();
  df->Compress();
  ms = timer.ElapsedMS();
  timer.Start();
  df->mesh_->ComputePseudoNormals();
  ms = timer.ElapsedMS();
  timer.Start();
  df->ComputeCoarseDist();
  ms = timer.ElapsedMS();
  Array3D8u frozen;
  timer.Start();
  df->FastSweepCoarse(frozen);
}

float ComputeVoxelSize(const TrigMesh & mesh, float voxResMM, unsigned MAX_GRID_SIZE) {
  BBox box;
  ComputeBBox(mesh.v, box);
  float h = voxResMM;
  for (size_t d = 0; d < 3; d++) {
    float len = box.vmax[d] - box.vmin[d];
    if (len / h > MAX_GRID_SIZE) {
      h = len / MAX_GRID_SIZE;
    }
  }
  return h;
}

TrigMesh InflateMesh(const InflateConf& conf, TrigMesh& mesh) {
  float h = ComputeVoxelSize(mesh, conf.voxResMM, conf.MAX_GRID_SIZE);
  std::shared_ptr<AdapDF> udf = std::make_shared<AdapUDF>();
  udf->distUnit = 1e-2;
  udf->voxSize = h;
  udf->band = conf.thicknessMM / h + 1;
  udf->mesh_ = &mesh;
  ComputeCoarseDist(udf.get());
  Array3D8u outside;
  outside = FloodOutside(udf->dist, h / udf->distUnit);
  Vec3f voxRes(h);
  // SaveVolAsObjMesh("F:/dump/inflate_flood.obj", frozen, (float*)(&voxRes),
  // 1);
  for (size_t i = 0; i < outside.GetData().size(); i++) {
    if (!outside.GetData()[i]) {
      udf->dist.GetData()[i] = -1;
    }
  }
  // SaveSlice(udf->dist, udf->dist.GetSize()[2] / 2, udf->distUnit);
  TrigMesh surf;
  SDFImpAdap dfImp(udf);
  dfImp.MarchingCubes(conf.thicknessMM, &surf);
  return surf;
}
