#include "AdapSDF.h"
#include "AdapUDF.h"
#include "SDFMesh.h"
#include "TrigMesh.h"
#include "BBox.h"
#include "MarchingCubes.h"
#include "FastSweep.h"
#include "HalfEdgeMesh.h"
#include "MeshUtil.h"
#include "MeshOptimization.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>

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

void TrigToPolyMesh(const TrigMesh & mesh, PolyMesh&pm) {
  size_t numV = mesh.GetNumVerts();
  for (size_t i = 0; i < numV; i++) {
    pm.V().push_back(mesh.GetVertex(i));
  }
  
  size_t numT = mesh.GetNumTrigs();
  for (size_t i = 0; i < numT; i++) {
    PolyFace face({mesh.t[3 * i], mesh.t[3 * i + 1], mesh.t[3 * i + 2]});
    pm.F().push_back(face);
  }
}

std::vector<unsigned> TrigNeighbors(const HalfEdgeMesh & hem, unsigned tIdx) {
  std::vector<unsigned> n;
  const auto& face = hem.F(tIdx);
  for (unsigned i = 0; i < face.n; i++) {
    unsigned heIdx = hem.HalfEdgeIndex(tIdx, i);
    const auto &he = hem.he[heIdx];
    if(he.hasTwin()) {
      //const auto& twin = hem.he[he.twin];
      n.push_back(hem.edgeFaceIndex[he.twin]);
    }
  }
  return n;
}

Vec3f NormalTrig(const PolyMesh & pm, unsigned t) {
  Vec3f v0 = pm.V()[pm.F(t)[0]];
  Vec3f v1 = pm.V()[pm.F(t)[1]];
  Vec3f v2 = pm.V()[pm.F(t)[2]];
  Vec3f n = (v1 - v0).cross(v2 - v0);
  n.normalize();
  return n;
}

void FloodMeshByNormal(const HalfEdgeMesh& hem, unsigned seedTrig, int label,
                       std::vector<int>& labels, bool ignoreOri) {
  std::queue<unsigned> q;
  q.push(seedTrig);
  const float MAX_ANGLE = 30/180.0f * 3.1415926;
  while (!q.empty()) {
    unsigned t = q.front();
    q.pop();
    std::vector<unsigned> nbrs = TrigNeighbors(hem, t);
    labels[t] = label;
    Vec3f n0 = NormalTrig(hem.m_, t);
    for (size_t i = 0; i < nbrs.size(); i++) {
      unsigned nbr = nbrs[i];
      if (labels[nbr] > 0) {
        continue;
      }
      Vec3f ni = NormalTrig(hem.m_, nbr);
      float dot = n0.dot(ni);
      if (ignoreOri) {
        dot = std::abs(dot);
      }
      float angle = acos(dot);

      if (angle < MAX_ANGLE) {
        labels[nbr] = label;
        q.push(nbr);
      }
    }
  }
}

/// @brief 
/// @param mesh 
/// @return cluster ids for each triangle 
std::vector<int> FloodMeshByNormal(const TrigMesh & mesh, bool ignoreOri = false) {
  size_t numT = mesh.GetNumTrigs();
  PolyMesh pm;
  TrigToPolyMesh(mesh, pm);
  HalfEdgeMesh hem(pm);
  
  //0 unlabeled.
  std::vector<int> labels(numT, 0);
  int labelCount = 1;
  for (size_t t = 0; t < numT; t++) {
    if (labels[t] > 0) {
      continue;
    }
    FloodMeshByNormal(hem, t, labelCount, labels, ignoreOri);
    labelCount++;
  }
  return labels;
}

TrigMesh Subset(const std::vector<int>& labels, int targetLabel,
                const TrigMesh& mesh) {
  TrigMesh out;
  std::vector<unsigned> oldToNewVert(mesh.GetNumVerts(),
                                     mesh.GetNumVerts() + 1);
  for (size_t i = 0; i < mesh.GetNumTrigs(); i++) {
    if (labels[i] != targetLabel) {
      continue;
    }
    for (unsigned vi = 0; vi < 3; vi++) {
      unsigned v = mesh.t[3 * i + vi];
      unsigned newIndex = oldToNewVert[v];
      Vec3f newCoord = mesh.GetTriangleVertex(i, vi);
      if(newIndex>=out.GetNumVerts()){
        newIndex = out.GetNumVerts();
        oldToNewVert[v] = newIndex;
        out.v.push_back(newCoord[0]);
        out.v.push_back(newCoord[1]);
        out.v.push_back(newCoord[2]);
      }
      out.t.push_back(newIndex);
    }
  }
  return out;
}

