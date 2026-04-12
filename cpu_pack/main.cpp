#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "FloodOutside.h"
#include "Matrix3f.h"
#include "cpu_voxelizer.h"
#include "pocketfft_3df.h"
#include <iostream>
#include <fstream>

#include "Stopwatch.h"
#include "meshutil.h"

#include <deque>

#include <cstring>
#include <random>
#include <thread>
#include "ImageIO.h"

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

std::array<unsigned, 3> ComputeGridSize(const Box3f &box, float dx) {
  Vec3f boxSize = box.vmax - box.vmin;
  return {unsigned(boxSize[0] / dx) + 1, unsigned(boxSize[1] / dx) + 1, unsigned(boxSize[2] / dx) + 1};
}

std::array<float, 3> ToArray(const Vec3f &v) {
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

// data for computing convolution for a mesh.
class MeshConvo {
  public:
    // can be bigger than actual mesh box
    Box3f box;
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
      box = ComputeBBox(mesh->v);
      box.vmin = AlignOriginToGrid(box.vmin, dx);
      VoxConf conf;
      conf.origin = ToArray(box.vmin);
      conf.unit = {dx, dx, dx};
      conf.gridSize = ComputeGridSize(box, dx);
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
        bg.vox(dstx, dsty, dstz) = 1;
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
    Union(bg, offset, fg);
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
  const unsigned FFT_ALIGNMENT = 8;

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
  for (size_t i = 0; i < numFruits; i++) {
    unsigned numTrials = 0;
    for (size_t j = 0; j < NUM_COPIES; j++) {
      Vec3f pos, rot = randAngles[j % randAngles.size()];
      bool success = Add(bg, fruits[i], pos, rot);
      if (!success) {
        numTrials++;
        if (numTrials > 10) {
          break;
        }
      }
      out << fruitFiles[i] << " " << pos[0] << " " << pos[1] << " " << pos[2] << " " << rot[0] << " " << rot[1] << " "
          << rot[2] << "\n";
      if (debugging) {
        TrigMesh out = fruits[i];
        Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
        TransformVerts(fruits[i].v, out.v, rotMat);
        out.translate(pos[0], pos[1], pos[2]);
        std::string debugMesh = outputFolder + "/" + fruitFiles[i] + std::to_string(j) + ".obj";
        out.SaveObj(debugMesh);
      }
    }
  }
  out.close();
}

int main(int argc, char * argv[]){
  std::cout<<"cpu_pack\n";
  return 0;
}