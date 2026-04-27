#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "FloodOutside.h"
#include "Matrix3f.h"
#include "cpu_voxelizer.h"
#include "pocketfft_3df.h"
#include "TrigGrid.h"
#include <iostream>
#include <fstream>

#include "Stopwatch.h"
#include "meshutil.h"
#include "AdapSDF.h"
#include "AdapUDF.h"
#include "SDFMesh.h"
#include "MarchingCubes.h"
#include "FastSweep.h"
#include "PointTrigDist.h"
#include "BBox.h"

#include <algorithm>
#include <numeric>
#include <deque>
#include <cctype>
#include <cstring>
#include <random>
#include <thread>
#include <filesystem>
#include "ImageIO.h"
namespace fs = std::filesystem;

Vec3f closestPointTriangle(const Vec3f & p, const Vec3f & a, const Vec3f & b, const Vec3f & c);

unsigned PadSize(unsigned s, unsigned alignment) {
  if ((s > 0) && (s % alignment == 0)) {
    return s;
  }
  return s + alignment - s % alignment;
}

std::array<unsigned, 3> PadSizes(Vec3u &s, unsigned alignment) {
  return {PadSize(s[0], alignment), PadSize(s[1], alignment), PadSize(s[2], alignment)};
}

void VoxelizeMesh(const TrigMesh &m, Array3D8u &grid, VoxConf conf) {
  grid.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &m, grid);
}

std::array<unsigned, 3> ComputeGridSize(const Box3f &box, float dx, unsigned alignment) {
  Vec3f boxSize = box.vmax - box.vmin;
  std::array<unsigned,3> size;
  for (unsigned d = 0; d < 3; d++) {
    size[d] = unsigned(boxSize[d] / dx) + 2;
    size[d] = PadSize(size[d], alignment);
  }
  return size;
}

static std::array<float, 3> ToArray(const Vec3f &v) {
  return {v[0], v[1], v[2]};
}

Vec3f AlignOriginToGrid(const Vec3f &o, float dx) {
  Vec3f aligned;
  for (unsigned d = 0; d < 3; d++) {
    aligned[d] = std::floor(o[d] / dx) * dx;
  }
  return aligned;
}

void Dot(const Array3D<std::complex<float>> &src, Array3D<std::complex<float>> &dst) {
  Vec3u size = src.GetSize();
  if (size != dst.GetSize()) {
    return;
  }
  const auto &s = src.GetData();
  for (size_t i = 0; i < s.size(); i++) {
    dst.GetData()[i] *= s[i];
  }
}

void Scale(Array3Df &arr, float scale) {
  for (auto &f : arr.GetData()) {
    f *= scale;
  }
}

// assumes input size is even.
Array3Df IFFT(const Array3D<std::complex<float>> &coeff) {
  Vec3u size = coeff.GetSize();
  pocketfft::shape_t realSize(3);
  realSize[0] = size[0];
  realSize[1] = size[1];
  realSize[2] = (size[2] - 1) * 2;
  Array3Df outf(realSize[0], realSize[1], realSize[2]);
  unsigned numThreads = std::thread::hardware_concurrency();
  pocketfft::c2r_3df(realSize, coeff.DataPtr(), outf.DataPtr(), numThreads);
  float scale = 1.0f / (float(size[0]) * size[1] * realSize[2]);
  Scale(outf, scale);
  return outf;
}

static int LoadMesh(TrigMesh &m, const fs::path & p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(),ext.begin(), [](unsigned char c) { return std::tolower(c); });
  int ret = -1;
  if (ext == ".stl") {
    ret = m.LoadStl(p.string());
  } else if (ext == ".obj") {
    ret = m.LoadObj(p.string());
  } else {
    return -1;
  }
  return ret;
}

// data for computing convolution for a mesh.
class MeshConvo {
  public:
    // can be bigger than actual mesh box
    // box of the grid in world space.
    Box3f box;
    // original boundingbox for the mesh
    Box3f meshBox;
    Array3D8u vox;
    Array3D<std::complex<float>> fft;
    float dx = 1e-3;