void ProcessHead() {
  std::string rigidFile = "F:/meshes/head/hardshell.obj";
  std::string softFile = "F:/meshes/head/front.obj";

  TrigMesh softMesh;
  softMesh.LoadObj(softFile);
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
  float waxDrain = 0.3;
  // union outer shell
  Vec3u size = softSdf->dist.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float softd = softCopy(x, y, z) * distUnit;
        Vec3f worldCoord = softSdf->WorldCoord(Vec3f(x, y, z));
        float rigidDist = sdf->GetFineDist(worldCoord) - waxDrain;
        float dist = std::min(rigidDist, softd);
        softCopy(x, y, z) = dist / distUnit;        
      }
    }
  }

  // subtract skin from original soft mesh
  short skinThickness = 0.52 / distUnit;
  for (size_t i = 0; i < softCopy.GetData().size(); i++) {
    short d0 = softSdf->dist.GetData()[i];
    short dCopy = softCopy.GetData()[i];
    if (dCopy >= AdapDF::MAX_DIST) {
      continue;
    }
    softSdf->dist.GetData()[i] = std::max(d0, short(dCopy + skinThickness));
  }

  TrigMesh surf;
  MarchingCubes(softSdf->dist, 0, distUnit, softSdf->voxSize, softSdf->origin,
                &surf);
  MergeCloseVertices(surf);
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.05, 0.4, surf.v,
                                          surf.t);

  std::filesystem::path p(softFile);
  p.replace_extension("surf3.obj");
  surf.SaveObj(p.string());
}

std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh & mesh) {
  std::shared_ptr<AdapSDF> softSdf = std::make_shared<AdapSDF>();
  softSdf->voxSize = h;
  softSdf->band = 16;
  softSdf->distUnit = distUnit;
  softSdf->BuildTrigList(&mesh);
  softSdf->Compress();
  mesh.ComputePseudoNormals();
  softSdf->ComputeCoarseDist();
  CloseExterior(softSdf->dist, softSdf->MAX_DIST);
  Array3D8u frozen;
  softSdf->FastSweepCoarse(frozen);
  return softSdf;
}

std::shared_ptr<AdapUDF> ComputeUDF(float distUnit, float h, TrigMesh& mesh) {
  std::shared_ptr<AdapUDF> udf = std::make_shared<AdapUDF>();
  udf->voxSize = h;
  udf->band = 16;
  udf->distUnit = distUnit;
  udf->BuildTrigList(&mesh);
  udf->Compress();
  mesh.ComputePseudoNormals();
  udf->ComputeCoarseDist();
  for (size_t i = 0; i < udf->dist.GetData().size(); i++) {
    short d = udf->dist.GetData()[i];
    if (d < 0) {
      udf->dist.GetData()[i] = -d;
    }
  }
  Array3D8u frozen;
  udf->FastSweepCoarse(frozen);
  return udf;
}

