#include "SDF.h"
#include "MarchingCubes.h"
#include "Timer.hpp"
#include <array>
#include <queue>
// A highly scalable massively parallel fast marching method
// for the Eikonal equation 
// Jianming Yang, Frederick Stern
// This implementation only works for uniform grid with 
// unit grid length

///not really infinity. just large enough
#define INF_DIST 1e6f
///voxel index and signed distance
struct VoxDist {
  std::array<unsigned, 3> vox;
  float dist;
  VoxDist() :vox({ 0,0,0 }), dist(10000.0f) {}
  VoxDist(std::array<unsigned, 3> v, float d) :vox(v), dist(d) {}
};

struct CompareVox {
  bool operator()(const VoxDist& n1, const VoxDist& n2)
  {
    return std::abs(n1.dist) > std::abs(n2.dist);
  }
};

enum class SDFLabel {
  FAR=0,
  KNOWN,
  BAND
};

///data structures for 
struct FMStructs
{
  SDFMesh* sdf;
  GridTree<uint8_t> label;
  std::priority_queue<VoxDist, std::vector<VoxDist>, CompareVox>pq;
  FMStructs():sdf(nullptr){}
};

bool inbound(int x, int y, int z, const Vec3u & size) 
{
  return x >= 0 && x<int(size[0])
    && y >= 0 && y<int(size[1])
    && z >= 0 && z<int(size[2]);
}

void SolveQuadAxis(int x, int y, int z, unsigned axis, FMStructs* fm,
     float * dist, int * h)
{
  int d = 0;
  TreePointer labPtr(&fm->label);
  TreePointer distPtr(&fm->sdf->sdf);
  uint8_t lab = uint8_t(SDFLabel::FAR);
  const Vec3u& s = fm->label.GetSize();

  float distMinus = INF_DIST;
  float distPlus = INF_DIST;
  float minDist = INF_DIST;
  Vec3i idx(x, y, z);
  if (idx[axis] > 0) {
    idx[axis]--;
    labPtr.PointTo(idx[0], idx[1], idx[2]);
    if (labPtr.HasValue()) {
      GetVoxelValue(labPtr, lab);
      distPtr.PointTo(idx[0], idx[1], idx[2]);
    }
    if (lab == uint8_t(SDFLabel::KNOWN)) {
      GetVoxelValue(distPtr, distMinus);
      minDist = distMinus;
      d = -1;
    }
    idx[axis]++;
  }

  lab = uint8_t(SDFLabel::FAR);
  if (idx[axis] < s[axis] - 1) {
    idx[axis]++;
    labPtr.PointTo(idx[0], idx[1], idx[2]);
    if (labPtr.HasValue()) {
      GetVoxelValue(labPtr, lab);
      distPtr.PointTo(idx[0], idx[1], idx[2]);
      GetVoxelValue(distPtr, distPlus);
    }
    if (lab == uint8_t(SDFLabel::KNOWN)) {
      if (d == 0 || std::abs(distPlus) < std::abs(distMinus)) {
        minDist = distPlus;
        d = 1;
      }
    }
  }

  if (d != 0) {
    //h = 1.0/Delta x;
    h[axis] = 1;
    dist[axis] = minDist;
  }
}

float SolveQuadratic(int x, int y, int z, 
  FMStructs *fm)
{
  //read min distance in x y z directions if they exist
  float dist[3] = { 0,0,0 };
  //which direction is the min distance cell
  int h[3] = { 0,0,0 };
  SolveQuadAxis(x, y, z, 0, fm, dist, h);
  SolveQuadAxis(x, y, z, 1, fm, dist, h);
  SolveQuadAxis(x, y, z, 2, fm, dist, h);
  float a = 0, b=0, c=0;
  float psi = INF_DIST;
  if (h[0] == 0 && h[1] == 0 && h[2] == 0) {
    return psi;
  }
  for (unsigned axis = 0; axis < 3; axis++) {
    int h2 = h[axis] * h[axis];
    a += h2;
    b += h2 * std::abs(dist[axis]);
    c += h2 * dist[axis] * dist[axis];
  }
  b *= -2;
  //F{^-2}_{l,m,n} = 1
  c -= 1;

  float Delta = b * b - 4 * a * c;
  
  if (Delta < 0) {
    //no consistent distance
    //should never get here with properly initialized field.
    Delta = 0;
  }
  float psi_t = (std::sqrt(Delta) - b) / (2 * a);
  //if (dist[0] < psi_t && dist[1] < psi_t && dist[2] < psi_t) {
    psi = psi_t;
  //}
  
  return psi;
}

