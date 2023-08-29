#include "AdapSDF.h"
#include "BBox.h"
#include "cpu_voxelizer.h"
#include "MergeClosePoints.h"
#include "FastSweep.h"
#include "TrigMesh.h"
#include "SDFVoxCb.h"
#include "Stopwatch.h"
#include <iostream>

float MinDist(const std::vector<size_t>& trigs,
              const std::vector<TrigFrame>& trigFrames, const Vec3f& query,
              const TrigMesh* mesh);

AdapSDF::AdapSDF() { fineGrid.resize(1); }

int AdapSDF::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  size_t numVox = sx * sy * size_t(sz);
  if (numVox > MAX_NUM_VOX) {
    return -1;
  }
  // 1 more vertex than voxels.
  dist.Allocate(sx+1, sy+1, sz+1);
  dist.Fill(MAX_DIST);

  Vec3u coarseSize(sx / 4, sy / 4, sz / 4);
  coarseSize[0] += (sx % 4 > 0);
  coarseSize[1] += (sy % 4 > 0);
  coarseSize[2] += (sz % 4 > 0);

  sparseGrid.Allocate(coarseSize[0], coarseSize[1], coarseSize[2]);

  trigList.resize(1);
  debugIndex.resize(1);
  return 0;
}

unsigned AdapSDF::AddDenseCell(const Vec3u& gridIdx) {
  unsigned cellIdx = sparseGrid.AddDense(gridIdx[0],gridIdx[1],gridIdx[2]);
  return cellIdx;
}

void AdapSDF::BuildTrigList(TrigMesh* mesh) { 

  mesh->ComputePseudoNormals(); 
  mesh_ = mesh;
  voxconf conf;
  conf.unit = Vec3f(voxSize, voxSize, voxSize);

  BBox box;
  ComputeBBox(mesh->v, box);
  // add margin for narrow band sdf
  band = std::min(band, AdapSDF::MAX_BAND);
  box.vmin = box.vmin - float(band) * conf.unit;
  box.vmax = box.vmax + float(band) * conf.unit;
  origin = box.vmin;
  conf.origin = origin;
  
  Vec3f count = (box.vmax - box.vmin) / conf.unit;
  conf.gridSize = Vec3u(count[0] + 1, count[1] + 1, count[2] + 1);
  Allocate(count[0], count[1], count[2]);

  TrigListVoxCb cb;
  cb.sdf = this;
  cb.m = mesh;
  Utils::Stopwatch timer;
  timer.Start();
  cpu_voxelize_mesh(conf, mesh, cb);
  float ms = timer.ElapsedMS();
  std::cout << "AdapSDF::BuildTrigList trig grid time " << ms << "\n";
}

void AdapSDF::ComputeCoarseDist() {
  if (mesh_ == nullptr) {
    return; 
  }
  size_t numTrigs = mesh_->t.size() / 3;
  trigFrames.resize(numTrigs);
  for (size_t i = 0; i < numTrigs; i++) {
    Triangle trig = mesh_->GetTriangleVerts(i);
    Vec3f n = mesh_->GetTrigNormal(i);
    ComputeTrigFrame((const float*)(&trig.v),n, trigFrames[i]);
  }

  //loop through coarse vertices 
  Vec3u size = dist.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        ComputeCoarseDist(x, y, z);
      }
    }
  }

  trigFrames.clear();
}

bool IsCorner(unsigned x, unsigned y, unsigned z, unsigned N) {
  return (x==0||x==N-1) && (y==0||y==N-1) && (z==0||z==N-1);
}

void AdapSDF::ComputeDistGrid5(Vec3f x0, FixedGrid5& fine,
                               const std::vector<size_t>& trigs) {
  const unsigned N = fine.N;
  //fine voxel size
  float h = voxSize / float(N-1);
  for (unsigned z = 0; z < N; z++) {
    for (unsigned y = 0; y < N; y++) {
      for (unsigned x = 0; x < N; x++) {
        if (IsCorner(x, y, z, N)) {
          //already computed and frozen.
          continue;
        }
        Vec3f point = x0 + h * Vec3f(x, y, z);
        float d = MinDist(trigs, trigFrames, point, mesh_);
        fine(x, y, z) = short(d / distUnit);
      }
    }
  }
}