void ProcessFront0() {  
  std::string softFile = "F:/meshes/head/front_merge.obj";
  std::string sideFile = "F:/meshes/head/front_patch.obj";

  TrigMesh softMesh;
  softMesh.LoadObj(softFile);
  MergeCloseVertices(softMesh);
  std::vector<int> labels = FloodMeshByNormal(softMesh);

  int maxLabel = 1;
  for (size_t i = 0; i < labels.size(); i++) {
    maxLabel = std::max(maxLabel, labels[i]);
  }

  std::string outDir = "F:/meshes/head/front_seg";
  std::ofstream statsOut(outDir + "/stats.txt");
  for (int label = 0; label <= maxLabel; label++) {
    TrigMesh face = Subset(labels, label, softMesh);
    float area = SurfaceArea(face);
    statsOut << label << " " << area << "\n";
    std::string outFile = outDir  + "/side" + std::to_string(label) + ".obj";
    face.SaveObj(outFile);
  }

  TrigMesh sideMesh;
  sideMesh.LoadObj(sideFile);
  
  const float h = 0.25f;
  const float narrowBand = 8;
  const float distUnit = 0.005f;

  std::shared_ptr<AdapSDF> softSdf = ComputeSDF(distUnit, 0.2, softMesh);
  std::shared_ptr<AdapUDF> surfUdf = ComputeUDF(distUnit, 0.2, sideMesh);

  std::cout << softSdf->origin[0] << " " << softSdf->origin[1] << " "
            << softSdf->origin[2] << "\n";
  BBox box;
  ComputeBBox(softMesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";


  //volume near surface
  Array3D<short> softCopy = softSdf->dist;

  // remove volume within 0.5mm of surface.
  const float surfThickness = 0.5f;  
  Vec3u size = softSdf->dist.GetSize();
  float MAX_POSITIVE_DIST = 100;  

  // subtract from volume.
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float softd = softCopy(x, y, z) * distUnit;
        Vec3f worldCoord = softSdf->WorldCoord(Vec3f(x, y, z));        
        float surfDist = surfUdf->GetCoarseDist(worldCoord);
        if (surfDist >= AdapDF::MAX_DIST) {
          surfDist = MAX_POSITIVE_DIST;
        }
        surfDist -= surfThickness;
        softd = std::max(-surfDist, softd);        
        softCopy(x, y, z) = softd / distUnit;
      }
    }
  }

  TrigMesh surf;
  MarchingCubes(softCopy, 0, distUnit, softSdf->voxSize, softSdf->origin,
                &surf);
  MergeCloseVertices(surf);
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.05, 0.4, surf.v,
                                          surf.t);

  std::filesystem::path p(softFile);
  p.replace_extension("latt.obj");
  surf.SaveObj(p.string());
}

void MakeFrontLatticeMesh() {
  std::string surfFile = "F:/meshes/head/front.obj";
  std::string headFile = "F:/meshes/head/head_trim.obj";

  TrigMesh frontMesh;
  frontMesh.LoadObj(surfFile);
  MergeCloseVertices(frontMesh);
  TrigMesh headMesh;
  headMesh.LoadObj(headFile);
  const float h = 0.25f;
  const float narrowBand = 8;
  const float distUnit = 0.005f;

  std::shared_ptr<AdapSDF> headSdf = ComputeSDF(distUnit, h, headMesh);
  std::shared_ptr<AdapUDF> surfUdf = ComputeUDF(distUnit, h, frontMesh);

  std::cout << surfUdf->origin[0] << " " << surfUdf->origin[1] << " "
            << surfUdf->origin[2] << "\n";
  BBox box;
  ComputeBBox(headMesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  // volume near surface
  Array3D<short> frontCopy = surfUdf->dist;

  const float skinThickness = 0.4f;
  const float latticeThickness = 3;
  Vec3u size = surfUdf->dist.GetSize();
  float MAX_POSITIVE_DIST = 100;


  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float frontd = frontCopy(x, y, z) * distUnit;
        // set volume to between 0.5 to latticeThickness away from surface.
        frontd = std::abs(frontd - (skinThickness + 0.5f * latticeThickness)) - 0.5f * latticeThickness;
        // intersect volume with head volume
        Vec3f worldCoord = surfUdf->WorldCoord(Vec3f(x, y, z));
        float headDist = headSdf->GetCoarseDist(worldCoord);
        if (headDist >= AdapDF::MAX_DIST) {
          headDist = MAX_POSITIVE_DIST;
        }
        frontd = std::max(frontd, headDist + skinThickness);
        frontCopy(x, y, z) = frontd / distUnit;
      }
    }
  }

  TrigMesh surf;
  MarchingCubes(frontCopy, 0, distUnit, surfUdf->voxSize, surfUdf->origin,
                &surf);
  MergeCloseVertices(surf);
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.05, 0.4, surf.v,
                                          surf.t);

  std::filesystem::path p(surfFile);
  p.replace_extension("latt.obj");
  surf.SaveObj(p.string());
}


