#include "AdapSDF.h"
#include "SDFMesh.h"
#include "TrigMesh.h"
#include "BBox.h"
#include "MarchingCubes.h"
#include "FastSweep.h"
#include "MeshUtil.h"
#include "MeshOptimization.h"
#include <filesystem>
#include <fstream>
#include <iostream>

void SaveDistGrid() {
  TrigMesh mesh;
  std::string meshFile = "F:/meshes/lunebox/botLatticeField.stl";
  mesh.LoadStl(meshFile);
  BBox box;
  ComputeBBox(mesh.v, box);
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  float h = 1.0f;
  SDFImpAdap* imp = new SDFImpAdap(sdf);
  SDFMesh sdfMesh(imp);
  sdfMesh.SetMesh(&mesh);
  sdfMesh.SetVoxelSize(h);
  sdfMesh.SetBand(27);
  sdfMesh.Compute();
  TrigMesh surf;
  sdfMesh.MarchingCubes(2, &surf);

  Vec3f origin = sdf->origin;
  std::cout << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  const Array3D<short>& dist = sdf->dist;
  Vec3u size = dist.GetSize();
  std::cout << size[0] << " " << size[1] << " " << size[2] << "\n";

  std::ofstream out("F:/meshes/lunebox/bottField.txt");
  out << "origin\n"
      << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  out << "voxelSize\n" << h << " " << h << " " << h << "\n";
  out << "grid\n" << size[0] << " " << size[1] << " " << size[2] << "\n";

  out << std::fixed << std::setprecision(3);
  float minThick = 0.3;
  float maxThick = 0.8;
  float maxDist = 15;
  float dTdx = (maxThick - minThick) / 15;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float d = dist(x, y, z) * sdf->distUnit;
        float t = d * dTdx;
        if (t > maxThick) {
          t = maxThick;
        }
        if (t < minThick) {
          t = minThick;
        }
        out << t << " ";
      }
      out << "\n";
    }
    out << "\n";
  }
  // std::filesystem::path p(meshFile);
  // p.replace_extension("surf2.obj");
  // surf.SaveObj(p.string());  
}

void TestSDF() { 
  std::string rigidFile = "F:/meshes/head/hardshell.obj";
  std::string softFile = "F:/meshes/head/back_head.obj";
  float rigidDist = 0.5f;
   const float h = 0.25f;
   const float narrowBand = 8;
   const float distUnit = 0.005f;
  TrigMesh rigidMesh;
  rigidMesh.LoadObj(rigidFile);

  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  sdf->distUnit = distUnit;
  SDFImpAdap* imp = new SDFImpAdap(sdf);
  SDFMesh sdfMesh(imp);
  sdfMesh.SetMesh(&rigidMesh);
  sdfMesh.SetVoxelSize(h);
  sdfMesh.SetBand(narrowBand);
  sdfMesh.Compute();

  TrigMesh softMesh;
  softMesh.LoadObj(softFile);
  std::shared_ptr<AdapSDF> softSdf = std::make_shared<AdapSDF>();
  softSdf->voxSize = 0.2;
  softSdf->band = 16;
  softSdf->distUnit = distUnit;
  softSdf->BuildTrigList(&softMesh);
  softSdf->Compress();
  softMesh.ComputePseudoNormals();
  softSdf->ComputeCoarseDist();
  CloseExterior(softSdf->dist, softSdf->MAX_DIST);  
  Array3D8u frozen;
  softSdf->FastSweepCoarse(frozen);
  
  std::cout << softSdf->origin[0] << " " << softSdf->origin[1] << " "
            << softSdf->origin[2] << "\n";
  BBox box;
  ComputeBBox(softMesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  Array3D<short> softCopy = softSdf->dist;
  //take 0.5mm outer shell
  short skinThickness = 0.5 / distUnit;
  for (size_t i = 0; i < softCopy.GetData().size(); i++) {
    softCopy.GetData()[i] = -softCopy.GetData()[i] - skinThickness;
  }
  //subtract shell of rigid sdf + 0.6mm
  Vec3u size = softSdf->dist.GetSize();
  float rigidOuter = 0.55;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float softd = softCopy(x, y, z) * distUnit;
        Vec3f worldCoord = softSdf->WorldCoord(Vec3f(x, y, z));
        float rigidDist = sdf->GetFineDist(worldCoord);
        rigidDist = -(rigidDist - rigidOuter);
        if (rigidDist > 0) {
          float dist = std::max(rigidDist, softd);
          softCopy(x, y, z) = dist / distUnit;
        }
      }
    }
  }

  //subtract partial skin from original soft mesh
  for (size_t i = 0; i < softCopy.GetData().size(); i++) {
    short d0 = softSdf->dist.GetData()[i]; 
    softSdf->dist.GetData()[i] = std::max(d0, short(-softCopy.GetData()[i]));
  }
  
  //sdfMesh.MarchingCubes(0, &surf); 
  TrigMesh surf;
  MarchingCubes(softSdf->dist, 0, distUnit, softSdf->voxSize, softSdf->origin, &surf);  
  MergeCloseVertices(surf);
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.05, 0.4, surf.v,
                                        surf.t);

  std::filesystem::path p(softFile);
  p.replace_extension("surf2.obj");
  surf.SaveObj(p.string());
}