    // for linear convolution,
    // the voxel grid is temporarily padded to the
    // sum of two grid sizes.
    Vec3u paddedSize;
    bool gridReversed = false;
    // allocates and fills vox also updates box.
    // does nothing if mesh is null.
    void Voxelize(float voxelSize) {
      if (mesh == nullptr) {
        return;
      }
      dx = voxelSize;
      meshBox = ComputeBBox(mesh->v);
      box = meshBox;
      box.vmin = box.vmin - Vec3f(dx);
      box.vmin = AlignOriginToGrid(box.vmin, dx);
      VoxConf conf;
      conf.origin = ToArray(box.vmin);
      conf.unit = {dx, dx, dx};

      const unsigned FFT_ALIGNMENT = 8;
      conf.gridSize = ComputeGridSize(box, dx, FFT_ALIGNMENT);
      VoxelizeMesh(*mesh, vox, conf);
      FloodOutside8u(vox, 1);
    }
    void SetMeshPtr(TrigMesh *m) {
      mesh = m;
    }
    TrigMesh *GetMesh() {
      return mesh;
    }

    Vec3f GetOrigin() const {
      return box.vmin;
    }

    Vec3u GridSize() const {
      return vox.GetSize();
    }
    Array3D8u PadVox(Vec3u newSize) const {
      Vec3u size = GridSize();
      // new size much >= old size in all dimensions
      if (newSize[0] < size[0] || newSize[1] < size[1] || newSize[2] < size[2]) {
        return Array3D8u();
      }
      Array3D8u padded(newSize, 0);
      for (unsigned z = 0; z < size[2]; z++) {
        for (unsigned y = 0; y < size[1]; y++) {
          const uint8_t *srcRow = (const uint8_t *)(&vox(0, y, z));
          uint8_t *dstRow = &padded(0, y, z);
          size_t len = size[0];
          std::memcpy(dstRow, srcRow, len);
        }
      }
      return padded;
    }
    // since we only care about collision, all images are integer valued.
    // we should use NTT instead and always pad to powers of 2 up to 1024.
    // should limit to binary values to prevent overflow of coefficients to > 2^32
    // can do check for min(pixel_count1, pixel_count2)
    // if too many pixels in both , the result is invalid.
    void FFT(Vec3u paddedSize) {
      Array3D8u padded = PadVox(paddedSize);

      Vec3u complexSize = paddedSize;
      complexSize[2] = paddedSize[2] / 2 + 1;
      fft.Allocate(complexSize, 0);
      size_t nthreads = std::thread::hardware_concurrency();
      pocketfft::shape_t shape_in(3);
      shape_in[0] = paddedSize[0];
      shape_in[1] = paddedSize[1];
      shape_in[2] = paddedSize[2];
      pocketfft::r2c_3d8u(shape_in, padded.DataPtr(), fft.DataPtr(), nthreads);
    }
    // some distance metric.
    Array3D8u dist;

  private:
    TrigMesh *mesh = nullptr;
};

// set values > thresh to 1
Array3D8u Thresh(const Array3Df &f, float thresh) {
  Vec3u size = f.GetSize();
  Array3D8u binArray(size, 0);
  const auto &fVec = f.GetData();
  auto &binVec = binArray.GetData();
  for (size_t i = 0; i < fVec.size(); i++) {
    binVec[i] = fVec[i] > thresh;
  }
  return binArray;
}

// quantize and clamps to 0 to 255
Array3D8u Quantize(const Array3Df &f) {
  Vec3u size = f.GetSize();
  Array3D8u binArray(size, 0);
  const auto &fVec = f.GetData();
  auto &binVec = binArray.GetData();
  for (size_t i = 0; i < fVec.size(); i++) {
    float val = std::clamp(fVec[i], 0.0f, 255.0f);
    binVec[i] = uint8_t(val);
  }
  return binArray;
}

Array3D8u ThreshLessThan(const Array3Df &f, float thresh) {
  Vec3u size = f.GetSize();
  Array3D8u binArray(size, 0);
  const auto &fVec = f.GetData();
  auto &binVec = binArray.GetData();
  for (size_t i = 0; i < fVec.size(); i++) {
    binVec[i] = fVec[i] < thresh;
  }
  return binArray;
}

// reversing in x y z axis is the same as reverse linear array.
void Reverse(Array3D8u &arr) {
  std::reverse(arr.GetData().begin(), arr.GetData().end());
}

static Vec3f ToFloat(const Vec3u &v) {
  return Vec3f(v[0], v[1], v[2]);
}

static Vec3u ToVec3u(const Vec3f &fvec) {
  return Vec3u(unsigned(fvec[0]), unsigned(fvec[1]), unsigned(fvec[2]));
}

static bool InBound(Vec3f x, Vec3u size) {
  return x[0] >= 0 && x[1] >= 0 && x[2] >= 0 && x[0] < size[0] && x[1] < size[1] && x[2] < size[2];
}