void OrientFlatGroups() {
  std::string softFile = "F:/meshes/head/extruded0.obj";
  TrigMesh mesh;
  mesh.LoadObj(softFile);
  MergeCloseVertices(mesh);
  bool ignoreOri = true;
  std::vector<int> labels = FloodMeshByNormal(mesh, ignoreOri);
  
  int maxLabel = 1;
  for (size_t i = 0; i < labels.size(); i++) {
    maxLabel = std::max(maxLabel, labels[i]);
  }
  std::vector<std::vector<unsigned> > labelTrigs(maxLabel + 1);
  for (size_t i = 0; i < labels.size(); i++) {
    labelTrigs[labels[i]].push_back(i);
  }

  std::string outDir = "F:/meshes/head/front_seg";
  std::ofstream statsOut(outDir + "/stats.txt");
  for (int label = 0; label <= maxLabel; label++) {
    const std::vector<unsigned> &subset = labelTrigs[label];    
    if (subset.size() < 10) {
      continue;
    }    
    Vec3f avgNormal(0);
    for (size_t ti = 0; ti < subset.size(); ti++) {
      unsigned t = subset[ti];
      Vec3f v0 = mesh.GetTriangleVertex(t, 0);
      Vec3f v1 = mesh.GetTriangleVertex(t, 1);
      Vec3f v2 = mesh.GetTriangleVertex(t, 2);
      v1 = v1 - v0;
      v2 = v2 - v0;
      v1 = v1.cross(v2);
      float trigArea = v1.norm();
      avgNormal += trigArea * mesh.GetTrigNormal(ti);
    }
    float area = SurfaceArea(mesh);
    avgNormal.normalize();
    for (size_t ti = 0; ti < subset.size(); ti++) {
      unsigned t = subset[ti];
      Vec3f n = mesh.GetTrigNormal(ti);
      if (n.dot(avgNormal) < 0) {
        unsigned i0 = mesh.t[3 * ti];
        unsigned i1 = mesh.t[3 * ti + 1];
        mesh.t[3 * ti] = i1;
        mesh.t[3 * ti + 1] = i0;
      }
    }
  }
  for (int label = 0; label <= maxLabel; label++) {
    TrigMesh face = Subset(labels, label, mesh);
    float area = SurfaceArea(face);
    statsOut << label << " " << area << "\n";
    std::string outFile = outDir + "/side" + std::to_string(label) + ".obj";
    face.SaveObj(outFile);
  }
  mesh.SaveObj("F:/meshes/head/ori.obj");
}


void ProcessNose() {
  std::string rigidFile = "F:/meshes/head/hardshell.obj";
  std::string softFile = "F:/meshes/head/nose.obj";
  std::string sideFile = "F:/meshes/head/nose_patch.obj";

  TrigMesh sideMesh;
  sideMesh.LoadObj(sideFile);
  TrigMesh softMesh;
  softMesh.LoadObj(softFile);
  // std::vector<int> labels = FloodMeshByNormal(softMesh);
  // TrigMesh face = Subset(labels, 3, softMesh);
  // face.SaveObj("F:/meshes/head/nose_side3.obj");
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

  std::shared_ptr<AdapSDF> softSdf = ComputeSDF(distUnit, 0.2, softMesh);
  std::shared_ptr<AdapUDF> surfUdf = ComputeUDF(distUnit, 0.2, sideMesh);

  std::cout << softSdf->origin[0] << " " << softSdf->origin[1] << " "
            << softSdf->origin[2] << "\n";
  BBox box;
  ComputeBBox(softMesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  Array3D<short> softCopy = softSdf->dist;
  // union rigid shell into soft
  const float surfThickness = 1.0f;
  // intersect surface udf and union mesh
  Vec3u size = softSdf->dist.GetSize();
  float MAX_POSITIVE_DIST = 100;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float softd = softCopy(x, y, z) * distUnit;
        Vec3f worldCoord = softSdf->WorldCoord(Vec3f(x, y, z));
        float rigidDist = sdf->GetFineDist(worldCoord);
        float surfDist = surfUdf->GetCoarseDist(worldCoord);
        if (surfDist >= AdapDF::MAX_DIST) {
          surfDist = MAX_POSITIVE_DIST;
        }
        surfDist -= surfThickness;

        // if (rigidDist <= 0) {
        softd = std::min(rigidDist, softd);
        //}
        // intersect with thin volume around surface mesh
        // if (surfDist >= 0) {
        softd = std::max(surfDist, softd);
        //}
        softCopy(x, y, z) = softd / distUnit;
      }
    }
  }

  // for (size_t i = 0; i < softCopy.GetData().size(); i++) {
  //   short d0 = softSdf->dist.GetData()[i];
  //   softSdf->dist.GetData()[i] = short(softCopy.GetData()[i]);
  //   std::max(short(softCopy.GetData()[i]), short(-d0));
  // }

  // subtract 0.5mm from surface.
  const float skinThick = 0.2f;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float softd = softCopy(x, y, z) * distUnit;
        Vec3f worldCoord = softSdf->WorldCoord(Vec3f(x, y, z));
        float surfDist = surfUdf->GetCoarseDist(worldCoord);
        if (surfDist >= AdapDF::MAX_DIST) {
          surfDist = MAX_POSITIVE_DIST;
        }
        surfDist -= skinThick;

        // if (rigidDist <= 0) {
        //}
        // intersect with thin volume around surface mesh
        // if (surfDist >= 0) {
        softd = std::max(-surfDist, softd);
        //}
        softCopy(x, y, z) = softd / distUnit;
      }
    }
  }

  TrigMesh surf;
  MarchingCubes(softCopy, 0, distUnit, softSdf->voxSize, softSdf->origin,
                &surf);
  MergeCloseVertices(surf);
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.05, 0.4, surf.v,
                                          surf.t);

  std::filesystem::path p(softFile);
  p.replace_extension("add.obj");
  surf.SaveObj(p.string());
}

