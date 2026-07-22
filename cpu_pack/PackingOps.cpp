#include "PackingOps.h"
#include "AdapSDF.h"
#include "MeshOps.h"
#include "GridUtils.h"
#include "Array2D.h"
#include "ImageIO.h"
#include "PackingScene.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <iostream>

// from x y z index in convolved image to translation for fg mesh.
Vec3f GetDisplacement(Vec3u gridIdx, float dx, Vec3u fgSize, Vec3f fgOrigin, Vec3f bgOrigin) {
  Vec3f dxVec(dx);
  Vec3f s = fgSize.cast<float>() - Vec3f(1.0f);
  Vec3f origin = bgOrigin - fgOrigin - dxVec * s;
  Vec3f disp = dx * gridIdx.cast<float>() + origin;
  return disp;
}

bool FindSpot(MeshConvo &bg, const TrigMesh &part, Vec3f &pos, const Vec3f &rot, 
  std::shared_ptr<AdapSDF> sdf, float factor) {
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  float dx = bg.dx;

  MeshConvo fg;
  fg.SetMeshPtr(&rotated);
  fg.Voxelize(dx);
  // makes values in convo smaller.
  ThreshInPlace(fg.vox, 1);
  const unsigned FFT_ALIGNMENT = 8;
  Vec3u bgSize = bg.GridSize();
  Vec3u fgSize = fg.GridSize();
  // use circular fft/ntt. no need to pad with fg size.
  Vec3u totalSize = bgSize;
  //+fgSize;
  Vec3u gridSize = PadSizes(totalSize, FFT_ALIGNMENT);

  Utils::Stopwatch fftTimer;
  fftTimer.Start();
  bg.FFT(gridSize);

  fftTimer.Start();
  Reverse(fg.vox);
  fg.gridReversed = true;
  fg.FFT(gridSize);

  fftTimer.Start();
  Dot(bg.fft, fg.fft);

  fftTimer.Start();
  Array3Df conv = IFFT(fg.fft);
  Array3D8u collision = Quantize(conv);
  const float score0 = -1e6f;
  
  float highScore = score0;
  Vec3u bestPos(0);

  Vec3f meshCenter = fg.GetMeshCenter();

  Array2D8u debugSlice (gridSize[0], gridSize[1]);
  debugSlice.Fill(0);
  Array2D8u collSlice (gridSize[0], gridSize[1]);

  unsigned debugZ = (fgSize[2] + gridSize[2] - 1)/2;
  // add a little attraction towards bottom left.
  // using normalized x y z coordinate .
  float positionWeight = -1.0f;
  for (unsigned z = fgSize[2] - 1; z < gridSize[2]; z++) {
    for (unsigned y = fgSize[1] - 1; y < gridSize[1]; y++) {
      for (unsigned x = fgSize[0] - 1; x < gridSize[0]; x++) {
        if(z == debugZ){
          collSlice(x,y) = collision(x,y,z);
        }
        if (collision(x, y, z) > 0) {
          continue;
        }
        Vec3f disp = GetDisplacement(Vec3u(x,y,z), dx, fgSize, fg.GetOrigin(), bg.GetOrigin());
        Vec3f coord = disp + meshCenter;
    
        float dist = sdf->GetCoarseDist(coord);
        if (dist >= 32766) {
          dist = -dist;
        }
        float score = factor * dist + positionWeight * (coord[0]+ coord[1]+ coord[2]);
        if(z == debugZ){
          debugSlice(x,y) = uint8_t(score * 10);
        }
        if (score > highScore) {
          highScore = score;
          bestPos = Vec3u(x, y, z);
        }
      }
    }
  }

  (void)debugSlice;
  (void)collSlice;
  bool found = (highScore > score0);
  if (found) {    
    pos = GetDisplacement(bestPos, dx, fg.vox.GetSize(), fg.GetOrigin(), bg.GetOrigin());
  }
  return found;
}

struct CollisionGridResult {
  Array3D8u collision;
  Vec3u gridSize;
  Vec3u fgSize;
  Vec3f fgOrigin;
  Vec3f meshCenter;
  Box3f meshBox;
};

static CollisionGridResult ComputeCollisionGrid(MeshConvo &bg,
                                                const TrigMesh &part,
                                                const Vec3f &rot) {
  CollisionGridResult r;

  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  float dx = bg.dx;

  MeshConvo fg;
  fg.SetMeshPtr(&rotated);
  fg.Voxelize(dx);
  ThreshInPlace(fg.vox, 1);

  const unsigned FFT_ALIGNMENT = 8;
  Vec3u bgSize = bg.GridSize();
  r.fgSize = fg.GridSize();
  Vec3u totalSize = bgSize;

  std::array<unsigned, 3> padded = PadSizes(totalSize, FFT_ALIGNMENT);
  r.gridSize = Vec3u(padded[0], padded[1], padded[2]);

  bg.FFT(r.gridSize);

  Reverse(fg.vox);
  fg.gridReversed = true;
  fg.FFT(r.gridSize);

  Dot(bg.fft, fg.fft);

  Array3Df conv = IFFT(fg.fft);
  r.collision = Quantize(conv);
  r.fgOrigin = fg.GetOrigin();
  r.meshCenter = fg.GetMeshCenter();
  r.meshBox = fg.meshBox;
  return r;
}