static bool InBound(Vec3u x, Vec3u size) {
  return x[0] < size[0] && x[1] < size[1] && x[2] < size[2];
}

// find a path of descending voxel values to outside using manhattan distance
std::vector<Vec3u> BFSPathToOutside(Vec3u start, const Array3D8u &vox) {

  std::deque<Vec3u> queue;
  Vec3u size = vox.GetSize();
  unsigned maxSize = std::max(std::max(size[0], size[1]), size[2]);
  // distance to source, initialized to large value.
  float MAX_DIST = 2 * maxSize;
  Array3D<unsigned> dist(size, MAX_DIST);
  dist(start[0], start[1], start[2]) = 0;
  queue.push_back(start);
  const unsigned NUM_NBR = 6;
  const int nbr[NUM_NBR][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
  Vec3u endPoint = start;
  unsigned pathLen = 0;
  while (!queue.empty()) {
    Vec3u index = queue.front();
    queue.pop_front();
    Vec3i indexInt = index.cast<int>();
    unsigned currentDist = dist(index[0], index[1], index[2]);
    unsigned newDist = currentDist + 1;
    // collision value of this voxel
    uint8_t collVal = vox(index[0], index[1], index[2]);
    bool finished = false;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      Vec3i nbrInt = indexInt + Vec3i(nbr[ni][0], nbr[ni][1], nbr[ni][2]);
      Vec3u nbrUint = nbrInt.cast<unsigned>();
      if (!InBound(nbrUint, size)) {
        endPoint = index;
        pathLen = dist(index[0], index[1], index[2]);
        finished = true;
        break;
      }
      uint8_t nbrVal = vox(nbrUint[0], nbrUint[1], nbrUint[2]);
      // only look at neighbors with less collision values.
      if (nbrVal >= collVal && nbrVal > 0) {
        continue;
      }
      unsigned d = dist(nbrUint[0], nbrUint[1], nbrUint[2]);
      if (d > newDist) {
        dist(nbrUint[0], nbrUint[1], nbrUint[2]) = newDist;
        queue.push_back(nbrUint);
      }
    }
    if (finished) {
      break;
    }
  }
  // back track contruct path

  std::vector<Vec3u> path(1, endPoint);
  Vec3u index = endPoint;
  for (unsigned l = 0; l < pathLen; l++) {
    Vec3u minNbr = index;
    unsigned minDist = dist(index[0], index[1], index[2]);

    // found source
    if (minDist == 0) {
      break;
    }
    Vec3i indexInt = index.cast<int>();
    bool found = true;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      Vec3i nbrInt = indexInt + Vec3i(nbr[ni][0], nbr[ni][1], nbr[ni][2]);
      Vec3u nbrUint = nbrInt.cast<unsigned>();
      if (!InBound(nbrUint, size)) {
        continue;
      }
      unsigned nbrDist = dist(nbrUint[0], nbrUint[1], nbrUint[2]);
      if (nbrDist < minDist) {
        found = true;
        minDist = nbrDist;
        minNbr = nbrUint;
      }
    }
    if (!found) {
      // should never happen.
      break;
    }
    path.push_back(minNbr);
    index = minNbr;
  }
  if (endPoint != start) {
    path.push_back(start);
  }
  std::reverse(path.begin(), path.end());
  return path;
}

void SaveSlice(const std::string &filename, const Array3D8u &arr, unsigned z, float scale) {
  Vec3u size = arr.GetSize();
  Array2D8u image(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      image(x, y) = uint8_t(scale * arr(x, y, z));
    }
  }
  SavePngGrey(filename, image);
}

void Invert01(Array3D8u &vox) {
  for (auto &val : vox.GetData()) {
    val = val > 0 ? 0 : 1;
  }
}

Matrix3f RotationMatrixRad(float rx, float ry, float rz) {
  Matrix3f mat;
  mat = Matrix3f::rotateX(rx);
  mat = mat * Matrix3f::rotateY(ry);
  mat = mat * Matrix3f::rotateZ(rz);
  return mat;
}
void TransformVerts(const std::vector<float> &verts, std::vector<float> &dst, const Matrix3f mat) {
  size_t nV = verts.size() / 3;
  dst.resize(verts.size());
  for (size_t i = 0; i < nV; i++) {
    Vec3f v0(verts[3 * i], verts[3 * i + 1], verts[3 * i + 2]);
    Vec3f v1 = mat * v0;
    dst[3 * i] = v1[0];
    dst[3 * i + 1] = v1[1];
    dst[3 * i + 2] = v1[2];
  }
}

