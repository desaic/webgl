#include "PackingOps.h"
#include "AdapSDF.h"
#include "MeshOps.h"
#include "GridUtils.h"
#include "Array2D.h"
#include "ImageIO.h"
#include "PackingScene.h"
#include "Profiler.h"
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

// DEFAULT: sdf distance plus the fixed corner pull, exactly as before.
// POCKET: whatever the caller's scorer says, disp/coord folded into one
// comparable float (FindSpot's scan only tracks a single running best).
static float ScoreCandidate(const Vec3f &disp, const Vec3f &coord, const SpotScore &score) {
  if (score.mode == SpotScoreMode::POCKET) {
    if (!score.scorer) {
      return -1e6f;
    }
    return score.scorer(disp, coord);
  }
  float dist = score.sdf->GetCoarseDist(coord);
  if (dist >= 32766) {
    dist = -dist;
  }
  return score.factor * dist + score.positionWeight * (coord[0] + coord[1] + coord[2]);
}

bool FindSpot(MeshConvo &bg, const TrigMesh &part, Vec3f &pos, const Vec3f &rot,
             const SpotScore &score) {
  PROFILE_SCOPE("findspot.total");
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  float dx = bg.dx;

  MeshConvo fg;
  fg.SetMeshPtr(&rotated);
  {
    PROFILE_SCOPE("findspot.fg_voxelize");
    fg.Voxelize(dx);
    // makes values in convo smaller.
    ThreshInPlace(fg.vox, 1);
  }
  const unsigned FFT_ALIGNMENT = 8;
  Vec3u bgSize = bg.GridSize();
  Vec3u fgSize = fg.GridSize();
  // use circular fft/ntt. no need to pad with fg size.
  Vec3u totalSize = bgSize;
  //+fgSize;
  Vec3u gridSize = PadSizes(totalSize, FFT_ALIGNMENT);

  {
    // recomputed on every trial even though bg only changes after a Put.
    PROFILE_SCOPE("findspot.bg_fft");
    bg.FFT(gridSize);
  }

  {
    PROFILE_SCOPE("findspot.fg_fft");
    Reverse(fg.vox);
    fg.gridReversed = true;
    fg.FFT(gridSize);
  }

  {
    PROFILE_SCOPE("findspot.dot");
    Dot(bg.fft, fg.fft);
  }

  Array3Df conv;
  {
    PROFILE_SCOPE("findspot.ifft");
    conv = IFFT(fg.fft);
  }
  Array3D8u collision;
  {
    PROFILE_SCOPE("findspot.quantize");
    collision = Quantize(conv);
  }
  const float score0 = -1e6f;
  
  float highScore = score0;
  Vec3u bestPos(0);

  Vec3f meshCenter = fg.GetMeshCenter();

  Array2D8u debugSlice (gridSize[0], gridSize[1]);
  debugSlice.Fill(0);
  Array2D8u collSlice (gridSize[0], gridSize[1]);

  unsigned debugZ = (fgSize[2] + gridSize[2] - 1)/2;
  PROFILE_SCOPE("findspot.score_scan");
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

        float s = ScoreCandidate(disp, coord, score);
        if(z == debugZ){
          debugSlice(x,y) = uint8_t(s * 10);
        }
        if (s > highScore) {
          highScore = s;
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

bool FindSpot(MeshConvo &bg, const TrigMesh &part, Vec3f &pos, const Vec3f &rot,
             std::shared_ptr<AdapSDF> sdf, float factor) {
  SpotScore score;
  score.mode = SpotScoreMode::DEFAULT;
  score.sdf = sdf;
  score.factor = factor;
  return FindSpot(bg, part, pos, rot, score);
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

bool FindSpotBox(MeshConvo &bg,
                 const TrigMesh &part,
                 Vec3f &pos,
                 const Vec3f &rot,
                 const SpotScore &score,
                 const Box3f &searchBox,
                 float shrink,
                 Vec3f *outCenter) {
  PROFILE_SCOPE("findspot_box.total");
  Box3f itemBox = ComputeBBox(part.v);
  Vec3f itemExtent = itemBox.vmax - itemBox.vmin;
  float maxExtent = std::max({itemExtent[0], itemExtent[1], itemExtent[2]});

  // Shrink small fruits for the FFT to find spots more easily. shrink=0.5
  // matches the historical FindSpotSubgrid behavior; the pocket path uses
  // a smaller value since this reports a fit the real (unshrunk) berry may
  // not have.
  TrigMesh const *partPtr = &part;
  TrigMesh shrunk;
  if (maxExtent < 5.0f && maxExtent > shrink) {
    float scale = (maxExtent - shrink) / maxExtent;
    Vec3f center = 0.5f * (itemBox.vmin + itemBox.vmax);
    shrunk = part;
    for (size_t i = 0; i < shrunk.v.size(); i += 3) {
      shrunk.v[i]   = center[0] + (shrunk.v[i]   - center[0]) * scale;
      shrunk.v[i+1] = center[1] + (shrunk.v[i+1] - center[1]) * scale;
      shrunk.v[i+2] = center[2] + (shrunk.v[i+2] - center[2]) * scale;
    }
    partPtr = &shrunk;
    itemBox = ComputeBBox(shrunk.v);
    itemExtent = itemBox.vmax - itemBox.vmin;
  }

  Vec3f containerOrigin = bg.box.vmin;
  float dx = bg.dx;

  Vec3f cropMin = searchBox.vmin - itemExtent;
  Vec3f cropMax = searchBox.vmax + itemExtent;
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
  {
  PROFILE_SCOPE("findspot_box.crop");
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
  }

  MeshConvo tempConv;
  tempConv.box.vmin = cropMin;
  tempConv.box.vmax = cropMax;
  tempConv.vox = subVox;
  tempConv.dx = dx;

  Vec3f foundPos;
  bool found = FindSpot(tempConv, *partPtr, foundPos, rot, score);
  if (!found) {
    return false;
  }

  // Verify the entire rotated fruit fits inside the container.
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = *partPtr;
  TransformVerts(partPtr->v, rotated.v, rotMat);
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

  if (outCenter) {
    Vec3f rotCenter = 0.5f * (rotBox.vmin + rotBox.vmax);
    *outCenter = foundPos + rotCenter;
  }

  pos = foundPos;
  return true;
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
  PROFILE_SCOPE("findspot_subgrid.total");
  Vec3u cell3D = CellIndexTo3D(cellIdx, numCells);
  Vec3f containerOrigin = bg.box.vmin;

  Vec3f cellSize3(cellSize);
  Vec3f cellMin = containerOrigin + cell3D.cast<float>() * cellSize3;
  Vec3f cellMax = cellMin + cellSize3;

  Box3f searchBox;
  searchBox.vmin = cellMin;
  searchBox.vmax = cellMax;

  SpotScore score;
  score.mode = SpotScoreMode::DEFAULT;
  score.sdf = sdf;
  score.factor = factor;

  Vec3f foundPos, settledCenter;
  if (!FindSpotBox(bg, part, foundPos, rot, score, searchBox, 0.5f, &settledCenter)) {
    return false;
  }

  if (!ignoreCellBoundary) {
    const float MARGIN = 0.0f;
    if (settledCenter[0] < cellMin[0] - MARGIN || settledCenter[0] > cellMax[0] + MARGIN ||
        settledCenter[1] < cellMin[1] - MARGIN || settledCenter[1] > cellMax[1] + MARGIN ||
        settledCenter[2] < cellMin[2] - MARGIN || settledCenter[2] > cellMax[2] + MARGIN) {
      return false;
    }
  }

  pos = foundPos;
  return true;
}