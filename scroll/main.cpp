#include <iostream>
#include <array>
#include <iomanip>
#include <sstream>
#include "Array2D.h"
#include "lodepng.h"
#include "TrigMesh.h"
#include "tiffconf.h"
#include "UILib.h"
#include <random>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tiffio.h"

static void LoadPngToSlice(const std::string& filename, Array2D8u& slice) {
  std::vector<unsigned char> buf;
  lodepng::State state;
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  unsigned error = lodepng::load_file(buf, filename.c_str());
  unsigned w, h;
  slice.GetData().clear();
  error = lodepng::decode(slice.GetData(), w, h, state, buf);
  slice.SetSize(w, h);
}

static size_t ExtractMat(const Array2D8u& slice, Array2D8u& dst, uint8_t targetMat)
{
  size_t pixCount = 0;
  Vec2u sliceSize = slice.GetSize();
  dst.Allocate(sliceSize[0], sliceSize[1]);
  dst.Fill(0);
  for (unsigned int row = 0; row < sliceSize[1]; row++) {
    for (unsigned col = 0; col < sliceSize[0]; col++) {
      uint8_t mat = slice(col, row);
      if (mat == targetMat) {
        dst(col, row) = mat;
        pixCount++;
      }
    }
  }
  return pixCount;
}

struct Rect {
  int x, y;
  int width, height;
  Rect() : x(0), y(0), width(0), height(0) {}
};

///grows in x then y. modifies visited pixels
static Rect GrowRect(const Array2D8u& slice, Array2D8u& visited, int x0, int y0) {
  Rect r;
  Vec2u sliceSize = slice.GetSize();
  int x1 = x0;
  if (visited(x0, y0) || slice(x0, y0) == 0) {
    return r;
  }
  for (x1 = x0; x1 < sliceSize[0]; x1++) {
    uint8_t mat = slice(x1, y0);
    if (visited(x1, y0) || mat == 0) {
      x1--;
      break;
    }
  }
  if (x1 == int(sliceSize[0])) {
    x1 = int(sliceSize[0]) - 1;
  }

  int y1 = y0;
  for (; y1 < sliceSize[1]; y1++) {
    bool goodRow = true;
    for (int x = x0; x <= x1; x++) {
      uint8_t mat = slice(x, y1);
      if (mat == 0 || visited(x, y1)) {
        goodRow = false;
        break;
      }
    }
    if (!goodRow) {
      y1--;
      break;
    }
  }
  if (y1 == int(sliceSize[1])) {
    y1 = int(sliceSize[1] - 1);
  }
  r.x = x0;
  r.y = y0;
  r.width = x1 - x0 + 1;
  r.height = y1 - y0 + 1;

  for (int y = r.y; y < r.y + r.height; y++) {
    for (int x = r.x; x < r.x + r.width; x++) {
      visited(x, y) = 1;
    }
  }

  return r;
}

static void Img2Mesh(TrigMesh& out, const Array2D8u& slice, float* voxRes, float z0) {
  Vec2u sliceSize = slice.GetSize();
  Array2D8u visited(sliceSize[0], sliceSize[1]);
  visited.Fill(0);
  for (int row = 0; row < sliceSize[1]; row++) {
    for (int col = 0; col < sliceSize[0]; col++) {
      if (slice(col, row) == 0) {
        continue;
      }
      if (visited(col, row)) {
        continue;
      }
      Rect r = GrowRect(slice, visited, col, row);
      if (r.width == 0 || r.height == 0) {
        continue;
      }
      TrigMesh cube;
      Vec3f mn, mx;
      mn[0] = r.x * voxRes[0];
      mn[1] = r.y * voxRes[1];
      mn[2] = z0;
      mx[0] = mn[0] + r.width * voxRes[0];
      mx[1] = mn[1] + r.height * voxRes[1];
      mx[2] = z0 + voxRes[2];
      cube = MakeCube(mn, mx);
      out.append(cube);
    }
  }
}

