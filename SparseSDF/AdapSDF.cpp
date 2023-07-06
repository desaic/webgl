#include "AdapSDF.h"
#include "MergeClosePoints.h"

AdapSDF::AdapSDF() { sparseData.resize(1); }

int AdapSDF::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  size_t numVox = sx * sy * size_t(sz);
  if (numVox > MAX_NUM_VOX) {
    return -1;
  }
  dist.Allocate(sx + 1, sy + 1, sz + 1);
  dist.Fill(MAX_DIST);

  Vec3u coarseSize(sx / 4, sy / 4, sz / 4);
  coarseSize[0] += (sx % 4 > 0);
  coarseSize[1] += (sy % 4 > 0);
  coarseSize[2] += (sz % 4 > 0);

  sparseGrid.Allocate(coarseSize[0], coarseSize[1], coarseSize[2]);
  return 0;
}

int AdapSDF::AddPoint(Vec3f point, const Vec3u &gridIdx) { 
  point = point - origin;
  Vec3u gridSize = sparseGrid.GetSize();
  if (gridIdx[0] >= gridSize[0] * 4 || gridIdx[1] >= gridSize[1] * 4 ||
      gridIdx[2] >= gridSize[2] * 4) {
    return -1;
  }
  PointSet& cell = AddSparseCell(gridIdx);
  cell.p.push_back(point);
  return 0; 
}

PointSet& AdapSDF::AddSparseCell(const Vec3u& gridIdx) {
  SparseNode4<unsigned>& node =
      GetSparseNode4(gridIdx[0], gridIdx[1], gridIdx[2]);

  if (!node.HasChildren()) {
    node.AddChildrenDense(0);
  }

  Vec3u fineIdx(gridIdx[0] & 3, gridIdx[1] & 3, gridIdx[2] & 3);
  unsigned* cellIdxPtr = node.AddChildDense(fineIdx[0], fineIdx[1], fineIdx[2]);
  unsigned cellIdx = *cellIdxPtr;
  if (cellIdx == 0) {
    // uninitialized cell
    cellIdx = unsigned(sparseData.size());
    *cellIdxPtr = cellIdx;
    sparseData.push_back(PointSet());
  }
  return sparseData[cellIdx];
}

void MergeClosePoints(PointSet& points,float eps) { std::vector<size_t> idx;
  MergeClosePoints(points.p, idx, eps);
  std::vector<Vec3f> v (idx.size());
  for (size_t i = 0; i < idx.size();i++) {
    v[i] = points.p[idx[i]];
  }
  points.p = v;
}

void AdapSDF::Compress() { auto&data = sparseGrid.GetData();
  for (auto& node: data) {
    node.Compress();
  }
  float eps = distUnit;
  totalPoints = 0;
  for (auto& cell : sparseData) {
    MergeClosePoints(cell,eps);
    totalPoints += cell.p.size();
  }
}

/// <summary>
/// Triliearly interpolates the vertex distance grid.
/// </summary>
/// <returns>distance in mm</returns>
float AdapSDF::GetCoarseDist(const Vec3f& x) const
{
  Vec3f local = x - origin;
  local = local / voxSize;
  unsigned ix = unsigned(local[0]);
  unsigned iy = unsigned(local[1]);
  unsigned iz = unsigned(local[2]);
  Vec3u gridSize = dist.GetSize();
  if (ix>=gridSize[0]-1 || iy>=gridSize[1]-1||iz>=gridSize[2]-1) {
    return MAX_DIST;
  }
  float a = (ix + 1) - local[0];
  float b = (iy + 1) - local[1];
  float c = (iz + 1) - local[2];
  //v indexed by y and z
  float v00 = a * dist(ix, iy, iz) + (1 - a) * dist(ix + 1, iy, iz);
  float v10 = a * dist(ix, iy + 1, iz) + (1 - a) * dist(ix + 1, iy + 1, iz);
  float v01 = a * dist(ix, iy, iz + 1) + (1 - a) * dist(ix + 1, iy, iz + 1);
  float v11 =
      a * dist(ix, iy + 1, iz + 1) + (1 - a) * dist(ix + 1, iy + 1, iz + 1);
  float v0 = b * v00 + (1 - b) * v10;
  float v1 = b * v01 + (1 - b) * v11;
  float v = c * v0 + (1 - c) * v1;
  return v * distUnit;
}
/// <summary>
/// taken as the min of the bilinear interpolated distance
/// and the point sample based distance.
/// </summary>
/// <returns>distance in mm</returns>
float AdapSDF::GetFineDist(const Vec3f& x) const {
  Vec3f local = x - origin;
  Vec3f idx = local / voxSize;
  unsigned ix = unsigned(idx[0]);
  unsigned iy = unsigned(idx[1]);
  unsigned iz = unsigned(idx[2]);
  Vec3u gridSize = dist.GetSize();
  if (ix >= gridSize[0] - 1 || iy >= gridSize[1] - 1 || iz >= gridSize[2] - 1) {
    return MAX_DIST;
  }
  
  const SparseNode4<unsigned>& node = GetSparseNode4(ix, iy, iz);
  float coarseDist = GetCoarseDist(x);
  if (!node.HasChild(ix & 3, iy & 3, iz & 3)) {
    return coarseDist;
  }
  unsigned pointSetIdx = *(node.GetChild(ix & 3, iy & 3, iz & 3));
  float minDist = MAX_DIST;
  const PointSet &points = sparseData[pointSetIdx];
  for (size_t i = 0; i < points.p.size(); i++) {
    Vec3f diff = local - points.p[i];
    minDist = std::min(minDist, diff.norm2());
  }
  return std::min(std::sqrt(minDist), coarseDist);
}

SparseNode4<unsigned>& AdapSDF::GetSparseNode4(unsigned x, unsigned y,
                                               unsigned z)
{
  return sparseGrid(x / 4, y / 4, z / 4);
}

const SparseNode4<unsigned>& AdapSDF::GetSparseNode4(unsigned x, unsigned y,
                                               unsigned z)const {
  return sparseGrid(x / 4, y / 4, z / 4);
}