///input params are int to make bound check easier.
void UpdateNeighbor(int x, int y, int z, FMStructs* fm)
{
  const Vec3u& size = fm->label.GetSize();
  if (!inbound(x, y, z, size)) {
    return;
  }
  TreePointer ptr(&fm->label);
  ptr.PointTo(x, y, z);
  uint8_t lab = uint8_t(SDFLabel::FAR);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, lab);
  }
  if (lab == uint8_t(SDFLabel::KNOWN)) {
    return;
  }

  float distTemp = SolveQuadratic(x, y, z, fm);

  TreePointer distPtr(&fm->sdf->sdf);
  distPtr.PointTo(x, y, z);
  float oldDist = INF_DIST;
  if (distPtr.HasValue()) {
    GetVoxelValue(distPtr, oldDist);
  }
  if (std::abs(distTemp) >= std::abs(oldDist)) {
    return;
  }
  lab = uint8_t(SDFLabel::BAND);
  ptr.CreatePath();
  AddVoxelValue(ptr, lab);
  distPtr.CreatePath();
  AddVoxelValue(distPtr, distTemp);
  VoxDist voxDist({ { uint32_t(x),uint32_t(y), uint32_t(z) } }, distTemp);
  //pq can contain duplicates because don't
  //want to reimplement pq.
  fm->pq.push(voxDist);
}

///input params are int to make bound check easier.
///i.e. -1 will be negative
void UpdateNeighbors(int x, int y, int z, FMStructs * fm) 
{
  //human compiler
  UpdateNeighbor(x - 1, y, z, fm);
  UpdateNeighbor(x + 1, y, z, fm);
  UpdateNeighbor(x, y - 1, z, fm);
  UpdateNeighbor(x, y + 1, z, fm);
  UpdateNeighbor(x, y, z - 1, fm);
  UpdateNeighbor(x, y, z + 1, fm);
}

void InitMarch(FMStructs * fm)
{
  Vec3u s = fm->sdf->sdf.GetSize();
  TreePointer sdfPtr(&fm->sdf->sdf);
  TreePointer labPtr(&fm->label);
  const unsigned zAxis = 2;
  for (unsigned x = 0; x < s[0]; x++) {
    for (unsigned y = 0; y < s[1]; y++) {
      sdfPtr.PointTo(x, y, 0);
      for (unsigned z = 0; z < s[2]; z++) {
        if (sdfPtr.HasValue()) {
          labPtr.CreateLeaf(x, y, z);
          uint8_t lab = (uint8_t)SDFLabel::KNOWN;
          AddVoxelValue(labPtr, lab);
        }
        sdfPtr.Increment(zAxis);
      }
    }
  }
}

void InitPQ(FMStructs* fm) 
{
  TreePointer labPtr(&fm->label);
  Vec3u s = fm->sdf->sdf.GetSize();
  const unsigned zAxis = 2;
  for (unsigned x = 0; x < s[0]; x++) {
    for (unsigned y = 0; y < s[1]; y++) {
      labPtr.PointTo(x, y, 0);
      for (unsigned z = 0; z < s[2]; z++) {
        if (labPtr.HasValue()) {
          UpdateNeighbors(x, y, z, fm);
        }
        labPtr.Increment(zAxis);
      }
    }
  }
}

