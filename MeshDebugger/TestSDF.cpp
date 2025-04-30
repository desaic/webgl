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
#include "VoxIO.h"
#include "Array3D.h"
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
  std::string surfFile = "F:/meshes/head/front_surf.obj";
  std::string headFile = "F:/meshes/head/head_trim.obj";

  TrigMesh frontMesh;
  frontMesh.LoadObj(surfFile);
  MergeCloseVertices(frontMesh);
  TrigMesh headMesh;
  headMesh.LoadObj(headFile);
  const float h = 0.25f;
  const float distUnit = 0.005f;

  std::shared_ptr<AdapSDF> headSdf = ComputeSDF(distUnit, 2 * h, headMesh);
  std::shared_ptr<AdapUDF> surfUdf = ComputeUDF(distUnit, h, frontMesh);

  std::cout << surfUdf->origin[0] << " " << surfUdf->origin[1] << " "
            << surfUdf->origin[2] << "\n";
  BBox box;
  ComputeBBox(headMesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  // volume near surface
  Array3D<short> frontCopy = surfUdf->dist;
  Array3D8u frozen;
  FastSweepParUnsigned(frontCopy, h, surfUdf->distUnit, 30, frozen);
  const float softLayerThickness = 3;
  const float latticeThickness = 2.5;
  const float softSolidDist = 3.0f;
  const float rigidDist = 4.0f;
  Vec3u size = surfUdf->dist.GetSize();
  float MAX_POSITIVE_DIST = 100;


  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float frontd = frontCopy(x, y, z) * distUnit;
        // set volume to 1mm away from rigid
        frontd = std::abs(frontd - rigidDist) - 0.5f * softLayerThickness;
        // intersect volume with 3mm away into head volume
        Vec3f worldCoord = surfUdf->WorldCoord(Vec3f(x, y, z));
        float headDist = headSdf->GetCoarseDist(worldCoord);
        if (headDist >= AdapDF::MAX_DIST) {
          headDist = MAX_POSITIVE_DIST;
        }
        frontd = std::max(frontd, headDist + softSolidDist);
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
  p.replace_extension("solid.obj");
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

TrigMesh GetInnerSurf(TrigMesh & mesh, float h, float offset) {
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

  //fast sweep whole grid
  //float band = 10000;
  //Array3D8u frozen;
  //FastSweepPar(sdf->dist, sdf->voxSize, sdf->distUnit, band, frozen);

  BBox box;
  ComputeBBox(mesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  TrigMesh surf;
  MarchingCubes(sdf->dist, -offset, distUnit, sdf->voxSize, sdf->origin,
                &surf);
  MergeCloseVertices(surf);
  //MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, 0.02, 0.4, surf.v,
  //                                        surf.t);
  return surf;
}

void MakeEarCore() {
  std::string meshFile = "F:/meshes/head/ear_l.obj";
  std::string outFile = "F:/meshes/head/ear_l_inner.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  TrigMesh out = GetInnerSurf(mesh, 0.25, 2);
  out.SaveObj(outFile);
}

void MakeRigidMesh() {
  std::string meshFile = "F:/meshes/head/head_trim.obj";
  std::string outFile = "F:/meshes/head/head_rigid.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  TrigMesh rigid = GetInnerSurf(mesh, 0.5, 4);
  rigid.SaveObj(outFile);
}

void MakeHollowMesh() {
  std::string meshFile = "F:/meshes/head/head_rigid1.obj";
  std::string outFile = "F:/meshes/head/head_hollow.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  TrigMesh rigid = GetInnerSurf(mesh, 0.5, 4);
  rigid.SaveObj(outFile);
}

void MakeSkinMesh() {
  std::string meshFile = "F:/meshes/head/head_surf0.obj";
  std::string outFile = "F:/meshes/head/head_skinsub.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  TrigMesh rigid = GetInnerSurf(mesh, 0.25, 0.5);
  rigid.SaveObj(outFile);
}

void MakeUpperSolid() {
  std::string meshFile = "F:/meshes/head/upper.obj";
  std::string outFile = "F:/meshes/head/upper_soft.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  //1.5 for skin + lattice. 2.5mm for remaing solid soft
  TrigMesh rigid = GetInnerSurf(mesh, 0.25, 1.5);
  rigid.SaveObj(outFile);
}

void MakeLowerSolid() {
  std::string meshFile = "F:/meshes/head/lower.obj";
  std::string outFile = "F:/meshes/head/lower_soft.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  // 2.5 for skin + lattice. 1.5mm for remaing solid soft
  TrigMesh rigid = GetInnerSurf(mesh, 0.25, 2.5);
  rigid.SaveObj(outFile);
}

void MakeGearMesh() {
  std::string meshFile = "F:/meshes/motor_air/planet_gear.obj";
  std::string outFile = "F:/meshes/motor_air/planet_gear_inner.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  TrigMesh inner = GetInnerSurf(mesh, 0.06, 0.15);
  inner.SaveObj(outFile);
}

void DisplaceVert() {
  std::string meshFile = "F:/meshes/motor_air/planet_gear.obj";
  std::string outFile = "F:/meshes/motor_air/planet_gear_disp.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  mesh.ComputeVertNormals();
  for (size_t i = 0; i < mesh.GetNumVerts(); i++) {
    Vec3f v = mesh.GetVertex(i);
    Vec3f n = mesh.nv[i];
    v += -0.15f * n;
    mesh.v[3 * i] = v[0];
    mesh.v[3 * i+1] = v[1];
    mesh.v[3 * i+2] = v[2];
  }
  mesh.SaveObj(outFile);
}

Array3D8u MirrorCubicStructure(Array3D8u& grid) {
  Vec3u size = grid.GetSize();

  Array3D8u out;
  Vec3u outSize = 2u * size;
  out.Allocate(outSize, 0);
  for (unsigned z = 0; z < outSize[2]; z++) {
    for (unsigned y = 0; y < outSize[1]; y++) {
      for (unsigned x = 0; x < outSize[0]; x++) {
        unsigned srcz = z;
        unsigned srcy = y;
        unsigned srcx = x;
        if (srcz >= size[2]) {
          srcz = outSize[2] - srcz - 1;
        }
        if (srcy >= size[1]) {
          srcy = outSize[1] - srcy - 1;
        }
        if (srcx >= size[1]) {
          srcx = outSize[1] - srcx - 1;
        }
        //sort xyz decreasing order
        if (srcx < srcy) {
          unsigned temp = srcx;
          srcx = srcy;
          srcy = temp;
        }
        if (srcy < srcz) {
          unsigned temp = srcy;
          srcy = srcz;
          srcz = temp;
        }
        if (srcx < srcy) {
          unsigned temp = srcx;
          srcx = srcy;
          srcy = temp;
        }
        out(x,y,z) = grid(srcx, srcy,srcz);
      }
    }
  }  
  return out;
}

void InvertVal(Array3D8u& grid) {
  std::vector<uint8_t>& data = grid.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = 255 - data[i];
  }
}

void Normalize(std::vector<float>& v) {
  if (v.size() == 0) {
    return;
  }
  float sum = 0;
  for (size_t i = 0; i < v.size(); i++) {
    sum += v[i];
  }
  if (sum == 0) {
    return;
  }
  float scale = 1.0f / sum;
  for (size_t i = 0; i < v.size(); i++) {
    v[i] *= scale;
  }
}

int GaussianFilter1D(float sigma, unsigned rad, std::vector<float>& filter) {
  if (rad > 100) {
    return -1;
  }
  if (sigma < 0.01f) {
    sigma = 0.01f;
  }
  unsigned filterSize = 2 * rad + 1;
  filter.resize(filterSize);
  float pi = 3.14159f;
  float factor = 1.0f / (sigma * std::sqrt(2 * pi));
  filter[rad] = factor;
  for (unsigned i = 0; i < rad; i++) {
    float x = (float(i) + 1) / sigma;
    float gauss = factor * std::exp(-0.5f * x * x);
    filter[rad + i + 1] = gauss;
    filter[rad - 1 - i] = gauss;
  }
  Normalize(filter);
  return 0;
}

// with mirror boundary condition
void FilterVec(uint8_t* v, size_t len, const std::vector<float>& kern) {
  size_t halfKern = kern.size() / 2;

  std::vector<uint8_t> padded(len + 2 * halfKern);
  for (size_t i = 0; i < halfKern; i++) {
    padded[i] = v[halfKern - i];
    padded[len - i] = v[len - halfKern + i];
  }
  for (size_t i = 0; i < len; i++) {
    padded[i + halfKern] = v[i];
  }

  for (size_t i = 0; i < len; i++) {
    float sum = 0;
    for (size_t j = 0; j < kern.size(); j++) {
      sum += kern[j] * padded[i + j];
    }
    v[i] = uint8_t(sum);
  }
}

void FilterDecomp(Array3D8u& grid, const std::vector<float>& kern) {
  Vec3u gsize = grid.GetSize();
  // z
  for (unsigned i = 0; i < gsize[0]; i++) {
    for (unsigned j = 0; j < gsize[1]; j++) {
      uint8_t* vec = &grid(i, j, 0);
      FilterVec(vec, gsize[2], kern);
    }
  }
  // y
  for (unsigned i = 0; i < gsize[0]; i++) {
    for (unsigned k = 0; k < gsize[2]; k++) {
      std::vector<uint8_t> v(gsize[1]);
      for (unsigned j = 0; j < gsize[1]; j++) {
        v[j] = grid(i, j, k);
      }
      FilterVec(v.data(), gsize[1], kern);
      for (unsigned j = 0; j < gsize[1]; j++) {
        grid(i, j, k) = v[j];
      }
    }
  }
  // x
  for (unsigned j = 0; j < gsize[1]; j++) {
    for (unsigned k = 0; k < gsize[2]; k++) {
      std::vector<uint8_t> v(gsize[0]);
      for (unsigned i = 0; i < gsize[0]; i++) {
        v[i] = grid(i, j, k);
      }
      FilterVec(v.data(), gsize[0], kern);
      for (unsigned i = 0; i < gsize[0]; i++) {
        grid(i, j, k) = v[i];
      }
    }
  }
}


void Upsample2x(const Array3D8u& gridIn, Array3D8u& gridOut) {
  Vec3u oldSize = gridIn.GetSize();
  gridOut.Allocate(oldSize[0] * 2, oldSize[1] * 2, oldSize[2] * 2);
  Vec3u newSize = gridOut.GetSize();
  for (unsigned i = 0; i < newSize[0]; i++) {
    for (unsigned j = 0; j < newSize[1]; j++) {
      for (unsigned k = 0; k < newSize[2]; k++) {
        gridOut(i, j, k) = gridIn(i / 2, j / 2, k / 2);
      }
    }
  }
}

void Downsample2x(const Array3D8u& gridIn, Array3D8u& gridOut) {
  Vec3u oldSize = gridIn.GetSize();
  gridOut.Allocate(oldSize[0] / 2, oldSize[1] / 2, oldSize[2] / 2);
  Vec3u newSize = gridOut.GetSize();
  for (unsigned i = 0; i < newSize[0]; i++) {
    unsigned i0 = i * 2;
    for (unsigned j = 0; j < newSize[1]; j++) {
      unsigned j0 = j * 2;
      for (unsigned k = 0; k < newSize[2]; k++) {
        unsigned k0 = k * 2;
        int sum = gridIn(i0, j0, k0);
        sum += gridIn(i0 + 1, j0, k0);
        sum += gridIn(i0, j0 + 1, k0);
        sum += gridIn(i0 + 1, j0 + 1, k0);
        sum += gridIn(i0, j0, k0 + 1);
        sum += gridIn(i0 + 1, j0, k0 + 1);
        sum += gridIn(i0, j0 + 1, k0 + 1);
        sum += gridIn(i0 + 1, j0 + 1, k0 + 1);
        sum /= 8;
        gridOut(i, j, k) = sum;
      }
    }
  }
}
void Scale(Array3D8u& grid, float scale) {
  std::vector<uint8_t>& data = grid.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = uint8_t(scale * data[i]);
  }
}
void Thresh(Array3D8u& grid, float t) {
  std::vector<uint8_t>& data = grid.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] > t) {
      data[i] = 1;
    } else {
      data[i] = 0;
    }
  }
}