#include <sstream>
void PadGridXY() {
  std::ifstream in("F:/meshes/head/loop.txt");
  std::string lines[8];
  for (int i = 0; i < 8; i++) {
    std::getline(in, lines[i]);
  }
  
  std::istringstream iss(lines[7]);
  Vec3u gridSize;
  iss >> gridSize[0] >> gridSize[1] >> gridSize[2];
  Array3D8u gridIn(gridSize[0], gridSize[1], gridSize[2]);
  for (size_t i = 0; i < gridIn.GetData().size(); i++) {
    int num;
    in >> num;
    gridIn.GetData()[i] = uint8_t(num);
  }

  unsigned padding = 10;
  Vec3u outSize(gridSize[0] + 2 * padding, gridSize[1] + 2 * padding,
                gridSize[2]);
  Array3D8u gridOut(outSize, 3);
  for (unsigned z = 0; z < gridSize[2]; z++) {
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        unsigned dsty = y + padding;
        unsigned dstx = x + padding;
        gridOut(dstx, dsty, z) = gridIn(x, y, z);
      }
    }
  }

  std::ofstream out("F:/meshes/head/loop_sparse_2mm.txt");
  for (int i = 0; i < 7; i++) {
    out << lines[i] << "\n";
  }
  out << outSize[0] << " " << outSize[1] << " " << outSize[2] << "\n";
  for (unsigned z = 0; z < outSize[2]; z++) {
    for (unsigned y = 0; y < outSize[1]; y++) {
      for (unsigned x = 0; x < outSize[0]; x++) {
        out<< int(gridOut(x, y, z))<<" ";
      }
      out << "\n";
    }
    out<<"\n";
  }
  out.close();
}

namespace {

unsigned findIndex(unsigned* arr, unsigned len, unsigned x) {
  for (unsigned i = 0; i < len; i++) {
    if (arr[i] == x) {
      return i;
    }
  }
  return 0;
}

bool IsSkinny(const Triangle& t) {
  const float MIN_DIST = 2e-2;
  for (unsigned i = 0; i < 3; i++) {
    Vec3f v0 = t.v[i];
    Vec3f v1 = t.v[(i + 1) % 3];
    Vec3f v2 = t.v[(i + 2) % 3];

    Vec3f e1 = v1 - v0;
    Vec3f e2 = v2 - v0;
    float norm1 = e1.norm();
    float norm2 = e2.norm();
    if (norm1 < MIN_DIST || norm2 < MIN_DIST) {
      return true;
    }
    e2.normalize();
    Vec3f vertical = e1 - e1.dot(e2) * e2;
    float dist = vertical.norm();
    if (dist < 0.1) {
    //  std::cout << "debug\n";
    }
    if (dist < MIN_DIST) {
      return true;
    }
  }
  return false;
}

void RemoveSkinnyTrigs(TrigMesh& mesh) {
  std::vector<unsigned> kept;
  for (size_t i = 0; i < mesh.GetNumTrigs(); i++) {
    Triangle triangle = mesh.GetTriangleVerts(i);
    if (!IsSkinny(triangle)) {
      kept.push_back(mesh.GetIndex(i, 0));
      kept.push_back(mesh.GetIndex(i, 1));
      kept.push_back(mesh.GetIndex(i, 2));
    }
  }
  mesh.t = kept;
}

}  // namespace

