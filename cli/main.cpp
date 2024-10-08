
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "Array3DUtils.h"
#include "BBox.h"
#include "Grid3Df.h"
#include "Inflate.h"
#include "Lattice.h"
#include "cpu_voxelizer.h"
#include "SDFMesh.h"
#include "AdapUDF.h"
#include "AdapSDF.h"
#include "MeshUtil.h"
#include "VoxIO.h"
#include "ZRayCast.h"

slicer::Grid3Df MakeOctetUnitCell(Vec3f cellSize) {
  slicer::Grid3Df cell;
  cell.voxelSize = Vec3f(0.1f);
  unsigned n = cellSize[0] / cell.voxelSize[0];
  // distance values are defined on vertices instead of voxel centers
  Vec3u gridSize(n + 1);
  cell.Allocate(gridSize, 1000);
  cell.Array() = slicer::MakeOctetCell(cell.voxelSize, cellSize, gridSize);
  return cell;
}

struct VoxFun : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
    if (fun) {
      fun(x, y, z, trigIdx);
    }
  }
  std::function<void(unsigned, unsigned, unsigned, size_t)> fun;
};

// finer and denser towards center by scaling input coordinates
// to larger numbers. scale = 1 near boundary.
Vec3f DFCoord(Vec3f x, float & scale, const AdapDF* df) {
  float maxScale = 2;
  float dist = df->GetCoarseDist(x);
  scale = std::min(maxScale, (1.0f+std::abs(dist) * 0.05f));
  Vec3f y = x * scale;
  return y;
}

Vec3f CellCoord(const Vec3f & x, const Vec3f & cellSize)
{
  Vec3f y;
  for (unsigned d = 0; d < 3; d++) {
    int idx = int(x[d] / cellSize[d]);
    if (idx * cellSize[d] > x[d]) {
      idx--;
    }
    y[d] = x[d] - cellSize[d] * idx;

  }
  return y;
}

Array3D8u removePadding(const Array3D8u& input, uint8_t minVal) {
  Array3D8u out;
  Vec3u gridSize = input.GetSize();
  Vec3u mn= gridSize, mx(0);
  for (unsigned z = 0; z < gridSize[2]; z++) {
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        if (input(x, y, z) < minVal) {
          continue;
        }
        mn[0] = std::min(mn[0], x);
        mn[1] = std::min(mn[1], y);
        mn[2] = std::min(mn[2], z);
        mx[0] = std::max(mx[0], x);
        mx[1] = std::max(mx[1], y);
        mx[2] = std::max(mx[2], z);
      }
    }
  }
  mn = mn + Vec3u(2, 2, 0);
  mx = mx - Vec3u(2, 2, 0);
  Vec3u outSize = mx - mn + Vec3u(1u);
  out.Allocate(outSize, minVal);
  for (unsigned z = 0; z < outSize[2]; z++) {
    for (unsigned y = 0; y < outSize[1]; y++) {
      for (unsigned x = 0; x < outSize[0]; x++) {
        out(x, y, z) = input(x + mn[0], y + mn[1], z + mn[2]);
      }
    }
  }
  return out;
}

void MakeAcousticLattice() {
  Vec3f cellSize(8);
  slicer::Grid3Df cell = MakeOctetUnitCell(cellSize);
  std::string meshFile = "F:/meshes/acoustic/pyramid.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  InflateConf conf;
  conf.voxResMM = 0.2;
  conf.thicknessMM = 1;  
  std::shared_ptr<AdapDF> df = ComputeSignedDistanceField(conf, mesh);
  //distance field for lattice.

  Vec3u dfSize = df->dist.GetSize();
  // std::shared_ptr<AdapDF> lattice= std::make_shared<AdapSDF>();
  //lattice->Allocate(dfSize[0], dfSize[1], dfSize[2]);
  //lattice->voxSize = df->voxSize;
  //lattice->origin = df->origin;
  //lattice->distUnit = df->distUnit;
  float h = df->voxSize;
  float scale = 1.0f;
  Array3D8u voxGrid(dfSize[0] - 1, dfSize[1]- 1, dfSize[2]-1);
  voxGrid.Fill(1);
  float isoLevel = 0.92;
  for (unsigned z = 0; z < dfSize[2]; z++) {
    for (unsigned y = 0; y < dfSize[1]; y++) {
      for (unsigned x = 0; x < dfSize[0]; x++) {
        Vec3f coord(x * h, y * h, z * h);
        //voxel centers are offset from distance field value vertices
        coord += df->origin + 0.5f*Vec3f(h);
        float coarseDist = df->GetCoarseDist(coord);
        if (coarseDist > 0) {
          continue;
        }
        coord = DFCoord(coord, scale, df.get());        
        Vec3f cellX = CellCoord(coord, cellSize);
        float dist = scale * cell.Trilinear(cellX);        
        float level = isoLevel;
        if (coarseDist > -0.5f) {
          level = 1.78;
        }
        if (dist <= level) {
          voxGrid(x, y, z) = 2;
        }
        //lattice->dist(x, y, z) = short(dist / df->distUnit);
      }
    }
  }
  //TrigMesh surf;
  //SDFImpAdap dfImp(lattice);
  //float beamThickness = 0.9;
  //dfImp.MarchingCubes(beamThickness, &surf);
  //surf.SaveObj("F:/meshes/acoustic/latticeSurf2.obj");
  //Vec3f voxSize(h);
  //SaveVolAsObjMesh("F:/meshes/acoustic/vox.obj", voxGrid, (float*)(&voxSize), (float*)(&df->origin),
  //                 2);
  voxGrid = removePadding(voxGrid, 2);
  SaveVoxTxt(voxGrid, h, "F:/meshes/acoustic/octet_pyramid_skin.txt");
}

