#include "AdapSDF.h"
#include "SDFMesh.h"
#include "TrigMesh.h"
#include "BBox.h"

#include <filesystem>
#include <fstream>
#include <iostream>

void TestSDF() { 
  TrigMesh mesh;
  std::string meshFile = "F:/meshes/lunebox/botLatticeField.stl";
  mesh.LoadStl(meshFile);
  BBox box;
  ComputeBBox(mesh.v, box);
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  float h = 1.0f;
  SDFImpAdap *imp = new SDFImpAdap(sdf);
  SDFMesh sdfMesh(imp);
  sdfMesh.SetMesh(&mesh);
  sdfMesh.SetVoxelSize(h);
  sdfMesh.SetBand(27);
  sdfMesh.Compute();
  TrigMesh surf;
  sdfMesh.MarchingCubes(2, &surf);
  
  Vec3f origin = sdf->origin;
  std::cout << origin[0] << " " << origin[1] << " " << origin[2]
            << "\n";
  const Array3D<short> &dist = sdf->dist;
  Vec3u size = dist.GetSize();
  std::cout << size[0] << " " << size[1] << " " << size[2] << "\n";

  std::ofstream out("F:/meshes/lunebox/bottField.txt");
  out << "origin\n" << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
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
  //std::filesystem::path p(meshFile);
  //p.replace_extension("surf2.obj");
  //surf.SaveObj(p.string());
}
