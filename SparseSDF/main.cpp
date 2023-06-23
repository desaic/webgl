#include "TrigMesh.h"
#include "meshutil.h"
#include "Vec3.h"
#include "MarchingCubes.h"
#include "lodepng.h"
#include "Array2D.h"
#include "Array3D.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

void MirrorCubicStructure(const Array3D8u& s_in, Array3D8u& s_out);
void MarchingCubes(const Array3D8u& grid, float level, TrigMesh* surf);

void loadBinaryStructure(const std::string& filename, Array3D8u& s)
{
  int dim = 3;
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (!in.good()) {
    std::cout << "Cannot open input " << filename << "\n";
    in.close();
    return;
  }
  unsigned int gridSize[3];  
  for (int i = 0; i < dim; i++) {
    in.read((char*)(&gridSize[i]), sizeof(int));    
  }

  s.Allocate(gridSize[0], gridSize[1], gridSize[2]);

  std::vector<uint8_t>& vox = s.GetData();
  for (size_t i = 0; i < vox.size();i+=8) {
    unsigned char aggr;
    in.read((char*)&aggr, sizeof(unsigned char));
    //1 bit per voxel.
    size_t j = 0;
    for (unsigned char mask = 1; mask > 0; mask <<= 1, j++)
      vox[i+j] = (aggr & mask) > 0;
  }
  in.close();
}

void Upsample2x(const Array3D8u&gridIn, Array3D8u& gridOut) {
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

void PadGrid(const Array3D8u& grid, Array3D8u& padded) {
  Vec3u oldSize = grid.GetSize();
  padded.Allocate(oldSize[0] + 2, oldSize[1] + 2, oldSize[2] + 2);
  Vec3u newSize = padded.GetSize();
  for (unsigned i = 0; i < oldSize[0]; i++) {
    for (unsigned j = 0; j < oldSize[1]; j++) {
      for (unsigned k = 0; k < oldSize[2]; k++) {
        padded(i + 1, j + 1, k + 1) = grid(i, j, k);
      }
    }
  }
  //pad x
  for (unsigned y = 0; y < newSize[1]; y++) {
    unsigned srcy = y - 1;
    if (y == 0) {
      srcy = 0;
    } else if (y == newSize[1] - 1) {
      srcy = oldSize[1] - 1;
    }
    
    for (unsigned z = 0; z < newSize[2]; z++) {
      unsigned srcz = z - 1;
      if (z == 0) {
        srcz = 0;
      }
      else if (z == newSize[2] - 1) {
        srcz = oldSize[2] - 1;
      }
      padded(0, y, z) = grid(0, srcy, srcz);
      padded(newSize[0]-1, y, z) = grid(oldSize[0]-1, srcy, srcz);
    }
  }

  //pad y
  for (unsigned x = 0; x < newSize[0];x++) {
    unsigned srcx = x - 1;
    if (x == 0) {
      srcx = 0;
    }
    else if (x == newSize[0] - 1) {
      srcx = oldSize[0] - 1;
    }

    for (unsigned z = 0; z < newSize[2]; z++) {        
      unsigned srcz = z - 1;
      if (z == 0) {
        srcz = 0;
      }
      else if (z == newSize[2] - 1) {
        srcz = oldSize[2] - 1;
      }
      padded(x, 0, z) = grid(srcx, 0, srcz);
      padded(x, newSize[1] - 1, z) = grid(srcx, oldSize[1] - 1, srcz);
    }
  }

  for (unsigned x = 1; x < newSize[0]-1; x++) {
    for (unsigned y = 1; y < newSize[1] - 1;y++) {
      padded(x, y, 0) = grid(x-1, y-1, 0);
      padded(x, y, newSize[2]-1) = grid(x - 1, y-1, oldSize[2]-1);
    }
  }
}

void PadGridConst(const Array3D8u& grid, Array3D8u& padded, uint8_t val) {
  Vec3u oldSize = grid.GetSize();
  padded.Allocate(oldSize[0] + 2, oldSize[1] + 2, oldSize[2] + 2);
  Vec3u newSize = padded.GetSize();
  padded.Fill(val);
  for (unsigned i = 0; i < oldSize[0]; i++) {
    for (unsigned j = 0; j < oldSize[1]; j++) {
      for (unsigned k = 0; k < oldSize[2]; k++) {
        padded(i + 1, j + 1, k + 1) = grid(i, j, k);
      }
    }
  }
}

void ExpandCellToVerts(const Array3D8u& grid, Array3D8u&verts) {
  Vec3u oldSize = grid.GetSize();
  verts.Allocate(oldSize[0] + 1, oldSize[1] + 1, oldSize[2] + 1);
  Vec3u newSize = verts.GetSize();
  verts.Fill(0);

  Array3D8u padded;
  PadGrid(grid, padded);

  for (unsigned i = 0; i < newSize[0]; i++) {
    for (unsigned j = 0; j < newSize[1]; j++) {
      for (unsigned k = 0; k < newSize[2]; k++) {
        uint8_t val = padded(i, j, k);
        val = padded(i, j+1, k);
        val = padded(i+1, j, k);
        val = padded(i+1, j+1, k);

        val = padded(i, j, k+1);
        val = padded(i, j+1, k+1);
        val = padded(i+1, j, k+1);
        val = padded(i+1, j+1, k+1);
        verts(i, j, k) = val;
      }
    }
  }
}

void Scale(Array3D8u& grid, float scale ) {
  std::vector<uint8_t>& data = grid.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = uint8_t(scale*data[i]);
  }
}