void PrintGrid5(const FixedGrid5&grid) { const unsigned N = grid.N;
  for (unsigned z = 0; z < N; z++) {
    for (unsigned y = 0; y < N; y++) {
      for (unsigned x = 0; x < N; x++) {
        std::cout << grid(x, y, z) << ' ';
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

void AdapSDF::ComputeFineDistBrute() {
  //allocate fine cells.
  std::cout<<trigList.size()<<"\n";
  fineGrid.resize(trigList.size());
  Vec3u dsize = dist.GetSize();
  const unsigned N = FixedGrid5::N;
  //loop through fine cells.
  //number of cells is 1 - number of vertices and dist is stored on vertices.
  for (unsigned z = 0; z < dsize[2] - 1; z++) {
    for (unsigned y = 0; y < dsize[1] - 1; y++) {
      for (unsigned x = 0; x < dsize[0] - 1; x++) {
        unsigned sparseIdx = GetSparseCellIdx(x, y, z);
        // index 0 reserved for emtpy cells.
        if (sparseIdx == 0) {
          continue;
        }
        FixedGrid5& fine = fineGrid[sparseIdx];
        //corner vertices
        fine(0, 0, 0) = dist(x, y, z);
        fine(4, 0, 0) = dist(x + 1, y, z);
        fine(0, 4, 0) = dist(x, y + 1, z);
        fine(4, 4, 0) = dist(x + 1, y + 1, z);
        fine(0, 0, 4) = dist(x, y, z + 1);
        fine(4, 0, 4) = dist(x + 1, y, z + 1);
        fine(0, 4, 4) = dist(x, y + 1, z + 1);
        fine(4, 4, 4) = dist(x + 1, y + 1, z + 1);

        //find closest distance within the coarse cell
        const std::vector<size_t>& trigs = trigList[sparseIdx];
        if (trigs.size() == 0) {
          continue;
        }
        Vec3f x0 = voxSize * Vec3f(x, y, z) + origin;
        ComputeDistGrid5(x0, fine, trigs);
        if (x == 80 && y == 12 && z == 37) {
          std::cout << "debug sweep original dist\n";
          PrintGrid5(fine);
        }
      }
    }
  }

  //take min of edge vertices from diagonal neighbors, 
  //if a neighbor cell has no fine grid, skip it.
  //x
  for (unsigned z = 0; z < dsize[2] - 2; z++) {
    for (unsigned y = 0; y < dsize[1] - 2; y++) {
      for (unsigned x = 0; x < dsize[0] - 1; x++) {
        unsigned sparseIdx1 = GetSparseCellIdx(x, y, z);
        unsigned sparseIdx2 = GetSparseCellIdx(x, y + 1, z + 1);
        //compute min only if both adjace cells are present.
        if (sparseIdx1 == 0 || sparseIdx2 == 0) {
          continue;
        }
        FixedGrid5& fine1 = fineGrid[sparseIdx1];
        FixedGrid5& fine2 = fineGrid[sparseIdx2];
        for (unsigned finex = 1; finex < N - 1; finex++) {
          short val1 = fine1(finex, N - 1, N - 1);
          short val2 = fine2(finex, 0, 0);
          if (std::abs(val1) > std::abs(val2)) {
            fine1(finex, N - 1, N - 1) = val2;
          } else {
            fine2(finex, 0, 0) = val1;
          }
        }
      }
    }
  }
  //y
  for (unsigned z = 0; z < dsize[2] - 2; z++) {
    for (unsigned y = 0; y < dsize[1] - 1; y++) {
      for (unsigned x = 0; x < dsize[0] -2; x++) {
        unsigned sparseIdx1 = GetSparseCellIdx(x, y, z);
        unsigned sparseIdx2 = GetSparseCellIdx(x+1, y, z + 1);
        // compute min only if both adjace cells are present.
        if (sparseIdx1 == 0 || sparseIdx2 == 0) {
          continue;
        }
        FixedGrid5& fine1 = fineGrid[sparseIdx1];
        FixedGrid5& fine2 = fineGrid[sparseIdx2];
        for (unsigned finey = 1; finey < N - 1; finey++) {
          short val1 = fine1(N - 1,finey, N - 1);
          short val2 = fine2(0,finey, 0);
          if (std::abs(val1) > std::abs(val2)) {
            fine1(N - 1, finey, N - 1) = val2;
          } else {
            fine2(0, finey, 0) = val1;
          }
        }
      }
    }
  }
  //z
  for (unsigned z = 0; z < dsize[2] - 1; z++) {
    for (unsigned y = 0; y < dsize[1] - 2; y++) {
      for (unsigned x = 0; x < dsize[0] - 2; x++) {
        unsigned sparseIdx1 = GetSparseCellIdx(x, y, z);
        unsigned sparseIdx2 = GetSparseCellIdx(x+1, y + 1, z);
        // compute min only if both adjace cells are present.
        if (sparseIdx1 == 0 || sparseIdx2 == 0) {
          continue;
        }
        FixedGrid5& fine1 = fineGrid[sparseIdx1];
        FixedGrid5& fine2 = fineGrid[sparseIdx2];
        for (unsigned finez = 1; finez < N - 1; finez++) {
          short val1 = fine1(N - 1, N - 1, finez);
          short val2 = fine2(0, 0, finez);
          if (std::abs(val1) > std::abs(val2)) {
            fine1(N - 1, N - 1, finez) = val2;
          } else {
            fine2(0, 0, finez) = val1;
          }
        }
      }
    }
  }

  //take min of face vertices
  //yz
  for (unsigned z = 0; z < dsize[2] - 1; z++) {
    for (unsigned y = 0; y < dsize[1] - 1; y++) {
      for (unsigned x = 0; x < dsize[0] - 2; x++) {
        unsigned sparseIdx1 = GetSparseCellIdx(x, y, z);
        unsigned sparseIdx2 = GetSparseCellIdx(x + 1, y, z);
        // compute min only if both adjace cells are present.
        if (sparseIdx1 == 0 || sparseIdx2 == 0) {
          continue;
        }
        FixedGrid5& fine1 = fineGrid[sparseIdx1];
        FixedGrid5& fine2 = fineGrid[sparseIdx2];
        for (unsigned finez = 0; finez < N; finez++) {
          for (unsigned finey = 0; finey < N; finey++) {
            short val1 = fine1(N - 1, finey, finez);
            short val2 = fine2(0, finey, finez);
            if (std::abs(val1) > std::abs(val2)) {
              fine1(N - 1, finey, finez) = val2;
            } else {
              fine2(0, finey, finez) = val1;
            }
          }
        }
      }
    }
  }
  //xz
  for (unsigned z = 0; z < dsize[2] - 1; z++) {
    for (unsigned y = 0; y < dsize[1] - 2; y++) {
      for (unsigned x = 0; x < dsize[0] - 1; x++) {
        unsigned sparseIdx1 = GetSparseCellIdx(x, y, z);
        unsigned sparseIdx2 = GetSparseCellIdx(x, y+1, z);
        // compute min only if both adjace cells are present.
        if (sparseIdx1 == 0 || sparseIdx2 == 0) {
          continue;
        }
        FixedGrid5& fine1 = fineGrid[sparseIdx1];
        FixedGrid5& fine2 = fineGrid[sparseIdx2];
        for (unsigned finez = 0; finez < N; finez++) {
          for (unsigned finex = 0; finex < N; finex++) {
            short val1 = fine1(finex, N - 1, finez);
            short val2 = fine2(finex, 0, finez);
            if (std::abs(val1) > std::abs(val2)) {
              fine1(finex, N - 1, finez) = val2;
            } else {
              fine2(finex, 0, finez) = val1;
            }
          }
        }
      }
    }
  }
  //xy
  for (unsigned z = 0; z < dsize[2] - 2; z++) {
    for (unsigned y = 0; y < dsize[1] - 1; y++) {
      for (unsigned x = 0; x < dsize[0] - 1; x++) {
        unsigned sparseIdx1 = GetSparseCellIdx(x, y, z);
        unsigned sparseIdx2 = GetSparseCellIdx(x, y, z+1);
        // compute min only if both adjace cells are present.
        if (sparseIdx1 == 0 || sparseIdx2 == 0) {
          continue;
        }
        FixedGrid5& fine1 = fineGrid[sparseIdx1];
        FixedGrid5& fine2 = fineGrid[sparseIdx2];
        
        for (unsigned finey = 0; finey < N; finey++) {
          for (unsigned finex = 0; finex < N; finex++) {
            short val1 = fine1(finex, finey, N-1);
            short val2 = fine2(finex, finey, 0);
            if (std::abs(val1) > std::abs(val2)) {
              fine1(finex, finey, N - 1) = val2;
            } else {
              fine2(finex, finey, 0) = val1;
            }
          }
        }
      }
    }
  }

  //fast sweep, freeze corner cells and cells within h_fine.
  
  float h_fine = voxSize / (N - 1);
  for (unsigned z = 0; z < dsize[2] - 1; z++) {
    for (unsigned y = 0; y < dsize[1] - 1; y++) {
      for (unsigned x = 0; x < dsize[0] - 1; x++) {
        unsigned sparseIdx = GetSparseCellIdx(x, y, z);
        // index 0 reserved for emtpy cells.
        if (sparseIdx == 0) {
          continue;
        }
        FixedGrid5& fine = fineGrid[sparseIdx];
        Array3D8u frozen;
        if (x == 80 && y == 12 && z == 37) {
          std::cout << "debug sweep\n";
          PrintGrid5(fine);
        }
        frozen.Allocate(Vec3u(N, N, N), 0);
        frozen(0, 0, 0) = 1;
        frozen(N - 1, 0, 0) = 1;
        frozen(0, N - 1, 0) = 1;
        frozen(N - 1, N - 1, 0) = 1;
        frozen(0, 0, N - 1) = 1;
        frozen(N - 1, 0, N - 1) = 1;
        frozen(0, N - 1, N - 1) = 1;
        frozen(N - 1, N - 1, N - 1) = 1;
        FastSweep((short*)fine.val, N, h_fine, distUnit, band*N, frozen);
        if (x == 80 && y == 12 && z == 37) {
          std::cout << "debug after sweep\n";
          PrintGrid5(fine);
        }
      }
    }
  }
}

void AdapSDF::GatherTrigs(unsigned x, unsigned y, unsigned z,
                          std::vector<size_t>& trigs) {
  Vec3u size = sparseGrid.GetFineSize();
  if (x >= size[0] || y >= size[1] || z >= size[2]) {
    return;
  }
  unsigned idx = GetSparseCellIdx(x, y, z);
  if (idx == 0) {
    //special index empty cell
    return;
  }
  const std::vector<size_t>& cell = trigList[idx];
  trigs.insert(trigs.end(), cell.begin(), cell.end());
}

float MinDist(const std::vector<size_t>& trigs,
              const std::vector<TrigFrame> & trigFrames,
  const Vec3f & query, const TrigMesh * mesh) {
  float minDist = 1e20;
  for (size_t i = 0; i < trigs.size(); i++) {
    float px, py;
    size_t tIdx = trigs[i];
    const TrigFrame& frame = trigFrames[tIdx];
    Triangle trig = mesh->GetTriangleVerts(tIdx);
    Vec3f pv0 = query - trig.v[0];
    px = pv0.dot(frame.x);
    py = pv0.dot(frame.y);
    TrigDistInfo info =
        PointTrigDist2D(px, py, frame.v1x, frame.v2x, frame.v2y);

    Vec3f normal = mesh->GetNormal(tIdx, info.bary);
    Vec3f closest = info.bary[0] * trig.v[0] + info.bary[1] * trig.v[1] +
                    info.bary[2] * trig.v[2];

    float trigz = pv0.dot(frame.z);
    // vector from closest point to voxel point.
    Vec3f trigPt = query - closest;
    float d = std::sqrt(info.sqrDist + trigz * trigz);
    if (std::abs(minDist) > d) {
      if (normal.dot(trigPt) < 0) {
        minDist = -d;
      } else {
        minDist = d;
      }
    }
  }
  return minDist;
}

void AdapSDF::ComputeCoarseDist(unsigned x, unsigned y, unsigned z) {
  //gather triangles adjacent to this vertex.
  //using set or unordered_set is slower due to overhead.
  std::vector<size_t> trigs;
  GatherTrigs(x - 1, y - 1, z - 1, trigs);
  GatherTrigs(x, y - 1, z - 1, trigs);
  GatherTrigs(x - 1, y, z - 1, trigs);
  GatherTrigs(x, y, z - 1, trigs);
  GatherTrigs(x - 1, y - 1, z, trigs);
  GatherTrigs(x, y - 1, z, trigs);
  GatherTrigs(x - 1, y, z, trigs);
  GatherTrigs(x, y, z, trigs);
  if (trigs.size() == 0) {
    return;
  }
  Vec3f point = voxSize * Vec3f(x, y, z) + origin;
  float minDist = MinDist(trigs, trigFrames, point, mesh_);
  dist(x, y, z) = short(minDist / distUnit);
}

  bool AdapSDF::HasCellDense(const Vec3u& gridIdx) const {
  return sparseGrid.HasDense(gridIdx[0], gridIdx[1], gridIdx[2]);
}

bool AdapSDF::HasCellSparse(const Vec3u& gridIdx) const {
  return sparseGrid.HasSparse(gridIdx[0], gridIdx[1], gridIdx[2]);
}

void AdapSDF::Compress() { sparseGrid.Compress(); }

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
  const FixedGrid5& fGrid = fineGrid[nodeIdx];

  Vec3f fineCoord = local - voxSize * Vec3f(ix, iy, iz);
  const unsigned N = FixedGrid5::N;
  float fine_h = voxSize / (N - 1);

  float indexEps = 1e-3;
  Vec3f fineIdx = (1.0f / fine_h) * fineCoord;
  // handle case where point is on right wall etc.
  for (unsigned d = 0; d < 3; d++) {
    fineIdx[d] = std::min(fineIdx[d], float(N - 1));
  }
  unsigned fx = unsigned(fineIdx[0] - indexEps);
  unsigned fy = unsigned(fineIdx[1] - indexEps);
  unsigned fz = unsigned(fineIdx[2] - indexEps);  
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
  v *= distUnit;
  return v;
}

SparseNode4<unsigned>& AdapSDF::GetSparseNode4(unsigned x, unsigned y,
                                               unsigned z)
{
  return sparseGrid.GetSparseNode4(x, y, z);
}

const SparseNode4<unsigned>& AdapSDF::GetSparseNode4(unsigned x, unsigned y,
                                               unsigned z)const {
  return sparseGrid.GetSparseNode4(x, y, z);
}

unsigned AdapSDF::GetSparseCellIdx(unsigned x, unsigned y, unsigned z) const {
  const SparseNode4<unsigned>& node = GetSparseNode4(x, y, z);
  if (!node.HasChild(x & 3, y & 3, z & 3)) {
    return 0;
  }
  unsigned nodeIdx = *(node.GetChild(x & 3, y & 3, z & 3));
  return nodeIdx;
}