/// each material is saved to a separate obj with _{material_id} suffix
static void SaveSliceAsStl(std::string outFilePrefix, const Array2D8u& slice,
  float* voxRes, float z0, int layer) {
  const unsigned int MAX_MATERIAL = 4;
  TrigMesh mout[MAX_MATERIAL];
  std::vector<int> pixCount(MAX_MATERIAL, 0);
  Vec2u sliceSize = slice.GetSize();
  for (uint8_t targetMat = 1; targetMat <= 4; targetMat++) {
    Array2D8u matSlice;
    size_t numPix = ExtractMat(slice, matSlice, targetMat);
    if (numPix == 0) {
      continue;
    }

    Img2Mesh(mout[targetMat - 1], matSlice, voxRes, z0);
    std::string outFile = outFilePrefix + std::to_string(layer)
      + "_" + std::to_string(targetMat) + ".stl";
    mout[targetMat - 1].SaveStlTxt(outFile);
  }
}

void Png2StlTxt(const std::string dir, size_t numImg) {
  size_t layer = 0;
  std::string prefix = dir + "/";
  for (; layer < numImg; layer++) {
    std::string imgFile = prefix + std::to_string(layer) + ".png";
    Array2D8u slice;
    LoadPngToSlice(imgFile, slice);
    float voxRes[3] = { 0.064, 0.064, 1 };
    float z0 = layer * voxRes[2];
    SaveSliceAsStl(prefix, slice,
      voxRes, z0, layer);
  }
}

uint8_t map16to8(unsigned short x) {
  const unsigned short x0 = (unsigned short)(0.35f * 65535);
  const unsigned short x1 = 65535;
  const unsigned short range = x1 - x0;
  const unsigned short scale = range / 255;
  if (x < x0) {
    x = x0;
  }
  if (x > x1) {
    x = x1;
  }
  return (x -x0)/ scale;
}

void LoadTiff(Array2D8u & image, const std::string & file) {

  TIFF* tif;
  const char* filename = file.c_str();
  tif = TIFFOpen(filename, "r");
  if (!tif) {
    return;
  }
  uint32 imagelength;
  uint32 w, h;
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
  printf("%d %d\n", w, h);
  tstrip_t strip;
  size_t npixels = w * h;
  uint32 tileWidth, tileLength;
  TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
  TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength);
  printf("%d %d len: %d\n", tileWidth, tileLength, imagelength);
  size_t scanLineSize = TIFFScanlineSize(tif);
  std::vector<uint8_t> buf (scanLineSize);
  image.Allocate(w, h);
  for (uint32 row = 0; row < imagelength; row++) {
    uint8_t* dst = image.DataPtr() + row * w;
    TIFFReadScanline(tif, buf.data(), row, 1);
    for (uint32_t col = 0; col < w;col++) {
      unsigned short srcVal = *(unsigned short*)(&buf[2*col]);
      dst[col] =map16to8(srcVal);
    }
  }
  TIFFClose(tif);
}

int SavePngGrey(const std::string& filename, const Array2D8u& arr) {
  std::vector<unsigned char> buf;
  lodepng::State state;
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  unsigned error = lodepng::encode(buf, arr.GetData(), arr.GetSize()[0],
    arr.GetSize()[1], state);
  if (error) {
    return -2;
  }
  error = lodepng::save_file(buf, filename.c_str());
  if (error) {
    return -1;
  }
  return 0;
}

std::string MakeFileName(const std::string& prefix,
  int idx, int padding, const std::string& suffix) {
  std::ostringstream oss;
  oss << prefix << std::setfill('0') << std::setw(padding) << idx << suffix;
  return oss.str();
}