void Thresh(Array3D8u& grid, float t) {
  std::vector<uint8_t>& data = grid.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] > t) {
      data[i] = 1;
    }
    else {
      data[i] = 0;
    }
  }
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

//with mirror boundary condition
void FilterVec(uint8_t* v, size_t len, const std::vector<float>& kern)
{
  size_t halfKern = kern.size() / 2;

  std::vector<uint8_t> padded(len+2*halfKern);
  for (size_t i = 0; i < halfKern; i++) {
    padded[i] = v[halfKern - i];
    padded[len - i] = v[len - halfKern + i];
  }
  for (size_t i = 0; i < len; i++) {
    padded[i + halfKern] = v[i];
  }
  
  for (size_t i = 0; i < len; i++) {
    float sum = 0;
    for (size_t j = 0; j<kern.size(); j++) {
      sum += kern[j] * padded[i + j];
    }
    v[i] = uint8_t(sum);
  }
}

void FilterDecomp(Array3D8u& grid, const std::vector<float>& kern)
{
  Vec3u gsize = grid.GetSize();
  //z
  for (unsigned i = 0; i < gsize[0];i++) {
    for (unsigned j = 0; j < gsize[1]; j++) {
      uint8_t* vec = &grid(i, j, 0);
      FilterVec(vec, gsize[2], kern);
    }
  }
  //y
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
  //x
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

int SavePng(const std::string& filename, Array2D8u& slice) {
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  state.encoder.auto_convert = 0;
  std::vector<unsigned char> out;
  Vec2u size = slice.GetSize();
  lodepng::encode(out, slice.GetData(), size[0], size[1], state);
  std::ofstream outFile(filename, std::ofstream::out | std::ofstream::binary);
  outFile.write((const char*)out.data(), out.size());
  outFile.close();
  return 0;
}

int LoadPngGrey(const std::string& filename, Array2D8u& arr) {
  std::vector<unsigned char> buf;
  lodepng::State state;
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  unsigned error = lodepng::load_file(buf, filename.c_str());
  if (error) {
    return -1;
  }
  unsigned w, h;
  arr.GetData().clear();
  error = lodepng::decode(arr.GetData(), w, h, state, buf);
  if (error) {
    return -2;
  }
  arr.SetSize(w, h);
  return 0;
}
void SaveSlices(const std::string &prefix, const Array3D8u&vol) {
  Vec3u size = vol.GetSize();
  Array2D8u slice(size[0], size[1]);
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        slice(x, y) = vol(x, y, z);
      }
    }
    std::stringstream ss;
    ss << prefix;
    ss << std::setw(4) << std::setfill('0')<< z << ".png";
    std::string outFile = ss.str();
    SavePng(outFile, slice);
  }
}