void LoadBinVox() {
  std::string binFile = "F:/meshes/3dcubic64/5481.bin";
  std::ifstream in(binFile, std::iostream::binary);
  Vec3u size;
  in.read((char*)(&size), 3 * sizeof(unsigned));
  Array3D8u grid;
  size_t inputBytes = size[0] * size[1] * size[2] / 8;
  std::vector<uint8_t> binArr(inputBytes);
  in.read((char*)binArr.data(),inputBytes);
  grid.Allocate(size,0);
  for (size_t i = 0; i < inputBytes; i++) {
    for (size_t j = 0; j < 8; j++) {
      grid.GetData()[i * 8 + j] = (binArr[i] >> j) & 1;
    }
  }
  Vec3f voxRes(1, 1, 1);
  Array3D8u mirrored = MirrorCubicStructure(grid);
  SaveVolAsObjMesh("F:/dump/5481.obj", mirrored, (float*)(&voxRes), 1);
  Array3D8u up;
  Upsample2x(mirrored, up);
  //SaveVoxTxt(mirrored, 1, "F:/dump/5481.txt");
  std::vector<float> kern;
  GaussianFilter1D(1, 3, kern);
  Scale(up, 200);
  FilterDecomp(up, kern);
  FilterDecomp(up, kern);
  Array3D8u down;
  Downsample2x(up, down);
  Thresh(down, 40);
  SaveVolAsObjMesh("F:/dump/5481_big_smooth.obj", down, (float*)(&voxRes), 1);
  SaveVoxTxt(down, 1, "F:/dump/5481.txt");
}