void ConvertImages() {
  //std::string prefix = "H:/scroll1mask/masked_";
  std::string prefix = "H:/segments/dl.ash2txt.org/hari-seldon-uploads/team-finished-paths/scroll1/20230929220924/layers/";
  std::string suffixIn = ".tif";
  std::string suffixOut = ".png";
  std::string outPrefix = "H:/segments/dl.ash2txt.org/hari-seldon-uploads/team-finished-paths/scroll1/20230929220924/png/";
  unsigned i0 = 0;
  unsigned i1 = 64;
  const int padding = 2;
  for (unsigned i = i0; i <= i1; i++) {
    std::string inFile = MakeFileName(prefix, i, padding, suffixIn);
    Array2D8u image;
    LoadTiff(image, inFile);
    std::string outFile = MakeFileName(outPrefix, i, padding, suffixOut);
    SavePngGrey(outFile, image);
  }
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

uint8_t MergePixels(uint8_t a, uint8_t b) {
  uint8_t out;
  //if there is a bright pixel, return the max to preserve it.
  const uint8_t brightThresh = 125;
  if (a>brightThresh) {
    if (a > b) {
      return a;
    }
    return b;
  }
  if (b > brightThresh) {
    if (a > b) {
      return a;
    }
    return b;
  }
  //return average value otherwise.
  out = (uint16_t(a) + b) / 2;
  return out;
}

/// generates uniformly random float
struct UniRand {
  std::uniform_real_distribution<float> uniDist;
  std::mt19937 randGen;
  UniRand() : uniDist(0.0f, 1.0f), randGen(std::random_device{}()) {}
  float rand() { return uniDist(randGen); }
};

UniRand uniRand;

uint8_t sampleMat(uint8_t greyVal) {
  
  //value below this is void.
  const uint8_t lowVal = 20;
  //blend support and mat1
  const uint8_t midVal = 100;
  //values above this is mostly epoxy.
  const uint8_t highVal = 250;
  if (greyVal < lowVal) {
    return 0;
  }
  float r = uniRand.rand();
  if (greyVal < midVal) {
    float P1 = (greyVal - lowVal) / float(midVal - lowVal);
    if (r < P1) {
      return 1;
    }
    return 0;
  }
  if (greyVal < highVal) {
    float P2 = (greyVal - midVal) / float(highVal - midVal);
    if (r < P2) {
      return 2;
    }
    return 1;
  }
  return 2;
}

void MakeSlices() {
  unsigned i0 = 6881;
  unsigned i1 = 7720;
  const std::string inPrefix = "H:/scroll1maskpng/";
  const std::string outPrefix = "H:/scroll1slice/";
  const unsigned padding = 5;
  const std::string suffix = ".png";

  for (unsigned i = i0; i <= i1; i++) {
    Array2D8u image;
    std::string inFile = MakeFileName(inPrefix, i, padding, suffix);
    LoadPngGrey(inFile, image);
    Vec2u size = image.GetSize();
    Vec2u outSize=size;
    //compress 2x in y since y printing res is only 63.5um.
    outSize[1] = size[1]/2;
    Array2D8u slice(size[0],outSize[1]);
    //read 2 src rows and combine into 1.
    for (unsigned dstRow = 0; dstRow < outSize[1]; dstRow++) {
      const uint8_t* srcRow0 = image.DataPtr() + 2 * dstRow * size[0];
      const uint8_t* srcRow1 = image.DataPtr() + (2 * dstRow + 1) * size[0];
      uint8_t* dstPtr = slice.DataPtr() + dstRow * size[0];
      for (unsigned col = 0; col < outSize[0]; col++) {
        uint8_t greyVal = MergePixels(srcRow0[col], srcRow1[col]);
        dstPtr[col] = sampleMat(greyVal) * 50;
      }
    }
    std::string outFile = MakeFileName(outPrefix, i, padding, suffix);
    SavePngGrey(outFile, slice);
  }

}

void UIMain() { 
  UILib ui;
  ui.SetShowGL(false);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 800);
  ui.Run();
  const unsigned PollFreqMs = 100;
  while (ui.IsRunning()) {
    // main thread not doing much
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

int main(int argc, char* argv[]) {
  //MakeSlices();
  //ConvertImages();
  UIMain();
  return 0;
}