void Union(MeshConvo &bg, Vec3i offset, const MeshConvo &fg) {
  Vec3u size = fg.GridSize();
  Vec3u bgSize = bg.GridSize();
  for (unsigned z = 0; z < size[2]; z++) {
    int dstz = int(z) + offset[2];
    if (dstz < 0 || dstz >= int(bgSize[2])) {
      continue;
    }
    for (unsigned y = 0; y < size[1]; y++) {
      int dsty = int(y) + offset[1];
      if (dsty < 0 || dsty >= int(bgSize[1])) {
        continue;
      }
      for (unsigned x = 0; x < size[0]; x++) {
        int dstx = int(x) + offset[0];
        if (dstx < 0 || dstx >= int(bgSize[0])) {
          continue;
        }
        if (fg.vox(x, y, z) > 0) {
          bg.vox(dstx, dsty, dstz) = 1;
        }
      }
    }
  }
}

void UnionReversed(MeshConvo &bg, Vec3i offset, const MeshConvo &fg) {
  Vec3u size = fg.GridSize();
  Vec3u bgSize = bg.GridSize();
  for (unsigned z = 0; z < size[2]; z++) {
    int dstz = int(z) + offset[2];
    if (dstz < 0 || dstz >= int(bgSize[2])) {
      continue;
    }
    for (unsigned y = 0; y < size[1]; y++) {
      int dsty = int(y) + offset[1];
      if (dsty < 0 || dsty >= int(bgSize[1])) {
        continue;
      }
      for (unsigned x = 0; x < size[0]; x++) {
        int dstx = int(x) + offset[0];
        if (dstx < 0 || dstx >= int(bgSize[0])) {
          continue;
        }
        if (fg.vox(size[0] - x - 1, size[1] - y - 1, size[2]- z- 1) > 0) {
          bg.vox(dstx, dsty, dstz) = 1;
        }
      }
    }
  }
}

bool Add(MeshConvo &bg, const TrigMesh &part, Vec3f &pos, const Vec3f &rot) {
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  float dx = bg.dx;

  MeshConvo fg;
  fg.SetMeshPtr(&rotated);
  fg.Voxelize(dx);
  const unsigned FFT_ALIGNMENT = 8;
  FloodOutside8u(fg.vox, 1);
  Vec3u bgSize = bg.GridSize();
  Vec3u fgSize = fg.GridSize();
  // use circular fft/ntt. no need to pad with fg size.
  Vec3u totalSize = bgSize;
  //+fgSize;
  std::cout << "total grid size " << totalSize[0] << " " << totalSize[1] << " " << totalSize[2] << "\n";
  Vec3u gridSize = PadSizes(totalSize, FFT_ALIGNMENT);

  bg.FFT(gridSize);
  Reverse(fg.vox);
  fg.gridReversed = true;
  fg.FFT(gridSize);
  Dot(bg.fft, fg.fft);
  Array3Df conv = IFFT(fg.fft);
  Array3D8u collision = Quantize(conv);
  float highScore = -1;
  Vec3u bestPos(0);

  Vec3f center = (gridSize.cast<float>() + fgSize.cast<float>());
  center = 0.5f * center - Vec3f(1);
  float maxDist = gridSize.cast<float>().norm();
  // index from 0 to fgSize-2 are outside of the container.
  // fgSize -1 is inside the container.

  for (unsigned z = fgSize[2] - 1; z < gridSize[2]; z++) {
    for (unsigned y = fgSize[1] - 1; y < gridSize[1]; y++) {
      for (unsigned x = fgSize[0] - 1; x < gridSize[0]; x++) {
        if (collision(x, y, z) > 0) {
          continue;
        }
        float dx = float(x) - center[0];
        float dy = float(y) - center[1];
        float dz = float(z) - center[2];
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float score = maxDist - dist;
        if (score > highScore) {
          highScore = score;
          bestPos = Vec3u(x, y, z);
        }
      }
    }
  }
  bool found = highScore >= 0;
  if (found) {
    Vec3f dxVec(dx);
    Vec3f s = fg.GridSize().cast<float>() - Vec3f(1.0f);
    Vec3f origin = bg.GetOrigin() - fg.GetOrigin() - dxVec * s;
    pos = dx * bestPos.cast<float>() + origin;
    Vec3i offset = bestPos.cast<int>();
    Vec3i fgSizeInt = fgSize.cast<int>();
    offset = offset - fgSizeInt + Vec3i(1);
    if (fg.gridReversed) {
      UnionReversed(bg, offset, fg);
    } else {
      Union(bg, offset, fg);
    }
  }
  return found;
}

