#include "Inflate.h"
#include "AdapDF.h"
#include "Stopwatch.h"
#include "AdapSDF.h"
#include "AdapUDF.h"
#include "Array3DUtils.h"
#include "meshutil.h"
#include "Array2D.h"
#include "ImageIO.h"
#include <deque>

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

static void SaveSlice(const std::string &file, const Array3D<short> & dist, unsigned z, float distUnit) {
  Vec3u size = dist.GetSize();
  Array2D8u slice(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      short val = dist(x, y, z);
      if (val >= 32766) {
        slice(x, y) = 255;
        continue;
      }
      if (val < -32766) {
        slice(x,y) = 0;
        continue;
      }
      float d = val * distUnit;
      if (d > 155) {
        d = 155;
      }
      if (d < -100) {
        d = -100;
      }
      slice(x, y) = uint8_t(100 + d);
    }
  }
  SavePngGrey(file, slice);
}

std::shared_ptr<AdapDF> ComputeOutsideDistanceField(const InflateConf& conf,
                                                    TrigMesh& mesh) {
  float h = ComputeVoxelSize(mesh, conf.voxResMM, conf.MAX_GRID_SIZE);
  std::shared_ptr<AdapDF> udf = std::make_shared<AdapSDF>();
  udf->distUnit = 1e-2;
  udf->voxSize = h;
  udf->band = conf.thicknessMM / h + 2;
  udf->mesh_ = &mesh;
  ComputeCoarseDist(udf.get());
  udf->band = 10000;
  Array3D8u frozen;
  udf->FastSweepCoarse(frozen);

  /*Array3D8u outside;
  outside = FloodOutside(udf->dist, 1.8 * h / udf->distUnit);

  for (size_t i = 0; i < outside.GetData().size(); i++) {
    if (!outside.GetData()[i]) {
      udf->dist.GetData()[i] = -1;
    }
  }
  for (size_t i = 0; i < outside.GetData().size(); i++) {
    outside.GetData()[i] = !outside.GetData()[i];
    }
  Vec3f voxRes(h);
  Vec3f origin = udf->origin;  
   SaveVolAsObjMesh("F:/dump/inflate_flood.obj", outside,
   (float*)(&voxRes),(float*)(&origin),
    1);*/
  SaveSlice("F:/dump/dist.png", udf->dist, udf->dist.GetSize()[2] / 2, 0.1);
  return udf;
}

TrigMesh InflateMesh(const InflateConf& conf, TrigMesh& mesh) {
  std::shared_ptr<AdapDF> udf = ComputeOutsideDistanceField(conf, mesh);
  // SaveSlice(udf->dist, udf->dist.GetSize()[2] / 2, udf->distUnit);
  TrigMesh surf;
  SDFImpAdap dfImp(udf);
  dfImp.MarchingCubes(conf.thicknessMM, &surf);
  return surf;
}
