#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <array>

template <typename T>
class Matrix {
public:

  Matrix() :s({ 0,0 }) {}

  T& operator()(size_t row, size_t col) {
    return v[row * s[1] + col];
  }

  const T& operator()(size_t row, size_t col) const{
    return v[row * s[1] + col];
  }

  void Allocate(size_t rows, size_t cols) {
    s[0] = rows;
    s[1] = cols;
    v.resize(rows * cols);
  }

  void Fill(T val) {
    std::fill(v.begin(), v.end(), val);
  }

  //rows and cols.
  std::array<size_t, 2> s;
  std::vector <T> v;
  std::string ToString() const {
    std::ostringstream oss;
    for (size_t row = 0; row < s[0]; row++) {
      for (size_t col = 0; col < s[1]; col++) {
        oss << int((*this)(row, col)) << " ";
      }
      oss << "\n";
    }
    return oss.str();
  }
};

typedef Matrix<unsigned char> Matrixu8;
typedef std::array<int, 3> Vec3i;

struct BBox {
  Vec3i mn, mx;
  BBox() :mn({ 0,0,0 }), mx({ 0,0,0 }) {}
};

int rasterXYFace(std::vector<int> & face,
  Matrixu8 & raster)
{
  int z0 = face[2];
  for (size_t i = 0; i < face.size(); i += 3) {
    int z = face[i + 2];
    if (z != z0) {
      //not an xy face.
      return -1;
    }
  }
  Matrixu8 edgeImg;
  edgeImg.Allocate(raster.s[0]+1, raster.s[1]+1);
  edgeImg.Fill(0);
  for (size_t e = 0; e < face.size(); e += 3) {
    Vec3i v0, v1;
    size_t e1 = e + 3;
    if (e1 >= face.size()) {
      e1 = 0;
    }
    for (int d = 0; d < 3; d++) {
      v0[d] = face[e + d];
      v1[d] = face[e1 + d];
    }
    //only need vertical edges.
    if (v0[0] != v1[0]) {
      continue;
    }
    int y0 = v0[1];
    int y1 = v1[1];
    if (y1 < y0) {
      int tmp = y0;
      y0 = y1;
      y1 = tmp;
    }
    for (int y = y0; y < y1; y++) {
      edgeImg(y, v0[0]) = 1;
    }
  }
  //go through rows to fill the image
  for (size_t row = 0; row < raster.s[0]; row++) {
    int count = 0;
    for (size_t col = 0; col < raster.s[1]; col++) {
      if (edgeImg(row, col) > 0) {
        count++;
      }
      if (count % 2 > 0) {
        raster(row, col) = 1;
      }
    }
  }
  return 0;
}

BBox GetBBox(const std::vector <std::vector<int> >& mesh)
{
  BBox box;
  //set min to first vertex.
  for (int d = 0; d < 3; d++) {
    box.mn[d] = mesh[0][d];
  }
  for (size_t f = 0; f < mesh.size(); f++) {
    for (size_t v = 0; v < mesh[f].size(); v += 3) {
      for (int d = 0; d < 3; d++) {
        int coord = mesh[f][v + d];
        box.mn[d] = std::min(box.mn[d], coord);
        box.mx[d] = std::max(box.mx[d], coord);
      }
    }
  }
  return box;
}

void SnapToOrigin(std::vector <std::vector<int> >& mesh,
  const Vec3i & origin) 
{
  for (size_t f = 0; f < mesh.size(); f++) {
    for (size_t v = 0; v < mesh[f].size(); v += 3) {
      for (int d = 0; d < 3; d++) {
        mesh[f][v + d] -= origin[d];
      }
    }
  }
}

struct SlicePtr {
  int idx; 
  int z;
  Matrixu8* face;
  SlicePtr() :idx(0), z(0), face(nullptr) {}
  bool operator < (const SlicePtr& b) const{
    return z < b.z;
  }
};

//void WriteSlice(const Matrixu8 & raster, int c, int f)
//{
//  std::string outFile = std::to_string(c) + "_" + std::to_string(f) + ".txt";
//  std::ofstream out(outFile);
//  out << raster.ToString();
//  out.close();
//}

int sp7() {
//int main(){
  int numCases;
  std::cin >> numCases;
  int MAX_COORD = 1000;
  for (int c = 0; c < numCases; c++) {
    int numFaces = 0;
    std::cin >> numFaces;
    int64_t vol = 0;
    std::vector <std::vector<int> > mesh;
    mesh.resize(numFaces);
    for (int f = 0; f < numFaces; f++) {
      std::vector<int> & face = mesh[f];
      int numPoints = 0;
      std::cin >> numPoints;
      face.resize(numPoints * 3);
      for (int p = 0; p < numPoints; p++) {
        for (int d = 0; d < 3; d++) {
          std::cin >> face[p*3 + d];
        }
      }
    }
    BBox box = GetBBox(mesh);
    //translate mesh so that origin is (0, 0, 0)
    for (int d = 0; d < 3; d++) {
      box.mx[d] -= box.mn[d];
    }
    SnapToOrigin(mesh, box.mn);

    std::vector<Matrixu8> slices;
    std::vector <SlicePtr> slicePtrs;
    for (int f = 0; f < numFaces; f++) {
      Matrixu8 raster;
      raster.Allocate(box.mx[1], box.mx[0]);
      raster.Fill(0);
      int status = rasterXYFace(mesh[f], raster);
      if (status < 0) {
        continue;
      }
      slices.push_back(raster);
      SlicePtr ptr;
      ptr.idx = int(slicePtrs.size());
      ptr.z = mesh[f][2];
      slicePtrs.push_back(ptr);
    }
    std::sort(slicePtrs.begin(), slicePtrs.end());
    for (int s = 0; s < slicePtrs.size(); s++) {
      slicePtrs[s].face = &(slices[slicePtrs[s].idx]);
    }
    int count = 0;
    for (int row = 0; row < box.mx[1]; row++) {
      for (int col = 0; col < box.mx[0]; col++) {
        int prevz = 0;
        for (int s = 0; s < slicePtrs.size(); s++) {
          Matrixu8 * slice = slicePtrs[s].face;
          int z = slicePtrs[s].z;
          unsigned char val = (*slice)(row, col);
          if (val > 0) {
            if (count % 2 > 0) {
              vol += z - prevz;
            }
            count++;
            prevz = z;
          }
        }
      }
    }
    std::cout << "The bulk is composed of " << vol << " units.\n";
  }
  return 0;
}