void ExtrudeAlongNormal(const std::string & objFile, float thick) {
  TrigMesh mesh;
  mesh.LoadObj(objFile);
  MergeCloseVertices(mesh);
  
  mesh.ComputeVertNormals();
  PolyMesh pm;
  TrigToPolyMesh(mesh, pm);
  HalfEdgeMesh hem(pm);
  TrigMesh outMesh;
  outMesh = mesh;
  for (size_t i = 0; i < mesh.GetNumTrigs(); i++) {
    unsigned v0Idx = outMesh.GetNumVerts();
    unsigned oldVidx[3];
    //make new verts
    for (size_t j = 0; j < 3; j++) {
      unsigned vidx = mesh.GetIndex(i, j);
      oldVidx[j] = vidx;
      Vec3f v = mesh.GetTriangleVertex(i, j);
      Vec3f vn = mesh.nv[vidx];
      v += -thick * vn;
      outMesh.AddVert(v[0], v[1], v[2]);
    }
    
    //add a new triangle in opposite winding.
    // 0 1 2 -> 0 2 1
    outMesh.t.push_back(v0Idx);
    outMesh.t.push_back(v0Idx + 2);
    outMesh.t.push_back(v0Idx + 1);

    unsigned newVidx[3] = {v0Idx, v0Idx + 1, v0Idx + 2};

    //add side faces for boundary edges
    unsigned edgeIdx = hem.faceEdgeIndex[i];
    for (unsigned j = 0; j < 3; j++) {
      const auto& edge = hem.he[edgeIdx];
      if(edge.hasTwin()){
        edgeIdx = edge.next;
        continue;
      }

      unsigned oldv0 = edge.vertex;
      unsigned oldv1 = hem.he[edge.next].vertex;
      unsigned newv0 = newVidx[findIndex(oldVidx, 3, oldv0)];
      unsigned newv1 = newVidx[findIndex(oldVidx, 3, oldv1)];

      outMesh.t.push_back(oldv1);
      outMesh.t.push_back(oldv0);
      outMesh.t.push_back(newv0);

      outMesh.t.push_back(oldv1);      
      outMesh.t.push_back(newv0);
      outMesh.t.push_back(newv1);
    }
  }
  MergeCloseVertices(outMesh);
  outMesh.SaveObj("F:/meshes/head/extruded.obj");
}

void GetInnerSurf() {
  std::string meshFile = "F:/meshes/head/ear_l.obj";
  std::string outFile = "F:/meshes/head/ear_l_inner.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);

  const float h = 0.25f;
  const float narrowBand = 16;
  const float distUnit = 0.005f;

  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  sdf->distUnit = distUnit;
  SDFImpAdap* imp = new SDFImpAdap(sdf);
  SDFMesh sdfMesh(imp);
  sdfMesh.SetMesh(&mesh);
  sdfMesh.SetVoxelSize(h);
  sdfMesh.SetBand(narrowBand);
  sdfMesh.Compute();

  BBox box;
  ComputeBBox(mesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  TrigMesh surf;
  MarchingCubes(sdf->dist, -2, distUnit, sdf->voxSize, sdf->origin,
                &surf);
  MergeCloseVertices(surf);
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.02, 0.4, surf.v,
                                          surf.t);
  surf.SaveObj(outFile);
}

void TestSDF() { 
  //OrientFlatGroups();
  //GetInnerSurf();
  //ExtrudeAlongNormal("F:/meshes/head/front.obj", 0.52);
  // ProcessNose();
  //ProcessHead();
  //ProcessFront();
  //PadGridXY();

  MakeFrontLatticeMesh();
}