void TestPack() {
  std::string dataFolder = "F:/meshes/fruit_hand/";
  std::string outputFolder = "F:/meshes/fruit_hand/output/";
  std::string bgFile = dataFolder + "/hands/hand_120cm.obj";
  std::string fruitFiles[] = {"pineapple.obj", "canteloupe1.obj", "strawberries1.obj", "blueberries1.obj"};
  size_t numFruits = std::size(fruitFiles);
  TrigMesh bgMesh;
  std::vector<TrigMesh> fruits(numFruits);
  std::vector<Vec2f> uv;
  bgMesh.LoadObj(bgFile);
  for (size_t i = 0; i < numFruits; i++) {
    fruits[i].LoadObj(dataFolder + "/fruits_scale/" + fruitFiles[i]);
  }
  if (bgMesh.GetNumTrigs() == 0) {
    std::cout << "empty base mesh\n";
    return;
  }
  if (fruits[0].GetNumTrigs() == 0) {
    std::cout << "empty component mesh\n";
    return;
  }
  const float dx = 0.3;
  MeshConvo bg;
  bg.SetMeshPtr(&bgMesh);
  bg.Voxelize(dx);

  FloodOutside8u(bg.vox, 1);
  Invert01(bg.vox);
  // BFSBorderDist(bg);

  Vec3f dxVec(dx);
  // SaveVoxMesh(outputFolder + "hand_vox.obj", bg.vox, dxVec, bg.GetOrigin(), 1);
  const unsigned NUM_COPIES = 100;
  const unsigned ANGLE_TRIALS = 5;
  std::vector<Vec3f> randAngles(100);
  unsigned int seed = 123;
  std::mt19937 engine(seed);
  std::uniform_real_distribution<float> dist(0.0f, 6.2831853);

  for (size_t i = 0; i < randAngles.size(); i++) {
    randAngles[i][0] = dist(engine);
    randAngles[i][1] = dist(engine);
    randAngles[i][2] = dist(engine);
  }
  std::ofstream out(outputFolder + "/pack.txt");
  bool debugging = true;
  Vec3f voxRes(bg.dx);
  Vec3f origin = bg.GetOrigin();
  for (size_t i = 0; i < numFruits; i++) {
    unsigned numTrials = 0;
    size_t placedCount = 0;
    for (size_t j = 0; j < NUM_COPIES; j++) {
      Vec3f pos, rot = randAngles[j % randAngles.size()];
      bool success = Add(bg, fruits[i], pos, rot);
      if (!success) {
        numTrials++;
        if (numTrials > 10) {
          break;
        }        
        continue;
      }
     
      out << fruitFiles[i] << " " << pos[0] << " " << pos[1] << " " << pos[2] << " " << rot[0] << " " << rot[1] << " "
          << rot[2] << "\n";
      if (debugging) {
        TrigMesh out = fruits[i];
        Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
        TransformVerts(fruits[i].v, out.v, rotMat);
        out.translate(pos[0], pos[1], pos[2]);
        std::string debugMesh = outputFolder + "/" + fruitFiles[i] + std::to_string(placedCount) + ".obj";
        out.SaveObj(debugMesh);
        if (i == 1) {
          std::string debugVol = outputFolder + "/vol_" + std::to_string(i) + "_" + std::to_string(j) + ".obj";
          SaveVolAsObjMesh(debugVol, bg.vox, (float *)(&voxRes), (float *)(&origin), 1);
        }
      }
      placedCount++;
    }
  }
  out.close();
}

std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh &mesh) {
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  sdf->voxSize = h;
  sdf->band = 16;
  sdf->distUnit = distUnit;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  CloseExterior(sdf->dist, sdf->MAX_DIST);
  Array3D8u frozen;
  //sdf->FastSweepCoarse(frozen);
  int band = 1000;
  FastSweepPar(sdf->dist, sdf->voxSize, distUnit, band, frozen);
  return sdf;
}

