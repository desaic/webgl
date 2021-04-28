#include "tests.h"
#include "Array2D.h"
#include "BBox.h"
#include "GridTree.h"
#include "ImageUtil.h"
#include "TrayClient.h"
#include "SDF.h"
#include "cpt.h"
#include "Timer.hpp"

#include <iostream>

void TestNumMeshes(TrayClient* client)
{
  int numMeshes = client->GetNumMeshes();
  std::cout << "num meshes " << numMeshes << "\n";
}

void TestMesh(TrayClient* client)
{
  int numMeshes = client->GetNumMeshes();
  std::cout << "num meshes " << numMeshes << "\n";
  if (numMeshes == 0) {
    return;
  }
  const int iter = 100;
  Scene& scene = client->GetScene();
  std::vector<TrigMesh>& meshes = scene.GetMeshes();
  if (meshes.size() == 0) {
    return;
  }
  std::vector<float> verts = meshes[0].v;
  for (int i = 0; i < iter; i++) {
    for (size_t j = 0; j < meshes[0].v.size(); j++) {
      meshes[0].v[j] = verts[j] * (1 + i / float(iter));
    }
    client->SendMeshes();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  meshes[0].v = verts;
}

void TestPng()
{
  size_t gridSize = 512;
  Array2D8u img(gridSize, gridSize);
  GridTree<float> grid;
  grid.Allocate(gridSize, gridSize, gridSize);
  Vec3u s = grid.GetSize();

  TreePointer ptr(&grid);
  const unsigned zAxis = 2;
  for (unsigned x = 0; x < s[0]; x++) {
    float dx = x / float(s[0]);
    dx -= 0.3f;
    for (unsigned y = 0; y < s[1]; y++) {
      float dy = y / float(s[1]);
      dy -= 0.4f;
      ptr.PointTo(x, y, 0);
      for (unsigned z = 0; z < s[2]; z++) {
        float dz = z / float(s[2]);
        dz -= 0.5f;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < 0.05) {
          ptr.CreatePath();
          AddVoxelValue(ptr, d);
        }
        ptr.Increment(zAxis);
      }
    }
  }

  unsigned z = 200;
  for (unsigned x = 0; x < s[0]; x++) {
    float dx = x / float(s[0]);
    dx -= 0.3f;
    for (unsigned y = 0; y < s[1]; y++) {
      float dy = y / float(s[1]);
      dy -= 0.4f;
      bool exists = ptr.PointTo(x, y, z);
      if (exists) {
        float d = 0.0f;
        GetVoxelValue(ptr, d);
        img(x, y) = d * 250;
      }
    }
  }

  SavePng("test.png", img);
}

void TestCPT(TrayClient* client)
{
  SDFMesh sdf;
  Scene& s = client->GetScene();
  std::vector<TrigMesh>& meshes = s.GetMeshes();
  if (meshes.size() == 0) {
    std::cout << "no mesh to test cpt\n";
    return;
  }
  sdf.mesh = &meshes[0];
  sdf.band = 5;
  //mm
  const float voxelSize = 0.25;
  sdf.voxelSize = voxelSize;
  cpt(sdf);
  FastMarch(sdf);
  Vec3u gridSize = sdf.idxGrid.GetSize();
  for (int z = 0; z < 50; z++) {
    if (z >= gridSize[2]) {
      break;
    }
    Array2D8u img(gridSize[0], gridSize[1]);
    img.Fill(255);
    TreePointer ptr(&sdf.sdf);
    for (size_t x = 0; x < gridSize[0]; x++) {
      for (size_t y = 0; y < gridSize[1]; y++) {
        ptr.PointTo(x, y, z);
        bool exists = ptr.HasValue();
        if (!exists) {
          continue;
        }
        float val;
        GetVoxelValue(ptr, val);
        img(x, y) = 100+val*20;
      }
    }
    std::string outFile = std::to_string(z) + ".png";
    SavePng(outFile, img);
  }

  TrigMesh mesh;
  MarchingCubes(sdf, 1.5, &mesh);
  meshes.push_back(mesh);
  client->SendMeshes();
  
}