void LoadImageSequence(const std::string& dir, int maxIndex, Array3D8u& vol) {
  int lasti = 0;
  for (int i = 0; i <= maxIndex; i++) {
    std::string imageFile = dir + "/" + std::to_string(i) + ".png";
    std::filesystem::path imagePath(imageFile);
    if (!std::filesystem::exists(imagePath)) {
      //give up after 10 missing images.
      if (i - lasti > 10) { break; }
      continue;
    }
    lasti=i;

    Array2D8u image;
    LoadPngGrey(imageFile, image);
    if (i == 0) {
      vol.Allocate(image.GetSize()[0], image.GetSize()[1], maxIndex + 1);
    }
    Vec2u imageSize = image.GetSize();
    unsigned rows = std::min(imageSize[1], vol.GetSize()[1]);
    unsigned cols = std::min(imageSize[0], vol.GetSize()[0]);
    for (unsigned row = 0; row < rows; row++) {
      for (unsigned col = 0; col < cols; col++) {
        vol(col, row, i) = image(col, row);
      }
    }
  }
}

#include <intrin.h>
/// Return the number of on bits in the given 64-bit value.
unsigned CountOn(uint64_t v)
{
  v = __popcnt64(v);
  // Software Implementation
  //v = v - ((v >> 1) & UINT64_C(0x5555555555555555));
  //v = (v & UINT64_C(0x3333333333333333)) + ((v >> 2) & UINT64_C(0x3333333333333333));
  //v = (((v + (v >> 4)) & UINT64_C(0xF0F0F0F0F0F0F0F)) * UINT64_C(0x101010101010101)) >> 56;
  return static_cast<unsigned>(v);
}

//cell with a polynomial shape function
struct PolyCell {};

//cell with point samples for improving accuracy.
struct PointCell {};

// sparse node for a 4x4x4 block
#define COARSE_NODE_SIZE 4
template <typename T>
struct SparseNode4 {
  ~SparseNode4() { if (children != nullptr) { delete[]children; } }

  uint64_t mask = 0;
  //array of child cells.
  T * children = nullptr;
  T* GetChild(unsigned x, unsigned y, unsigned z) {
    unsigned linearIdx = (z * COARSE_NODE_SIZE + y) * COARSE_NODE_SIZE + x;
    bool hasChild = mask & (1ull << linearIdx);
    if (!hasChild) {
      return nullptr;
    }    
    unsigned childIdx = CountOn(mask & ((1ull << linearIdx) - 1));
    return children[childIdx];
  }
};

class AdapSDF {
public:
  // distance values are stored on grid vertices.
  Array3D8u vert;
  Array3D<SparseNode4<PolyCell>*> coarseGrid;

  //mm
  float distUnit = 0.01;

  Vec3f origin = { 0.0f, 0.0f, 0.0f };

  //in mm
  Vec3f voxSize;

  int band=5;
};

int BuildSDF(const TrigMesh&mesh, AdapSDF&sdf) {
  // compute distance values for vertices of voxels that intersect
  // triangles.

  // temporary coarse grid storing triangle indices intersecting this cell
  Array3D<SparseNode4<std::vector<size_t>>> idxGrid;

  //refine cells that intersect triangles

  //expand coarse cell.
}
#include <memory>
#include "cpu_voxelizer.h"
#include "Stopwatch.h"
#include "BBox.h"

void TestSDF() {
  std::cout << sizeof(std::shared_ptr<AdapSDF>) << "\n";
  std::string fileName = "F:/dolphin/meshes/lattice_big/MeshLattice.stl";
  TrigMesh mesh;
  mesh.LoadStl(fileName);
  Array3D8u grid;
  voxconf conf;
  BBox box;
  ComputeBBox(mesh.v, box);
  size_t num_verts = mesh.v.size() / 3;
  for (size_t i = 0; i < mesh.v.size(); i += 3) {
    mesh.v[i] -= box.vmin[0];
    mesh.v[i + 1] -= box.vmin[1];
    mesh.v[i + 2] -= box.vmin[2];
  }
  box.vmax = box.vmax - box.vmin;
  box.vmin = Vec3f(0,0,0);
  conf.unit = Vec3f(0.4, 0.4, 0.4);
  conf.origin=Vec3f(0,0,0);
  Vec3f count = box.vmax / conf.unit;
  conf.gridSize = Vec3u(count[0], count[1], count[2]);
  count[0]++;
  count[1]++;
  count[2]++;
  Utils::Stopwatch timer;
  timer.Start();
  cpu_voxelize_mesh(conf, &mesh, grid);  
  float ms = timer.ElapsedMS();
  std::cout << "vox time " << ms << "\n";
  SaveVolAsObjMesh("voxelize.obj", grid, (float*)(&conf.unit), 1);

  AdapSDF sdf;
}

