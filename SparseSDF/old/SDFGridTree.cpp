#include "SDFGridTree.h"
#include "heap.hpp"
#include "CPT.h"
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

void ReadVoxel(TreePointer& ptr, float &dst)
{
  if (ptr.HasValue()) {
    GetVoxelValue<float>(ptr, dst);
  }
}

float SDFGridTree::DistNN(Vec3f coord) {
  Vec3f localCoord = coord - conf.gridOrigin;
  Vec3i gridIdx;
  Vec3u size = sdf.GetSize();
  for (unsigned d = 0; d < 3; d++) {
    gridIdx[d] = int(localCoord[d] / conf.voxelSize);
    if (gridIdx[d] < 0) {
      gridIdx[d] = 0;
    }
    if (gridIdx[d] >= int(size[d]) ) {
      gridIdx[d] = int(size[d]) - 1;
    }
  }
  TreePointer ptr(&sdf);
  ptr.PointTo(gridIdx[0], gridIdx[1], gridIdx[2]);
  float dist = 1000.0f;
  ReadVoxel(ptr, dist);

  return dist * conf.voxelSize;
}

float SDFGridTree::DistTrilinear(Vec3f coord) {
  
  Vec3f localCoord = coord - conf.gridOrigin;
  //start with bottom left corner.
  Vec3i gridIdx;
  Vec3u size = sdf.GetSize();
  for (unsigned d = 0; d < 3; d++) {
    localCoord[d] = localCoord[d] / conf.voxelSize;
    gridIdx[d] = int( localCoord[d] );
    if (gridIdx[d] < 0) {
      gridIdx[d] = 0;
    }
    if (gridIdx[d] >= int(size[d])) {
      gridIdx[d] = int(size[d]) - 1;
    }
  }
  TreePointer ptr(&sdf);
  ptr.PointTo(gridIdx[0], gridIdx[1], gridIdx[2]);
  float far = 1000.f;
  float v000 = far;
  float v001 = far;
  float v011 = far;
  float v010 = far;
  float v100 = far;
  float v101 = far;
  float v111 = far;
  float v110 = far;

  TreePointer nbr = ptr;
  ReadVoxel(nbr, v000);

  nbr.Increment(0);
  ReadVoxel(nbr, v001);

  nbr.Increment(1);
  ReadVoxel(nbr, v011);

  nbr = ptr;
  nbr.Increment(1);
  ReadVoxel(nbr, v010);

  nbr = ptr;
  nbr.Increment(2);
  ReadVoxel(nbr, v100);

  nbr.Increment(0);
  ReadVoxel(nbr, v101);

  nbr.Increment(1);
  ReadVoxel(nbr, v111);

  nbr = ptr;
  nbr.Increment(1);
  nbr.Increment(2);
  ReadVoxel(nbr, v110);

  //natural coordinates in unit cube.
  float x = localCoord[0] - gridIdx[0];
  float y = localCoord[1] - gridIdx[1];
  float z = localCoord[2] - gridIdx[2];

  float v00 = v000 * (1.0f - x) + v001 * x;
  float v01 = v010 * (1.0f - x) + v011 * x;
  float v10 = v100 * (1.0f - x) + v101 * x;
  float v11 = v110 * (1.0f - x) + v111 * x;

  float v0 = v00 * (1 - y) + v01 * y;
  float v1 = v10 * (1 - y) + v11 * y;
  float dist = v0 * (1 - z) + v1 * z;
  return dist*conf.voxelSize;
}


///data structures and function arguments 
///for fast marching functions
struct FMStructs
{
  SDFGridTree* sdf;
  GridTree<uint8_t> label;
  heap h;
  //points into sdf
  TreePointer distPtr;
  //points to label
  TreePointer labelPtr;
  FMStructs():sdf(nullptr){}

  void InitPtr() {
    labelPtr.Init(&label);
    distPtr.Init(sdf->GetTree());
  }

  //some convenience functions
  void PointTo(int x, int y, int z) {
    distPtr.PointTo(x, y, z);
    labelPtr.PointTo(x, y, z);
  }
};

bool inbound(int x, int y, int z, const Vec3u & size) 
{
  return x >= 0 && x<int(size[0])
    && y >= 0 && y<int(size[1])
    && z >= 0 && z<int(size[2]);
}

int64_t linIdx(int x, int y, int z, const Vec3u size) {
  return int64_t(size[0]) * (y + int64_t(size[1]) * z) + x;
}

void gridIdx(int64_t linIdx, int &x, int& y, int& z, const Vec3u size) {
  x = linIdx % (size[0]);
  linIdx /= size[0];
  y = linIdx % size[1];
  z = linIdx / size[1];
}