void MakeInnerMesh() {
  TrigMesh container;
  container.LoadStl("F:/meshes/fruit_hand/hands/hand4.5m.stl");
  float dx = 2.0f;
  float distUnit = 0.01f * dx;
  std::shared_ptr<AdapSDF> sdf = ComputeSDF(distUnit, dx, container);
  //auto outside = FloodOutside(sdf->dist, 0);
  Box3f box = ComputeBBox(container.v);
  Vec3f boxSize = box.vmax - box.vmin;
  float maxLen = boxSize.norm() * 2;
  //for (size_t i = 0; i < outside.GetData().size(); i++) {
  //  short &d = sdf->dist.GetData()[i];
  //  if (!outside.GetData()[i] && d > 0) {
  //    d = -d;
  //  }
  //  float len = d * distUnit;
  //  if (len < -maxLen) {
  //    len = -maxLen;
  //    d = std::round(len / distUnit);
  //  }    
  //}
  TrigMesh surf;
  MarchingCubes(sdf->dist, -32, sdf->distUnit, sdf->voxSize, sdf->origin, &surf);
  surf.SaveObj("F:/meshes/fruit_hand/hand4.5m_inner.obj");
}

void CenterMeshes(const std::string & inDir, const std::string & outDir) {
  
  fs::path inPath(inDir);

  if (!fs::exists(inPath) || !fs::is_directory(inPath)) {
    std::cout << inPath.string() << " not a valid directory\n";
    return;
  }
  for (const auto &entry : fs::directory_iterator(inPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::cout << entry.path().filename() << "\n";
    fs::path p = entry.path();
    std::cout << "Full Filename: " << p.filename() << "\n";  
    std::cout << "Stem (Name):   " << p.stem() << "\n";     
    std::cout << "Extension:     " << p.extension() << "\n"; 
      
    std::string stem = p.stem().string();
    if (stem.ends_with("_m")) {
      //skip mirrored meshes.
      continue;
    }
    TrigMesh mesh;
    int ret = LoadMesh(mesh, p);
    if (ret != 0) {
      std::cout << " error " << ret << " loading " << p.filename().string() << "\n";
      continue;
    }
    Box3f box = ComputeBBox(mesh.v);
    Vec3f c = 0.5f * (box.vmax + box.vmin);
    mesh.translate(-c[0], -c[1], -c[2]);
    fs::path outPath = fs::path(outDir) / p.filename();
    mesh.SaveObj(outPath.string());
  }
  
}

class MeshInfo {
public:
  Box3f box;
  std::string name;
  TrigMesh mesh;
  float BoxDiagonal() const {
    return (box.vmax - box.vmin).norm();
  }
};

int LoadMeshInfo(MeshInfo &info, const fs::path &p) {
  int ret = LoadMesh(info.mesh, p);
  if (ret != 0) {
    return ret;
  }
  info.name = p.stem().string();
  info.box = ComputeBBox(info.mesh.v);  
  return ret;
}

static std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir) {
  std::vector<MeshInfo> fruits;
  fs::path inPath(meshDir);

  if (!fs::exists(inPath) || !fs::is_directory(inPath)) {
    std::cout << inPath.string() << " not a valid directory\n";
    return fruits;
  }
  for (const auto &entry : fs::directory_iterator(inPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::cout << entry.path().filename() << "\n";
    fs::path p = entry.path();
    std::cout << "Full Filename: " << p.filename() << "\n";
    std::cout << "Stem (Name):   " << p.stem() << "\n";
    std::cout << "Extension:     " << p.extension() << "\n";

    std::string stem = p.stem().string();
    if (stem.ends_with("_m")) {
      // skip mirrored meshes.
      continue;
    }
    MeshInfo info;    
    int ret = LoadMeshInfo(info, p);
    if (ret != 0) {
      std::cout << " error " << ret << " loading " << p.filename().string() << "\n";
      continue;
    }
    fruits.push_back(info);
  }

  return fruits;
}

class PackingScene {
  public:
    MeshInfo container;
    std::vector<MeshInfo> items;
    std::vector<int> sortedBySize;
    std::shared_ptr<AdapSDF> sdf;
    std::string outputFolder;
};

std::vector<int> SortBySize(const std::vector<MeshInfo> &items) {
  std::vector<int> indices(items.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            [&](int i, int j) { return items[i].BoxDiagonal() > items[j].BoxDiagonal(); });
  return indices;
}

bool AddUsingSDF(MeshConvo &bg, const TrigMesh &part, Vec3f &pos, const Vec3f &rot, 
  std::shared_ptr<AdapSDF> sdf, float factor = 1.0f) {
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);
  float dx = bg.dx;

  MeshConvo fg;
  fg.SetMeshPtr(&rotated);
  fg.Voxelize(dx);
  const unsigned FFT_ALIGNMENT = 8;
  FloodOutside8u(fg.vox, 1);
  Vec3u bgSize = bg.GridSize();
  Vec3u fgSize = fg.GridSize();
  // use circular fft/ntt. no need to pad with fg size.
  Vec3u totalSize = bgSize;
  //+fgSize;
  std::cout << "total grid size " << totalSize[0] << " " << totalSize[1] << " " << totalSize[2] << "\n";
  Vec3u gridSize = PadSizes(totalSize, FFT_ALIGNMENT);

  bg.FFT(gridSize);
  Reverse(fg.vox);
  fg.gridReversed = true;
  fg.FFT(gridSize);
  Dot(bg.fft, fg.fft);
  Array3Df conv = IFFT(fg.fft);
  Array3D8u collision = Quantize(conv);
  const float score0 = -1e6f;
  
  float highScore = score0;
  Vec3u bestPos(0);

  Vec3f center = (gridSize.cast<float>() + fgSize.cast<float>());
  center = 0.5f * center - Vec3f(1);
  float maxDist = gridSize.cast<float>().norm();
  // index from 0 to fgSize-2 are outside of the container.
  // fgSize -1 is inside the container.
    
  Vec3f vmin = fg.meshBox.vmin;
  Vec3f vmax = fg.meshBox.vmax;
  Vec3f o = 0.5f * (vmin + vmax) - fg.GetOrigin();
  Vec3i centerOffset( int(-o[0] / fg.dx), int(-o[1]/fg.dx), int(-o[2]/fg.dx) );

  std::cout<<"fg grid size "<<fg.GridSize()[0]<<" "<<fg.GridSize()[1]<<" "<<fg.GridSize()[2]<<"\n";
  std::cout<<"fg center coord "<<o[0]<<" "<<o[1]<<" "<<o[2]<<"\n";
  std::cout<<"fg origin " <<fg.GetOrigin()[0]<<" "<<fg.GetOrigin()[1] <<" "<<fg.GetOrigin()[2] <<"\n";
  std::cout <<"fg center offset "<< centerOffset[0] <<" "<<centerOffset[0] <<" " <<centerOffset[2]<<"\n";
  for (unsigned z = fgSize[2] - 1; z < gridSize[2]; z++) {
    for (unsigned y = fgSize[1] - 1; y < gridSize[1]; y++) {
      for (unsigned x = fgSize[0] - 1; x < gridSize[0]; x++) {
        if (collision(x, y, z) > 0) {
          continue;
        }
        Vec3f coord = bg.GetOrigin() + fg.dx * Vec3f(float((x-centerOffset[0])+0.5f), 
          float(y-centerOffset[1]+0.5f), 
          float(z-centerOffset[2]+0.5f));
        
        float dist = sdf->GetCoarseDist(coord);

        if (dist >= 32766) {
          dist = -dist;
        }
        float score = factor * dist;
        if (score > highScore) {
          highScore = score;
          bestPos = Vec3u(x, y, z);
        }
      }
    }
  }
  std::cout << "high score " << highScore << "\n" ;
  bool found = (highScore > score0);
  if (found) {
    Vec3f dxVec(dx);
    Vec3f s = fg.GridSize().cast<float>() - Vec3f(1.0f);
    Vec3f origin = bg.GetOrigin() - fg.GetOrigin() - dxVec * s;
    pos = dx * bestPos.cast<float>() + origin;
    Vec3i offset = bestPos.cast<int>();
    Vec3i fgSizeInt = fgSize.cast<int>();
    offset = offset - fgSizeInt + Vec3i(1);
    if (fg.gridReversed) {
      UnionReversed(bg, offset, fg);
    } else {
      Union(bg, offset, fg);
    }
  }
  return found;
}