void MarchNarrowBand(FMStructs* fm) {
  TreePointer labPtr(&fm->label);
  TreePointer sdfPtr(&fm->sdf->sdf);
  while (!fm->pq.empty()) {
    VoxDist v = fm->pq.top();
    fm->pq.pop();

    float dist = v.dist;
    if (std::abs(dist) > fm->sdf->band) {
      continue;
    }

    labPtr.PointTo(v.vox[0], v.vox[1], v.vox[2]);
    uint8_t lab = uint8_t(SDFLabel::FAR);
    if (labPtr.HasValue()) {
      GetVoxelValue(labPtr, lab);
    }
    if (lab == uint8_t(SDFLabel::KNOWN)) {
      //duplicate
      continue;
    }
    sdfPtr.PointTo(v.vox[0], v.vox[1], v.vox[2]);
    lab = uint8_t(SDFLabel::KNOWN);
    AddVoxelValue(labPtr, lab);
    UpdateNeighbors(v.vox[0], v.vox[1], v.vox[2], fm);
  }
}

void FastMarch(SDFMesh& sdf)
{
  Vec3u gridSize = sdf.sdf.GetSize();
  FMStructs fm;
  fm.label.Allocate(gridSize[0], gridSize[1], gridSize[2]);
  fm.sdf = &sdf;
  Timer timer;
  timer.start();
  InitMarch(&fm);
  timer.end();
  float seconds = timer.getSeconds();
  std::cout << "InitMarch " << seconds << " s.\n";
  
  timer.start();
  InitPQ(&fm);
  timer.end();
  seconds = timer.getSeconds();
  std::cout << "InitPQ " << seconds << " s.\n";

  timer.start();
  MarchNarrowBand(&fm);
  timer.end();
  seconds = timer.getSeconds();
  std::cout << "March " << seconds << " s.\n";
}

void MarchingCubes(unsigned x, unsigned y, unsigned z, 
  TreePointer & ptr, SDFMesh& sdf, float level, TrigMesh* surf)
{
  GridCell cell;
  cell.p[0] = sdf.gridOrigin;
  float h = sdf.voxelSize;
  cell.p[0][0] += x * h;
  cell.p[0][1] += y * h;
  cell.p[0][2] += z * h;

  for (unsigned i = 1; i < GridCell::NUM_PT; i++) {
    cell.p[i] = cell.p[0];
  }
  cell.p[1][1] += h;

  cell.p[2][0] += h;
  cell.p[2][1] += h;

  cell.p[3][0] += h;

  cell.p[4][2] += h;

  cell.p[5][1] += h;
  cell.p[5][2] += h;

  cell.p[6][0] += h;
  cell.p[6][1] += h;
  cell.p[6][2] += h;

  cell.p[7][0] += h;
  cell.p[7][2] += h;

  GetVoxelValue(ptr, cell.val[0]);
  ptr.PointTo(x,y + 1,z);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[1]);
  }

  ptr.PointTo(x+1, y + 1, z);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[2]);
  }

  ptr.PointTo(x + 1, y , z);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[3]);
  }

  ptr.PointTo(x, y, z+1);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[4]);
  }

  ptr.PointTo(x, y + 1, z + 1);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[5]);
  }

  ptr.PointTo(x + 1, y+1, z + 1);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[6]);
  }

  ptr.PointTo(x + 1, y, z + 1);
  if (ptr.HasValue()) {
    GetVoxelValue(ptr, cell.val[7]);
  }

  MarchCube(cell, level, surf);
}

void MarchingCubes(SDFMesh& sdf, float level, TrigMesh* surf)
{
  TreePointer ptr(&sdf.sdf);
  Vec3u s = sdf.sdf.GetSize();
  const unsigned zAxis = 2;
  for (unsigned x = 0; x < s[0] - 1; x++) {
    for (unsigned y = 0; y < s[1] - 1; y++) {
      
      for (unsigned z = 0; z < s[2] - 1; z++) {
        ptr.PointTo(x, y, z);
        if (ptr.HasValue()) {
          MarchingCubes(x,y,z,ptr, sdf, level, surf);
        }
      
      }
    }
  }
}