void SolveQuadAxis(int x, int y, int z, unsigned axis, FMStructs* fm,
     float * dist, int * h)
{
  int d = 0;
  uint8_t lab = uint8_t(SDFLabel::FAR);
  const Vec3u& s = fm->label.GetSize();
  float distMinus = INF_DIST;
  float distPlus = INF_DIST;
  float minDist = INF_DIST;
  Vec3i idx(x, y, z);
  if (idx[axis] > 0) {
    //idx[axis]--;
    fm->labelPtr.Decrement(axis);
    if (fm->labelPtr.HasValue()) {
      GetVoxelValue(fm->labelPtr, lab);
    }
    if (lab == uint8_t(SDFLabel::KNOWN)) {
      fm->distPtr.Decrement(axis);
      GetVoxelValue(fm->distPtr, distMinus);
      minDist = distMinus;
      d = -1;
      fm->distPtr.Increment(axis);
    }
    //idx[axis]++;
    fm->labelPtr.Increment(axis);
  }

  lab = uint8_t(SDFLabel::FAR);
  if (idx[axis] < s[axis] - 1) {
    //idx[axis]++;
    //labPtr.PointTo(idx[0], idx[1], idx[2]);
    fm->labelPtr.Increment(axis);
    if (fm->labelPtr.HasValue()) {
      GetVoxelValue(fm->labelPtr, lab);
    }
    if (lab == uint8_t(SDFLabel::KNOWN)) {
      fm->distPtr.Increment(axis);
      GetVoxelValue(fm->distPtr, distPlus);
      fm->distPtr.Decrement(axis);
      if (d == 0 || std::abs(distPlus) < std::abs(distMinus)) {
        minDist = distPlus;
        d = 1;
      }
    }
    fm->labelPtr.Decrement(axis);
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
  fm->InitPtr();
  fm->PointTo(x,y,z);
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

  TreePointer distPtr(fm->sdf->GetTree());
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
  //pq can contain duplicates because don't
  //want to reimplement pq.
  //fm->pq.push(voxDist);
  int64_t index = linIdx(x, y, z, size);
  if (fm->h.isInHeap(index)) {
    fm->h.update(distTemp, index);
  }
  else {
    fm->h.insert(distTemp, index);
  }
}

///input params are int to make bound check easier.
///i.e. -1 will be negative
void UpdateNeighbors(int x, int y, int z, FMStructs * fm) 
{
  ///TODO initialize tree pointer here.
  UpdateNeighbor(x - 1, y, z, fm);
  UpdateNeighbor(x + 1, y, z, fm);
  UpdateNeighbor(x, y - 1, z, fm);
  UpdateNeighbor(x, y + 1, z, fm);
  UpdateNeighbor(x, y, z - 1, fm);
  UpdateNeighbor(x, y, z + 1, fm);
}

void InitMarch(FMStructs * fm)
{
  Vec3u s = fm->sdf->GetTree()->GetSize();
  TreePointer sdfPtr(fm->sdf->GetTree());
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
  Vec3u s = fm->sdf->GetTree()->GetSize();
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
  TreePointer sdfPtr(fm->sdf->GetTree());
  Vec3u size = fm->label.GetSize();
  int64_t loopCnt = 0;
  while (fm->h.nElems()>0) {
    float dist;
    int64_t linIdx;
    dist = fm->h.getSmallest(&linIdx);
    loopCnt++;

    int x, y, z;
    gridIdx(linIdx, x, y, z, size);
    labPtr.PointTo(x,y,z);
    uint8_t lab = uint8_t(SDFLabel::FAR);
    if (labPtr.HasValue()) {
      GetVoxelValue(labPtr, lab);
    }
    else {
      labPtr.CreatePath();
    }
    if (lab == uint8_t(SDFLabel::KNOWN)) {
      //duplicate
      continue;
    }
    lab = uint8_t(SDFLabel::KNOWN);
    AddVoxelValue(labPtr, lab);
    if (std::abs(dist) > fm->sdf->conf.band) {
      continue;
    }
    UpdateNeighbors(x,y,z, fm);
  }
  std::cout << "loop cnt " << loopCnt << "\n";
}

void FastMarch(SDFGridTree& sdf)
{
  Vec3u gridSize = sdf.GetTree()->GetSize();
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
  std::cout << "pq size " << fm.h.nElems() << "\n";
  timer.start();
  MarchNarrowBand(&fm);
  timer.end();
  seconds = timer.getSeconds();
  std::cout << "March " << seconds << " s.\n";
}

int SDFGridTree::Compute() {
  if (mesh == nullptr) {
    std::cout << "sdf no mesh.\n";
    return -1;
  }
  int ret = cpt(*this);
  if (ret < 0) {
    std::cout << "SDFGridTree cpt error " << ret << "\n";
  }
  FastMarch(*this);
  return 0;
}

void MarchOneCube(unsigned x, unsigned y, unsigned z, 
  TreePointer & ptr, const SDFImp* sdf, float level, TrigMesh* surf)
{
  GridCell cell;
  cell.p[0] = sdf->conf.gridOrigin;
  float h = sdf->conf.voxelSize;
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

void SDFGridTree::MarchingCubes(float level, TrigMesh* surf) 
{
  TreePointer ptr(&sdf);
  Vec3u s = sdf.GetSize();
  const unsigned zAxis = 2;
  for (unsigned x = 0; x < s[0] - 1; x++) {
    for (unsigned y = 0; y < s[1] - 1; y++) {      
      for (unsigned z = 0; z < s[2] - 1; z++) {
        ptr.PointTo(x, y, z);
        if (ptr.HasValue()) {
          MarchOneCube(x, y, z, ptr, (SDFImp*)this, level, surf);
        }      
      }
    }
  }
}