void PackScene(PackingScene & scene) {  
  const float dx = 0.3;
  MeshConvo bg;
  bg.SetMeshPtr(&scene.container.mesh);
  bg.Voxelize(dx);

  FloodOutside8u(bg.vox, 1);
  Invert01(bg.vox);

  Vec3f dxVec(dx);  
  const unsigned NUM_COPIES = 1000;
  const float outputScale = 1.05f;
  const unsigned ANGLE_TRIALS = 5;
  std::vector<Vec3f> randAngles(100);
  unsigned int seed = 123;
  std::mt19937 engine(seed);
  std::uniform_real_distribution<float> dist(0.0f, 6.2831853);

  for (size_t i = 0; i < randAngles.size(); i++) {
    randAngles[i][0] = dist(engine);
    randAngles[i][1] = dist(engine);
    randAngles[i][2] = dist(engine);
  }
  std::ofstream out(scene.outputFolder + "/pack.txt");
  bool debugging = true;
  Vec3f voxRes(bg.dx);
  Vec3f origin = bg.GetOrigin();

  // small medium large
  int itemSizeGroup = 3;
  unsigned groupSize = scene.items.size() / 3;

  // Structure to store transformation data
  struct TransformData {
    Vec3f position;
    Vec3f rotation;
    float scale;
  };

  for(unsigned sizei = 0;sizei<scene.sortedBySize.size();sizei++){
    unsigned i = scene.sortedBySize[sizei];
    unsigned numTrials = 0;
    size_t placedCount = 0;
    itemSizeGroup = 3 - std::min(3u, sizei / groupSize);
    float factor = 1.0f;
    if (itemSizeGroup < 1) {
      factor = -1.0f;
    }
    std::cout << "factor " << factor << "\n";

    // Accumulate merged mesh and transformations for this item type
    TrigMesh mergedMesh;
    std::vector<TransformData> transforms;

    for (size_t j = 0; j < NUM_COPIES; j++) {
      Vec3f pos, rot = randAngles[j % randAngles.size()];
      bool success = AddUsingSDF(bg, scene.items[i].mesh, pos, rot, scene.sdf, factor);
      if (!success) {
        numTrials++;
        if (numTrials > 10) {
          break;
        }
        continue;
      }

      out << scene.items[i].name << " " << pos[0] << " " << pos[1] << " " << pos[2] << " " << rot[0] << " " << rot[1]
          << " "
          << rot[2] << "\n";

      if (debugging) {
        // Store transformation data
        TransformData transform;
        transform.position = pos;
        transform.rotation = rot;
        transform.scale = outputScale;
        transforms.push_back(transform);

        // Create transformed mesh
        TrigMesh transformedMesh = scene.items[i].mesh;
        transformedMesh.scale(outputScale);
        Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
        TransformVerts(scene.items[i].mesh.v, transformedMesh.v, rotMat);
        transformedMesh.translate(pos[0], pos[1], pos[2]);

        // Append to merged mesh for this item type
        mergedMesh.append(transformedMesh);

        if (i == 1) {
          std::string debugVol = scene.outputFolder + "/vol_" + std::to_string(i) + "_" + std::to_string(j) + ".obj";
          SaveVolAsObjMesh(debugVol, bg.vox, (float *)(&voxRes), (float *)(&origin), 1);
        }
      }
      placedCount++;
    }

    // Save merged mesh for this item type after packing all copies
    if (debugging && !transforms.empty()) {
      std::string mergedMeshFile = scene.outputFolder + "/" + scene.items[i].name + "_merged.obj";
      mergedMesh.SaveObj(mergedMeshFile);
      std::cout << "Saved merged mesh for " << scene.items[i].name << " with " << transforms.size() << " instances\n";

      // Save transformation data to a text file
      std::string transformFile = scene.outputFolder + "/" + scene.items[i].name + "_transforms.txt";
      std::ofstream transformOut(transformFile);
      for (const auto& t : transforms) {
        transformOut << t.position[0] << " " << t.position[1] << " " << t.position[2] << " "
                     << t.rotation[0] << " " << t.rotation[1] << " " << t.rotation[2] << " "
                     << t.scale << "\n";
      }
      transformOut.close();
      std::cout << "Saved transformations for " << scene.items[i].name << " to " << transformFile << "\n";
    }
  }
  out.close();
}

void PackFruits() {
  std::string meshDir = "F:/meshes/fruit_hand/fruits/";
  PackingScene scene;
  std::vector<MeshInfo> fruits = LoadAllMeshInfo(meshDir);
  scene.items = fruits;
  scene.container;
  fs::path containerFile("F:/meshes/fruit_hand/hands/hand4.5m_finger.stl");
  LoadMeshInfo(scene.container, containerFile);
  scene.sortedBySize = SortBySize(scene.items);
  float sdfDx = 1.0f;
  float distUnit = 0.001f * sdfDx;
  std::cout << "computing container sdf \n";
  scene.sdf = ComputeSDF(distUnit, sdfDx, scene.container.mesh);
  std::cout << "computing container sdf done \n";
  scene.outputFolder = "F:/meshes/fruit_hand/out/";
  PackScene(scene);
}

int main(int argc, char * argv[]){
  PackFruits();
  return 0;
}