#include "tests.h"
#include "Array2D.h"
#include "BBox.h"
#include "heap.hpp"
#include "GridTree.h"
#include "ImageUtil.h"
#include "TrayClient.h"
#include "SDF.h"
#include "cpt.h"
#include "Timer.hpp"
#include "LineIter2D.h"
#include "TrigIter2D.h"
#include "TetSlicer.h"
#include "OBBSlicer.h"
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

void TestHeap() {
  heap h;
  h.insert(0.1, 0);
  h.insert(0.2, 1);
  h.insert(0.4, 2);
  h.insert(0.5, 3);

  h.update(0.6, 1);
  h.printTree();
}

void TestTrigIter() {
  std::vector<Vec2f> v(3);
  v[0] = Vec2f(1.5, 2);
  v[1] = Vec2f(5.7, -3.7);
  v[2] = Vec2f(-4.1, -3.7);
  TrigIter2D trigIter(v[0], v[1], v[2]);
  BBox2D box;
  box.Compute(v);
  Vec2<int> origin(int(std::round(box.mn[0])), int(std::round(box.mn[1])));
  Vec2<int> topRight(int(std::round(box.mx[0])), int(std::round(box.mx[1])));
  Vec2<int> size = topRight - origin + Vec2<int>(1, 1);
  Array2D<uint8_t> img(size[0], size[1]);
  img.Fill(0);

  while (trigIter()) {
    int x0 = trigIter.x0();
    int x1 = trigIter.x1();
    int y = trigIter.y();
    std::cout << "([" << x0 << ", " << x1 << "], " << y << ") ";
    for (int x = x0; x < x1; x++) {
      img(x - origin[0], y - origin[1]) = 1;
    }
    
    ++trigIter;
  }
  std::cout << "\n";
  for (size_t row = 0; row < img.GetSize()[1]; row++) {
    for (size_t col = 0; col < img.GetSize()[0]; col++) {
      std::cout << int(img(col, row)) << " ";
    }
    std::cout << "\n";
  }

  std::cout << "\n";
}

void TestTetSlicer()
{
  std::cout << int(-3.7)<<"\n";
  std::cout << int(-3.4) << "\n";
  OBBox box;
  float obbVals[] = { -0.0392107, 1.42734, 2.37363, 21.4142, 21.3849, 1.12073,
    11.4142, - 11.3986, - 0.597374, - 9.53674e-09, 0.104672, - 1.99726 };
  box.origin = Vec3f(obbVals[0], obbVals[1], obbVals[2]);
  box.axes[0] = Vec3f(obbVals[3], obbVals[4], obbVals[5]);
  box.axes[1] = Vec3f(obbVals[6], obbVals[7], obbVals[8]);
  box.axes[2] = Vec3f(obbVals[9], obbVals[10], obbVals[11]);

  SparseVoxel<int> voxels;
  OBBSlicer slicer;
  slicer.Compute(box, voxels);
  std::cout << "zmin " << voxels.zmin;
  int zmax = voxels.zmin + voxels.slices.size();
  int ymin = 0, ymax = -1;
  int xmin = 0, xmax = -1;
  for (size_t z = 0; z < voxels.slices.size(); z++) {
    const SparseSlice<int>& slice = voxels.slices[z];
    //skip empty slices.
    if (slice.IsEmpty() ) {
      continue;
    }
    //uninitialized
    if (ymax < ymin) {
      ymin = slice.ymin;
      ymax = slice.ymin + int(slice.rows.size());
    }
    else {
      ymin = std::min(ymin, slice.ymin);
      ymax = std::max(ymax, slice.ymin + int(slice.rows.size()));
    }

    for (size_t y = 0; y < slice.rows.size(); y++) {
      if (slice.rows[y].IsEmpty()) {
        continue;
      }
      //uninitialized
      if (xmax < xmin) {
        xmin = slice.rows[y].lb;
        xmax = slice.rows[y].ub;
      }
      else {
        xmin = std::min(xmin, slice.rows[y].lb);
        xmax = std::max(xmax, slice.rows[y].ub);
      }
    }
  }
  std::cout << "x range " << xmin << ", " << xmax << "\n";
  std::cout << "y range " << ymin << ", " << ymax << "\n";
  std::cout << "z range " << voxels.zmin << ", " << zmax << "\n";
  Array2D<uint8_t> img(xmax - xmin, ymax - ymin);
  for (size_t z = 0; z < voxels.slices.size(); z++) {
    img.Fill(0); 
    const SparseSlice<int>& slice = voxels.slices[z];
    for (size_t y = 0; y < slice.rows.size(); y++) {
      int row = int(y) + slice.ymin - ymin;
      Interval<int> xrange = slice.rows[y];
      for (int x = xrange.lb; x < xrange.ub; x++) {
        int col = x - xmin;
        img(col, row) = 1;
      }
    }

    for (size_t row = 0; row < img.GetSize()[1]; row++) {
      for (size_t col = 0; col < img.GetSize()[0]; col++) {
        std::cout << int(img(col, row)) << " ";
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
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
  MarchingCubes(sdf, 0.5, &mesh);
  meshes.push_back(mesh);
  client->SendMeshes();
  
}