struct FuncCb : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override { 
    if (func) {
      func(x, y, z, trigIdx);
    }
  }
  std::function<void(unsigned, unsigned, unsigned, size_t)> func;  
};

static void VoxelizeMesh(uint8_t matId, const BBox& box, float voxRes,
                  const TrigMesh& mesh, Array3D8u& grid) {
  ZRaycast raycast;
  RaycastConf rcConf;
  rcConf.voxelSize_ = voxRes;
  rcConf.box_ = box;
  ABufferf abuf;

  int ret = raycast.Raycast(mesh, abuf, rcConf);
  float z0 = box.vmin[2];
  // expand abuf to vox grid
  Vec3u gridSize = grid.GetSize();
  for (unsigned y = 0; y < gridSize[1]; y++) {
    for (unsigned x = 0; x < gridSize[0]; x++) {
      const auto intersections = abuf(x, y);
      for (auto seg : intersections) {
        unsigned zIdx0 = seg.t0 / voxRes + 0.5f;
        unsigned zIdx1 = seg.t1 / voxRes + 0.5f;

        for (unsigned z = zIdx0; z < zIdx1; z++) {
          if (z >= gridSize[2]) {
            continue;
          }
          grid(x, y, z) = matId;
        }
      }
    }
  }
}

void MakeHoneyCombGrid() {
  TrigMesh m;
  m.LoadStl("F:/meshes/acoustic/honeyCombCell1.stl");
  BBox box;
  ComputeBBox(m.v, box);
  Array3D8u grid;
  Vec3f sizemm = box.vmax - box.vmin;
  float h = 0.1f;
  Vec3u gridSize;
  for (unsigned i = 0; i < 3; i++) {
    gridSize[i] = std::round(sizemm[i] / h);
  }
  grid.Allocate(gridSize[0] ,gridSize[1], gridSize[2]);
  grid.Fill(0);

  VoxelizeMesh(2, box, 0.1, m, grid);
  Vec3f voxRes(h);
  SaveVolAsObjMesh("F:/meshes/acoustic/honeyVox1.obj", grid, (float*)(&voxRes),(float*)( &box.vmin), 2);
 
  Vec3u croppedSize = gridSize;
  croppedSize[1] -= 12;
  Array3D8u croppedGrid(croppedSize, 1);

  for (unsigned z = 0; z < gridSize[2]; z++) {
    for (unsigned y = 0; y < croppedSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        croppedGrid(x, y, z) = grid(x, y + 6, z);
      }
    }
  }
  SaveVoxTxt(croppedGrid, h, "F:/meshes/acoustic/honey1.txt");
}

void WireframeLineSegs(std::string objFile) {
  TrigMesh m;
  m.LoadObj(objFile);
  std::set<Edge> edges;
  MergeCloseVertices(m);
  for (size_t i = 0; i < m.GetNumTrigs(); i++) {
    for (unsigned j = 0; j < 3; j++) {
      unsigned v0 = m.t[3 * i + j];
      unsigned v1 = m.t[3 * i + (j + 1) % 3];
      Edge edge(v0, v1);
      edges.insert(edge);
    }
  }
  std::cout << "numEdges " << edges.size() << "\n";
  std::ofstream out("F:/meshes/echoDot/wires.txt");
  out << edges.size() << "\n";
  std::vector<Edge> edgeList(edges.begin(), edges.end());
  //must >=4 for cubic spline.
  const unsigned NumCtrlPt = 4;
  int isLoop = 0;
  float segLen = 1.0f / (NumCtrlPt - 3);
  for (size_t i = 0; i < edgeList.size(); i++) {
    out << isLoop <<" 4\n";
    Vec3f v0 = m.GetVertex(edgeList[i].v[0]);
    Vec3f v1 = m.GetVertex(edgeList[i].v[1]);

    for (unsigned j = 0; j < NumCtrlPt; j++) {
      float alpha = 1.0f + segLen - float(j) * segLen;
      Vec3f v = alpha * v0 + (1.0f - alpha) * v1;
      out << v[0] << " " << v[1] << " " << v[2] << "\n";
    }
  }
}

void InflateSurface(std::string objFile) {
  TrigMesh mesh;
  mesh.LoadObj(objFile);
  InflateConf conf;
  conf.thicknessMM = 1.0f;
  conf.voxResMM = 0.5f;
  std::shared_ptr<AdapDF> sdf = UnsignedDistanceFieldBand(conf, mesh);
  // SaveSlice(udf->dist, udf->dist.GetSize()[2] / 2, udf->distUnit);
  TrigMesh surf;
  SDFImpAdap dfImp(sdf);
  dfImp.MarchingCubes(conf.thicknessMM, &surf);
  MergeCloseVertices(surf);
  surf.SaveObj("F:/meshes/echoDot/wireBound.obj");
}

int main(int argc, char** argv) {  
  //InflateSurface("F:/meshes/echoDot/wireMesh.obj");
  WireframeLineSegs("F:/meshes/echoDot/wireMesh.obj");
  //MakeHoneyCombGrid();
  //MakeAcousticLattice();
  return 0;
}