static float XSignMargin(const Box3f &meshBox, int xSign) {
  if (xSign < 0) {
    return -meshBox.vmax[0];
  } else if (xSign > 0) {
    return -meshBox.vmin[0];
  }
  return 0.0f;
}

static bool PassesXSignConstraint(float dispX, float margin, int xSign) {
  if (xSign < 0) {
    return dispX < margin;
  } else if (xSign > 0) {
    return dispX > margin;
  }
  return true;
}

static bool FindSpotConstrainedImpl(MeshConvo &bg,
                                    const TrigMesh &part,
                                    Vec3f &pos,
                                    const Vec3f &rot,
                                    std::shared_ptr<AdapSDF> sdf,
                                    float factor,
                                    const PackingConstraints &constraints) {
  CollisionGridResult coll = ComputeCollisionGrid(bg, part, rot);
  Vec3u gridSize = coll.gridSize;
  Vec3u fgSize = coll.fgSize;
  Vec3f fgOrigin = coll.fgOrigin;
  Vec3f bgOrigin = bg.GetOrigin();
  Vec3f meshCenter = coll.meshCenter;
  float dx = bg.dx;

  const float score0 = -1e6f;
  float highScore = score0;
  Vec3u bestPos(0);

  Vec3f originDisp = bgOrigin - fgOrigin - Vec3f(dx) * (fgSize.cast<float>() - Vec3f(1.0f));

  unsigned xMin = fgSize[0] - 1;
  unsigned xMax = gridSize[0];
  if (constraints.lockPosX) {
    float targetXf = (constraints.fixedPosX - originDisp[0]) / dx;
    int targetX = (int)std::round(targetXf);
    int lo = std::max(int(fgSize[0] - 1), targetX - 1);
    int hi = std::min(int(gridSize[0]), targetX + 2);
    if (lo >= hi) {
      return false;
    }
    xMin = (unsigned)lo;
    xMax = (unsigned)hi;
  }

  float margin = XSignMargin(coll.meshBox, constraints.xSign);
  float positionWeightX = -float(constraints.xSign);
  float positionWeightYZ = -1.0f;

  for (unsigned z = fgSize[2] - 1; z < gridSize[2]; z++) {
    for (unsigned y = fgSize[1] - 1; y < gridSize[1]; y++) {
      for (unsigned x = xMin; x < xMax; x++) {
        if (coll.collision(x, y, z) > 0) {
          continue;
        }
        Vec3f disp = GetDisplacement(Vec3u(x, y, z), dx, fgSize, fgOrigin, bgOrigin);
        if (!PassesXSignConstraint(disp[0], margin, constraints.xSign)) {
          continue;
        }
        Vec3f coord = disp + meshCenter;
        float dist = sdf->GetCoarseDist(coord);
        if (dist >= 32766) {
          dist = -dist;
        }
        float score = factor * dist + positionWeightX * coord[0]
                      + positionWeightYZ * (coord[1] + coord[2]);
        if (score > highScore) {
          highScore = score;
          bestPos = Vec3u(x, y, z);
        }
      }
    }
  }

  bool found = (highScore > score0);
  if (found) {
    pos = GetDisplacement(bestPos, dx, fgSize, fgOrigin, bgOrigin);
  }
  return found;
}

bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         const Vec3f &rot,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints) {
  return FindSpotConstrainedImpl(bg, part, pos, rot, sdf, factor, constraints);
}

static Vec3u CellIndexTo3D(unsigned cellIdx, Vec3u numCells) {
  unsigned x = cellIdx % numCells[0];
  unsigned y = (cellIdx / numCells[0]) % numCells[1];
  unsigned z = cellIdx / (numCells[0] * numCells[1]);
  return Vec3u(x, y, z);
}