int main(int argc, char* argv[])
{
  TestSDF();

  Array3D8u eyeVol;
  std::string imageDir = "F:/dolphin/meshes/eye0531/slices/";
  int maxIndex = 760;
  LoadImageSequence(imageDir, maxIndex, eyeVol);
  for (int id = 1; id < 4; id++) {
    float voxRes[3] = {0.064,0.0635,0.05};
    SaveVolAsObjMesh("eye_"+ std::to_string(id) + ".obj", eyeVol, voxRes, id);
  }

  int id = 5481;
  std::vector<float> kern;
  GaussianFilter1D(1, 3, kern);

  std::string structFile = "F:/dolphin/meshes/3DCubic64_Data/bin/" + std::to_string(id) + ".bin";
  Array3D8u s, mirrored;
  float voxRes[3] = { 1,1,1 };
  loadBinaryStructure(structFile, s);
  Array3D8u s2, s4;
  Scale(s, 255);
    SaveSlices("s_", s);

  Upsample2x(s, s2);

  //Upsample2x(s2, s4);
  FilterDecomp(s2, kern);
  Downsample2x(s2, s);

  Array3D8u v4,v8;
  ExpandCellToVerts(s, v4);

  SaveSlices("v4_", v4);
  TrigMesh surf;
  MirrorCubicStructure(v4, mirrored);
  MirrorCubicStructure(mirrored, v8);
  MirrorCubicStructure(v8, mirrored);
  PadGridConst(mirrored, v8,0);
  InvertVal(v8);

  MarchingCubes(v8, 220, &surf);

  std::string objFile = std::to_string(id) + "_smooth.obj";
  surf.SaveObj(objFile.c_str());

  //Thresh(mirrored, 125);
  //saveObjRect(std::to_string(id) + "_smooth.obj", mirrored, voxRes, 1);

  return 0;
}

void MirrorCubicStructure(const Array3D8u& s_in, Array3D8u& s_out) {
  Array3D8u st;
  Vec3u gridSize = s_in.GetSize();
  st.Allocate(gridSize[0] * 2, gridSize[1] * 2, gridSize[2] * 2);
  gridSize = st.GetSize();
  for (unsigned i = 0; i < gridSize[0]; i++) {
    for (unsigned j = 0; j < gridSize[1]; j++) {
      for (unsigned k = 0; k < gridSize[2]; k++) {
        unsigned si = i;
        unsigned sj = j;
        unsigned sk = k;
        unsigned tmp = 0;
        if (si >= gridSize[0] / 2) {
          si = gridSize[0] - i - 1;
        }
        if (sj >= gridSize[1] / 2) {
          sj = gridSize[1] - j - 1;
        }
        if (sk >= gridSize[2] / 2) {
          sk = gridSize[2] - k - 1;
        }
        if (si < sj) {
          tmp = si;
          si = sj;
          sj = tmp;
        }
        if (si < sk) {
          tmp = sk;
          sk = si;
          si = tmp;
        }
        if (sj < sk) {
          tmp = sk;
          sk = sj;
          sj = tmp;
        }
        st(i, j, k) = s_in(si, sj, sk);
      }
    }
  }
  s_out = st;
}


void MarchOneCube(unsigned x, unsigned y, unsigned z,
  const Array3D8u & grid, float level, TrigMesh* surf)
{
  GridCell cell;
  cell.p[0]=Vec3f(0,0,0);
  float h = 0.25f;
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

  cell.val[0] = grid(x,y,z);
  cell.val[1] = grid(x, y + 1, z);
  cell.val[2] = grid(x+1, y + 1, z);
  cell.val[3] = grid(x + 1, y, z);
  cell.val[4] = grid(x, y, z + 1);
  cell.val[5] = grid(x, y + 1, z + 1);
  cell.val[6] = grid(x + 1, y + 1, z + 1);
  cell.val[7] = grid(x + 1, y, z + 1);

  MarchCube(cell, level, surf);
}

void MarchingCubes(const Array3D8u & grid, float level, TrigMesh* surf)
{
  Vec3u s = grid.GetSize();
  const unsigned zAxis = 2;
  for (unsigned x = 0; x < s[0] - 1; x++) {
    for (unsigned y = 0; y < s[1] - 1; y++) {
      for (unsigned z = 0; z < s[2] - 1; z++) {
        MarchOneCube(x, y, z, grid, level, surf);
      }
    }
  }
}
