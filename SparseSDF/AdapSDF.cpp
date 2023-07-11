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

FixedGrid5& AdapSDF::AddSparseCell(const Vec3u& gridIdx) {
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
    sparseData.push_back(FixedGrid5());
  }
  return sparseData[cellIdx];
}

bool AdapSDF::HasCellDense(const Vec3u& gridIdx) const {
  const SparseNode4<unsigned>& node =
      GetSparseNode4(gridIdx[0], gridIdx[1], gridIdx[2]);

  if (!node.HasChildren()) {
    return false;
  }

  Vec3u fineIdx(gridIdx[0] & 3, gridIdx[1] & 3, gridIdx[2] & 3);
  const unsigned* cellIdxPtr = node.GetChildDense(fineIdx[0], fineIdx[1], fineIdx[2]);
  unsigned cellIdx = *cellIdxPtr;
  return cellIdx>0;
}

bool AdapSDF::HasCellSparse(const Vec3u& gridIdx) const {
  const SparseNode4<unsigned>& node =
      GetSparseNode4(gridIdx[0], gridIdx[1], gridIdx[2]);

  if (!node.HasChildren()) {
    return false;
  }
  return node.HasChild(gridIdx[0] & 3, gridIdx[1] & 3, gridIdx[2] & 3);
}

void AdapSDF::Compress() { auto&data = sparseGrid.GetData();
  for (auto& node: data) {
    node.Compress();
  }
  float eps = distUnit;
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
  unsigned nodeIdx = *(node.GetChild(ix & 3, iy & 3, iz & 3));
  float fineDist = MAX_DIST;
  const FixedGrid5& fGrid = sparseData[nodeIdx];

  Vec3f fineCoord = local - voxSize * Vec3f(ix, iy, iz);
  const unsigned N = FixedGrid5::N;
  float fine_h = voxSize / (N - 1);
  Vec3f fineIdx = (1.0f / fine_h) * fineCoord;
  unsigned fx = unsigned(fineIdx[0]);
  unsigned fy = unsigned(fineIdx[1]);
  unsigned fz = unsigned(fineIdx[2]);
  if (fx>N-2 || fy>N-2||fz>N-2) {
    return coarseDist;
  }
  float a = (fx + 1) - fineIdx[0];
  float b = (fy + 1) - fineIdx[1];
  float c = (fz + 1) - fineIdx[2];
  float v00 = a * fGrid(fx, fy, fz) + (1 - a) * fGrid(fx + 1, fy, fz);
  float v10 = a * fGrid(fx, fy + 1, fz) + (1 - a) * fGrid(fx + 1, fy + 1, fz);
  float v01 = a * fGrid(fx, fy, fz + 1) + (1 - a) * fGrid(fx + 1, fy, fz + 1);
  float v11 =
      a * fGrid(fx, fy + 1, fz + 1) + (1 - a) * fGrid(fx + 1, fy + 1, fz + 1);
  float v0 = b * v00 + (1 - b) * v10;
  float v1 = b * v01 + (1 - b) * v11;
  float v = c * v0 + (1 - c) * v1;
  return std::min(v * distUnit, coarseDist);
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