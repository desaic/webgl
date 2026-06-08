#include "GridUtils.h"
#include "Array2D.h"
#include "ImageIO.h"
#include "pocketfft_3df.h"

#include <algorithm>
#include <thread>


Vec3f AlignOriginToGrid(const Vec3f &o, float dx) {
  Vec3f aligned;
  for (unsigned d = 0; d < 3; d++) {
    aligned[d] = std::floor(o[d] / dx) * dx;
  }
  return aligned;
}


std::array<float, 3> ToArray(const Vec3f &v) {
  return {v[0], v[1], v[2]};
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

void ThreshInPlace(Array3D8u &arr , uint8_t thresh) {
  for (uint8_t & val: arr.GetData()) {
    if(val >= thresh){
      val = 1;
    }else{
      val = 0;
    }
  }
}

float fclamp(float v, float lb, float ub){
  if(v<lb){
    return lb;
  }
  if(v>ub){
    return ub;
  }
  return v;
}

// quantize and clamps to 0 to 255
Array3D8u Quantize(const Array3Df &f) {
  Vec3u size = f.GetSize();
  Array3D8u binArray(size, 0);
  const auto &fVec = f.GetData();
  auto &binVec = binArray.GetData();
  for (size_t i = 0; i < fVec.size(); i++) {
    float val = fclamp(fVec[i], 0.0f, 255.0f);
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

Vec3f ToFloat(const Vec3u &v) {
  return Vec3f(v[0], v[1], v[2]);
}

Vec3u ToVec3u(const Vec3f &fvec) {
  return Vec3u(unsigned(fvec[0]), unsigned(fvec[1]), unsigned(fvec[2]));
}

bool InBound(Vec3f x, Vec3u size) {
  return x[0] >= 0 && x[1] >= 0 && x[2] >= 0 && x[0] < size[0] && x[1] < size[1] && x[2] < size[2];
}

bool InBound(Vec3u x, Vec3u size) {
  return x[0] < size[0] && x[1] < size[1] && x[2] < size[2];
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

void InvertContainer(Array3D8u &vox, uint8_t boundaryVal) {
  for (auto &val : vox.GetData()) {
    if(val > 0 && val != boundaryVal){
      val = 0;
    }else{
      val = 1;
    }
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
