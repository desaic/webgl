#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <filesystem>

#include "Array2D.h"
#include "ConfigFile.h"
#include "TrigMesh.h"
#include "UILib.h"
#include "lodepng.h"
#include "tiffconf.h"
#include "tiffio.h"
#include "ImageIO.h"

namespace fs = std::filesystem;

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

void createFolder(const std::string& folderPath) {
  if (!fs::exists(folderPath)) {
    fs::create_directory(folderPath);
  }
}

static size_t ExtractMat(const Array2D8u& slice, Array2D8u& dst,
                         uint8_t targetMat) {
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

/// grows in x then y. modifies visited pixels
static Rect GrowRect(const Array2D8u& slice, Array2D8u& visited, int x0,
                     int y0) {
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

static void Img2Mesh(TrigMesh& out, const Array2D8u& slice, float* voxRes,
                     float z0) {
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
    std::string outFile = outFilePrefix + std::to_string(layer) + "_" +
                          std::to_string(targetMat) + ".stl";
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
    float voxRes[3] = {0.064, 0.064, 1};
    float z0 = layer * voxRes[2];
    SaveSliceAsStl(prefix, slice, voxRes, z0, layer);
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
  return (x - x0) / scale;
}

void LoadTiff(Array2D8u& image, const std::string& file) {
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
  std::vector<uint8_t> buf(scanLineSize);
  image.Allocate(w, h);
  for (uint32 row = 0; row < imagelength; row++) {
    uint8_t* dst = image.DataPtr() + row * w;
    TIFFReadScanline(tif, buf.data(), row, 1);
    for (uint32_t col = 0; col < w; col++) {
      unsigned short srcVal = *(unsigned short*)(&buf[2 * col]);
      dst[col] = map16to8(srcVal);
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

std::string MakeFileName(const std::string& prefix, int idx, int padding,
                         const std::string& suffix) {
  std::ostringstream oss;
  oss << prefix << std::setfill('0') << std::setw(padding) << idx << suffix;
  return oss.str();
}

void ConvertImages() {
  // std::string prefix = "H:/scroll1mask/masked_";
  std::string prefix =
      "H:/segments/dl.ash2txt.org/hari-seldon-uploads/team-finished-paths/"
      "scroll1/20231005123334/layers/";
  std::string suffixIn = ".tif";
  std::string suffixOut = ".png";
  std::string outPrefix =
      "H:/segments/dl.ash2txt.org/hari-seldon-uploads/team-finished-paths/"
      "scroll1/20231005123334/png/";
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

void GetCol(const Array2D8u& image, unsigned j, std::vector<uint8_t>& col) {
  Vec2u size = image.GetSize();
  col.resize(size[1]);
  for (unsigned row = 0; row < size[1]; row++) {
    col[row] = image(j, row);
  }
}
void SetCol(Array2D8u& image, unsigned j, const std::vector<uint8_t>& col) {
  Vec2u size = image.GetSize();
  for (unsigned row = 0; row < size[1]; row++) {
    image(j, row) = col[row];
  }
}

void DownsampleVec4x(const uint8_t * src, uint8_t * dst, size_t srclen) {
  for (size_t i = 0; i < srclen; i+=4) {
    uint16_t sum = 0;
    for (unsigned j = 0; j < 4; j++) {
      sum += src[i + j];
    }
    dst[i / 4] = uint8_t(sum / 4);
  }
}

void Downsample4x(const Array2D8u &image, Array2D8u & dst) {
  Vec2u size = image.GetSize();
  Vec2u dstSize(size[0]/4, size[1]/4);
  dst.Allocate(dstSize[0], dstSize[1]);

  //skinny image. same rows. 1/4 cols.
  Array2D8u tmpImage(dstSize[0], size[1]);

  //downsample rows
  for (unsigned row = 0; row < size[1]; row++) {
    const uint8_t* srcPtr = image.DataPtr() + row * size[0];
    uint8_t* dstPtr = tmpImage.DataPtr() + row * dstSize[0];
    DownsampleVec4x(srcPtr, dstPtr, size[0]);
  }
  //SavePngGrey("tmp.png", tmpImage);
  //cols
  for (unsigned col = 0; col < dstSize[0]; col ++) {
    std::vector<uint8_t> srcCol;
    GetCol(tmpImage, col, srcCol);
    std::vector<uint8_t> dstCol(dstSize[1]);
    DownsampleVec4x(srcCol.data(), dstCol.data(), size[1]);
    SetCol(dst, col, dstCol);
  }
}

void DownsampleImages() {
  std::string prefix =
      "H:/segments/dl.ash2txt.org/hari-seldon-uploads/team-finished-paths/"
      "scroll1/20231005123334/png/";
  std::string suffixIn = ".png";
  std::string suffixOut = ".png";
  std::string outDir =
      "H:/segments/dl.ash2txt.org/hari-seldon-uploads/team-finished-paths/"
      "scroll1/20231005123334/png_quat/";
  createFolder(outDir);
  unsigned i0 = 0;
  unsigned i1 = 64;
  const int padding = 2;
  for (unsigned i = i0; i <= i1; i++) {
    std::string inFile = MakeFileName(prefix, i, padding, suffixIn);
    Array2D8u image;
    LoadPngGrey(inFile, image);
    Array2D8u out;
    Downsample4x(image, out);
    std::string outFile = MakeFileName(outDir, i, padding, suffixOut);
    SavePngGrey(outFile, out);
  }
}

uint8_t MergePixels(uint8_t a, uint8_t b) {
  uint8_t out;
  // if there is a bright pixel, return the max to preserve it.
  const uint8_t brightThresh = 125;
  if (a > brightThresh) {
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
  // return average value otherwise.
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
  // value below this is void.
  const uint8_t lowVal = 20;
  // blend support and mat1
  const uint8_t midVal = 100;
  // values above this is mostly epoxy.
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
    Vec2u outSize = size;
    // compress 2x in y since y printing res is only 63.5um.
    outSize[1] = size[1] / 2;
    Array2D8u slice(size[0], outSize[1]);
    // read 2 src rows and combine into 1.
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

void LoadImages() { Array2D8u combined; }

ConfigFile conf;

void OpenConfig(const std::string& filename) {
  std::ifstream in(filename);
  if (!in.good()) {
    std::cout << "failed to open " << filename << "\n";
    return;
  }
  conf.Load(in);
}

bool endsWith(const std::string& fullString, const std::string& ending) {
  if (fullString.length() >= ending.length()) {
    return (fullString.compare(fullString.length() - ending.length(),
                               ending.length(), ending) == 0);
  } else {
    return false;
  }
}

void SaveConfig(const std::string& filename) {
  std::string file=filename;
  if (!endsWith(file, ".txt")) {
    file = filename + ".txt";
  }
  std::ofstream out(file);
  if (!out.good()) {
    std::cout << "failed to write " << filename << "\n";
    return;
  }
  conf.Save(out);
}

void GrayToRGBA(const Array2D8u & gray, Array2D8u & color) {
  Vec2u size = gray.GetSize();
  color.Allocate(size[0] * 4, size[1]);
  for (size_t row = 0; row < size[1]; row++) {
    const uint8_t* srcPtr = gray.DataPtr() + row * size[0];
    uint32_t* dstPtr = (uint32_t*)(color.DataPtr() + row * size[0] * 4);
    for (size_t col = 0; col < size[1];col++) {
      uint32_t gray = srcPtr[col];
      uint32_t color = 0xff000000 | (gray<<16) | (gray<<8) | (gray);
      dstPtr[col] = color;
    }
  }
}

int dummyId = 0;

void UpdateImage(UILib& ui, int imageId, const Array2D8u& gray) {
  Array2D8u color;
  GrayToRGBA(gray, color);
  unsigned numChannels = 4;
  ui.SetImageData(imageId, color, numChannels);
}

void InitRow(unsigned row, Array2D8u& image) {
  const Vec2u& size = image.GetSize();
  size_t x = 989345275647ull;
  for (unsigned col = 0; col < size[0]; col++) {
    image(col, row) = 200*( x%2);
    x /= 2;
  }

  
}

void ShiftVec(const uint8_t* src, uint8_t* dst, size_t len, unsigned shift) {
  std::memcpy(dst, src + shift, len - shift);
}

void Mult3PlusOneDiv2(uint8_t* vec, size_t len) {
  uint8_t carry = (vec[0] > 0);
  const uint8_t colorScale = 200;
  for (unsigned i = 0; i < len-1; i++) {
    uint8_t bit0 = vec[i] != 0;
    uint8_t bit1 = vec[i+1] != 0;
    uint8_t digiSum = (bit0 + bit1 + carry);
    vec[i] = colorScale*( digiSum & 0x1);
    carry = digiSum > 1;
  }
}

size_t CollatzRow(unsigned row, Array2D8u& image) {
  unsigned prevRow = row - 1;
  const Vec2u size = image.GetSize();
  if (row == 0) {
    prevRow = size[1] - 1;
  }
  const uint8_t* prevRowPtr = image.DataPtr() + prevRow * size[0];
  uint8_t* rowPtr = image.DataPtr() + row * size[0];
  unsigned shift = size[0];
  unsigned oneCount = 0;
  for (unsigned col = 0; col < size[0]; col++) {
    if (prevRowPtr[col] > 0) {
      if (oneCount == 0) {
        shift = col;
      }
      oneCount++;      
    }
  }
  if (oneCount <= 1) {
    //terminating
    if (shift > 0) {
      ShiftVec(prevRowPtr, rowPtr, size[0], shift);
    }
    return shift;
  }
  //every bit is 0. normally impossible.
  if (shift >= size[0]) {
    shift = 0;
    return 0;
  }
  if (shift > 0) {
    ShiftVec(prevRowPtr, rowPtr, size[0], shift);
    for (size_t i = size[0] - shift; i < size[0]; i++) {
      rowPtr[i] = 0;
    }
  } else {
    std::memcpy(rowPtr, prevRowPtr, size[0]);
  }

  Mult3PlusOneDiv2(rowPtr, size[0]);
  return 2 + shift;
}

void UIMain() {
  UILib ui;
  OpenConfig("config.txt");
  ui.SetShowGL(false);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 1000);
  ui.SetFileOpenCb(OpenConfig);
  ui.SetFileSaveCb(SaveConfig);
  ui.SetInitImagePos(100, 0);
  dummyId = ui.AddSlideri("lol", 20, -1, 30);
  int mainImageId = ui.AddImage();
  
  Array2D8u mainImage(1024, 800);
  mainImage.Fill(0);
  std::function<void()> btFunc =
      std::bind(&UpdateImage, std::ref(ui), mainImageId, mainImage);
  ui.AddButton("Update image", btFunc);
  std::function<void()> saveImageFunc =
      std::bind(&SavePngGrey, "mainImage.png" , std::ref(mainImage));
  ui.AddButton("Save main image", saveImageFunc);
  ui.Run();
  const unsigned PollFreqMs = 100;
  InitRow(0, mainImage);
  unsigned row = 1;
  unsigned stepCount = 0;
  bool runCalc = true;
  while (ui.IsRunning()) {
   
    if (runCalc) {
      size_t steps = CollatzRow(row, mainImage);
      stepCount += steps;
      if (steps == 0) {
        runCalc = 0;
        std::cout << "total steps" << stepCount;
      }
      row++;
      if (row >= mainImage.GetSize()[1]) {
        row = 0;
      }

      UpdateImage(ui, mainImageId, mainImage);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
    }

  }
}

int main(int argc, char* argv[]) {
  // MakeSlices();
  // ConvertImages();
  DownsampleImages();
 // UIMain();
  return 0;
}