void WriteDistField() {
  TrigMesh mesh;
  mesh.LoadObj("F:/meshes/acoustic/cyl90mm.obj");
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  float h = 0.5f;
  SDFImpAdap* imp = new SDFImpAdap(sdf);
  SDFMesh sdfMesh(imp);
  sdfMesh.SetMesh(&mesh);
  sdfMesh.SetVoxelSize(h);
  sdfMesh.SetBand(16);
  sdfMesh.Compute();

  Array3D8u frozen;
  FastSweepPar(sdf->dist, h, sdf->distUnit, 300, frozen);

  Vec3f origin = sdf->origin;
  std::cout << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  const Array3D<short>& dist = sdf->dist;
  Vec3u size = dist.GetSize();
  std::cout << size[0] << " " << size[1] << " " << size[2] << "\n";

  std::ofstream out("F:/meshes/acoustic/diskLayers.txt");

  float outdx = 50;
  Vec3u outSize(unsigned(size[0] * h / outdx) + 1,
                unsigned(size[1] * h / outdx) + 1,
                unsigned(size[2] * h / outdx) + 1);

  out << "origin\n"
      << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  out << "voxelSize\n" << outdx << " " << outdx << " " << outdx << "\n";
  out << "grid\n"
      << outSize[0] << " " << outSize[1] << " " << outSize[2] << "\n";

  out << std::fixed << std::setprecision(3);
  float minThick = 0.305;
  float maxThick = 1;
  float dTdx = (maxThick - minThick) / 19;
  for (unsigned z = 0; z < outSize[2]; z++) {
    for (unsigned y = 0; y < outSize[1]; y++) {
      for (unsigned x = 0; x < outSize[0]; x++) {
        Vec3f coord(x * outdx, y*outdx,z*outdx);
        coord += origin;
        float d = sdfMesh.DistTrilinear(coord);
        float t = 0;
        t = z/float(outSize[2] - 1) ;
        t = maxThick * t + (1 - t) * minThick;
        //if (d > 0) {
        //  t = minThick;
        //} else {
        //  t = minThick + std::abs(d) * dTdx;

        //}
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

}

void OffsetMesh() {
  std::string meshFile = "F:/meshes/elliot/cushion.stl";
  std::string outFile = "F:/meshes/head/head_skinsub.obj";
  TrigMesh mesh;
  mesh.LoadStl(meshFile);
  TrigMesh rigid = GetInnerSurf(mesh, 0.5, 2);
  rigid.SaveObj(outFile);
}

void FloodSDFOutside() {
  std::string meshFile = "F:/meshes/shoe_special/shoe_rough_asm.stl";
  std::string outFile = "F:/meshes/shoe_special/shoe_march.obj";
  TrigMesh mesh;
  mesh.LoadStl(meshFile);

  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  sdf->voxSize = 0.5;
  sdf->band = 8;
  sdf->distUnit = 0.01;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  CloseExteriorAndFillInterior(sdf->dist, sdf->MAX_DIST);
  Array3D8u frozen;
  sdf->FastSweepCoarse(frozen);

  TrigMesh surf;
  MarchingCubes(sdf->dist, 0.01, sdf->distUnit, sdf->voxSize, sdf->origin, &surf);
  MergeCloseVertices(surf);
  surf.SaveObj(outFile);
}

void WriteDiskField() {
  
  std::ofstream out("F:/meshes/gradient/coaster.txt");

  float outdx = 0.5;
  float radius = 44;
  Vec3u outSize(unsigned(2 * radius / outdx) + 1,
                unsigned(2 * radius / outdx) + 1, 2);
  Vec3f origin(-radius, -radius, -radius);
  out << "origin\n" << -radius << " " << -radius << " " << -radius << "\n";
  out << "voxelSize\n" << outdx << " " << outdx << " " << 2*radius << "\n";
  out << "grid\n"
      << outSize[0] << " " << outSize[1] << " " << outSize[2] << "\n";

  out << std::fixed << std::setprecision(3);
  float minThick = 0.4;
  float maxThick = 1.85;
  float dTdx = (maxThick - minThick) / radius;  
  for (unsigned z = 0; z < outSize[2]; z++) {
    for (unsigned y = 0; y < outSize[1]; y++) {
      for (unsigned x = 0; x < outSize[0]; x++) {
        Vec3f coord(x * outdx, y * outdx, z * outdx);
        coord += origin;
        float d = coord[0] * coord[0] + coord[1] * coord[1];
        d = std::sqrt(d) / radius;
        float t = maxThick * d + (1 - d) * minThick;
        // if (d > 0) {
        //   t = minThick;
        // } else {
        //   t = minThick + std::abs(d) * dTdx;

        //}
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
  out.close();
}

void TestSDF() { 
  //OrientFlatGroups();
  //GetInnerSurf();
  //ExtrudeAlongNormal("F:/meshes/head/front.obj", 0.52);
  // ProcessNose();
  //ProcessHead();
  //ProcessFront();
  //PadGridXY();

  //MakeFrontLatticeMesh();
  //MakeSkinMesh();
  //MakeLowerSolid();
  //MakeGearMesh();
  //LoadBinVox();

  //WriteDistField();
  WriteDiskField();
  //FloodSDFOutside();
}