bool FindSpotSubgrid(MeshConvo &bg,
                     const TrigMesh &part,
                     Vec3f &pos,
                     const Vec3f &rot,
                     std::shared_ptr<AdapSDF> sdf,
                     float factor,
                     float cellSize,
                     unsigned cellIdx,
                     Vec3u numCells,
                     bool ignoreCellBoundary) {
  Box3f itemBox = ComputeBBox(part.v);
  Vec3f itemExtent = itemBox.vmax - itemBox.vmin;

  Vec3u cell3D = CellIndexTo3D(cellIdx, numCells);
  Vec3f containerOrigin = bg.box.vmin;
  float dx = bg.dx;

  Vec3f cellSize3(cellSize);
  Vec3f cellMin = containerOrigin + cell3D.cast<float>() * cellSize3;
  Vec3f cellMax = cellMin + cellSize3;

  Vec3f cropMin = cellMin - itemExtent;
  Vec3f cropMax = cellMax + itemExtent;
  cropMin[0] = std::max(cropMin[0], bg.box.vmin[0]);
  cropMin[1] = std::max(cropMin[1], bg.box.vmin[1]);
  cropMin[2] = std::max(cropMin[2], bg.box.vmin[2]);
  cropMax[0] = std::min(cropMax[0], bg.box.vmax[0]);
  cropMax[1] = std::min(cropMax[1], bg.box.vmax[1]);
  cropMax[2] = std::min(cropMax[2], bg.box.vmax[2]);

  Vec3u bgSize = bg.GridSize();
  Vec3i voxMin(
    (int)std::round((cropMin[0] - containerOrigin[0]) / dx),
    (int)std::round((cropMin[1] - containerOrigin[1]) / dx),
    (int)std::round((cropMin[2] - containerOrigin[2]) / dx)
  );
  Vec3i voxMax(
    (int)std::round((cropMax[0] - containerOrigin[0]) / dx),
    (int)std::round((cropMax[1] - containerOrigin[1]) / dx),
    (int)std::round((cropMax[2] - containerOrigin[2]) / dx)
  );
  for (int d = 0; d < 3; d++) {
    voxMin[d] = std::max(0, std::min(voxMin[d], (int)bgSize[d] - 1));
    voxMax[d] = std::max(0, std::min(voxMax[d], (int)bgSize[d] - 1));
  }

  Vec3u subSize(
    (unsigned)(voxMax[0] - voxMin[0] + 1),
    (unsigned)(voxMax[1] - voxMin[1] + 1),
    (unsigned)(voxMax[2] - voxMin[2] + 1)
  );
  if (subSize[0] < 4 || subSize[1] < 4 || subSize[2] < 4) {
    return false;
  }

  Array3D8u subVox;
  subVox.Allocate(subSize, 0);
  for (unsigned z = 0; z < subSize[2]; z++) {
    for (unsigned y = 0; y < subSize[1]; y++) {
      for (unsigned x = 0; x < subSize[0]; x++) {
        subVox(x, y, z) = bg.vox(
          (unsigned)(voxMin[0] + (int)x),
          (unsigned)(voxMin[1] + (int)y),
          (unsigned)(voxMin[2] + (int)z)
        );
      }
    }
  }

  MeshConvo tempConv;
  tempConv.box.vmin = cropMin;
  tempConv.box.vmax = cropMax;
  tempConv.vox = subVox;
  tempConv.dx = dx;

  Vec3f foundPos;
  bool found = FindSpot(tempConv, part, foundPos, rot, sdf, factor);
  if (!found) {
    return false;
  }

  // Verify the entire rotated fruit fits inside the container.
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  Box3f rotBox = ComputeBBox(rotated.v);
  Vec3f fruitMin = foundPos + rotBox.vmin;
  Vec3f fruitMax = foundPos + rotBox.vmax;
  const float BOUNDARY_MARGIN = 0.5f;
  if (fruitMin[0] < bg.box.vmin[0] - BOUNDARY_MARGIN ||
      fruitMin[1] < bg.box.vmin[1] - BOUNDARY_MARGIN ||
      fruitMin[2] < bg.box.vmin[2] - BOUNDARY_MARGIN ||
      fruitMax[0] > bg.box.vmax[0] + BOUNDARY_MARGIN ||
      fruitMax[1] > bg.box.vmax[1] + BOUNDARY_MARGIN ||
      fruitMax[2] > bg.box.vmax[2] + BOUNDARY_MARGIN) {
    return false;
  }

  if (!ignoreCellBoundary) {
    Vec3f rotCenter = 0.5f * (rotBox.vmin + rotBox.vmax);
    Vec3f placedCenter = foundPos + rotCenter;
    const float MARGIN = 0.0f;
    if (placedCenter[0] < cellMin[0] - MARGIN || placedCenter[0] > cellMax[0] + MARGIN ||
        placedCenter[1] < cellMin[1] - MARGIN || placedCenter[1] > cellMax[1] + MARGIN ||
        placedCenter[2] < cellMin[2] - MARGIN || placedCenter[2] > cellMax[2] + MARGIN) {
      return false;
    }
  }

  pos = foundPos;
  return true;
}