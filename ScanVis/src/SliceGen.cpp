#include "Array3D.h"
#include "ImageIO.h"
#include "Avenir.h"
#include "ConfigFile.hpp"
#include "SliceGen.h"
#include <iostream>
#include <sstream>

Array3D<uint8_t>MakeRaftPattern(unsigned sx, unsigned sy, unsigned sz) {
  Array3D<uint8_t> raft;
  raft.Allocate(sx, sy, sz);
  raft.Fill(1);
  const int anchorHeight = 40;
  const int topSupLayers = 20;
  //not enough layers to make anchors
  if (sz < topSupLayers) {
    return raft;
  }

  unsigned zMax = sz - topSupLayers;
  unsigned bigHeight = anchorHeight / 2;
  float yScale = 2.0f;

  const int pillarSizeX = 30;
  const int pillarSizeY = 15;
  const int pillarHeight = 10;
  int smallFrac= 4;
  int bigFrac = 3;

  for (unsigned z = 0; z < sz; z++) {
    bool small = false;
    bool makePillars = false;
    if (zMax - z > bigHeight) {
      small = true;
    }
    if (z >= zMax && z < zMax + pillarHeight) {
      makePillars = true;
    }
    for (unsigned y = 0; y < sy; y++) {
      int dy = std::abs(int(y) - int(sy / 2));
      for (unsigned x = 0; x < sx; x++) {
        int dx = std::abs(int(x) - int(sx / 2));
        if (z < zMax ){
          int xbound = sx / bigFrac;
          int ybound = sy / bigFrac;
          if (small) {
            xbound = sx / smallFrac;
            ybound = sy / smallFrac;
          }
          if (dx < xbound || dy < ybound) {
            raft(x, y, z) = 2;
          }
        }
        else if (makePillars) {
          int gridIdxY = dy / pillarSizeY;
          int gridIdxX = dx / pillarSizeX;
          uint8_t id = 1;
          if (gridIdxY % 3 == 1 && gridIdxX % 3 == 1) {
            id = 2;
          }
          raft(x, y, z) = id;
        }
        else {
          raft(x, y, z) = 1;
        }
        //std::cout << int(raft(x, y, z));
      }
      //std::cout << "\n";
    }
    //std::cout << "\n";
  }
  return raft;
}

uint8_t SampleRaft(const Array3D8u &raft , unsigned x, unsigned y, unsigned z)
{
  unsigned coord[3];
  Vec3u size = raft.GetSize();
  if (size[0] == 0 || size[1] == 0 || size[2] == 0) {
    return 1;
  }
  coord[0] = x % size[0];
  coord[1] = y % size[1];
  coord[2] = z;
  if (z >= size[2]) {
    coord[2] = size[2] - 1;
  }
  return raft(coord[0], coord[1], coord[2]);
}

///tiles raft pattern in cell
Array2D8u MakeRaft(uint8_t anchorId, const Array3D8u & raft,
  unsigned z, Vec3u cellGridSize) {
  Vec3u rSize = raft.GetSize();
  Array2D8u slice(cellGridSize[0], cellGridSize[1]);
  uint8_t supId = 1;

  for (unsigned y = 0; y < slice.GetSize()[1]; y++) {
    for (unsigned x = 0; x < slice.GetSize()[0]; x++) {
      slice(x, y) = supId;
      uint8_t pat = SampleRaft(raft, x, y, z);
      if (pat == 2) {
        slice(x, y) = anchorId;
      }      
    }
  }
  return slice;
}

void CopyImage(Array2D8u& dst,
  const Array2D8u& src, unsigned x0, unsigned y0)
{
  for (unsigned x = 0; x < src.GetSize()[0]; x++) {
    if (x + x0 >= dst.GetSize()[0]) {
      break;
    }
    for (unsigned y = 0; y < src.GetSize()[1]; y++) {
      if (y + y0 >= dst.GetSize()[1]) {
        break;
      }
      dst(x + x0, y + y0) = src(x, y);
    }
  }
}

void CopyTransparentImage(Array2D8u& dst,
  const Array2D8u& src, unsigned x0, unsigned y0)
{
  for (unsigned x = 0; x < src.GetSize()[0]; x++) {
    if (x + x0 >= dst.GetSize()[0]) {
      break;
    }
    for (unsigned y = 0; y < src.GetSize()[1]; y++) {
      if (y + y0 >= dst.GetSize()[1]) {
        break;
      }
      if (src(x, y)) {
        dst(x + x0, y + y0) = src(x, y);
      }
    }
  }
}

struct RectPart
{
  Vec2u botSize;
  Vec2u topSize;
  unsigned height;
  unsigned erode;
  uint8_t mat;
  RectPart():height(0), erode(0),mat(2) {
  }
  void Draw(Array2D8u& dst, unsigned x0, unsigned y0, unsigned z) {
    Vec2u maxSize;
    maxSize[0] = std::max(botSize[0], topSize[0]);
    maxSize[1] = std::max(botSize[1], topSize[1]);
    unsigned x1 = x0 + maxSize[0];
    unsigned y1 = y0 + maxSize[1];
    if (x1 > dst.GetSize()[0]) {
      x1 = dst.GetSize()[0];
    }
    if (y1 > dst.GetSize()[1]) {
      y1 = dst.GetSize()[1];
    }

    float weight = z/float(height);
    weight = std::min(1.0f, weight);
    Vec2u rectSize;
    rectSize[0] = botSize[0] * (1.0f - weight) + weight * topSize[0];
    rectSize[1] = botSize[1] * (1.0f - weight) + weight * topSize[1];
    for (unsigned y = y0; y < y1; y++) {
      for (unsigned x = x0; x < x1; x++) {
        dst(x, y) = 1;
      }
    }
    if (z >= height) {
      return;
    }
    Vec2u margin;
    margin[0] = (maxSize[0] - rectSize[0])/2;
    margin[1] = (maxSize[1] - rectSize[1]) / 2;

    y1 = y0 + margin[1] + rectSize[1];
    x1 = x0 + margin[0] + rectSize[0];
    for (unsigned y = y0+margin[1]; y < y1; y++) {
      for (unsigned x = x0+margin[0]; x < x1; x++) {
        dst(x, y) = mat;
        if (x - x0 - margin[0] < erode || x1 - x - 1 < erode) {
          dst(x, y) = 0;
        }
        if (y - y0 - margin[1] < erode || y1 - y - 1 < erode) {
          dst(x, y) = 0;
        }
      }
    }
  }
};

void FillRect(Array2D8u & dst, unsigned x0, unsigned x1,
  unsigned y0, unsigned y1, uint8_t val) {
  for (unsigned y = y0; y < y1; y++) {
    for (unsigned x = x0; x < x1; x++) {
      dst(x, y) = val;
    }
  }
}

void addSpitBar(Array2D8u& slice)
{
  
  std::vector<char> materials(3,1);
  materials[1] = 2;
  materials[2] = 3;
  unsigned spitWidth = 100;
  //right spit bar
  int totalSpitWidth = 2 * spitWidth * materials.size();
  Array2D8u oldSlice = slice;
  Vec2u oldSize = oldSlice.GetSize();
  slice.Allocate(oldSize[0] + totalSpitWidth, oldSize[1]);
  slice.Fill(0);
  for (size_t row = 0; row < oldSize[1]; row++) {
    for (size_t col = 0; col < oldSize[0]; col++) {
      slice(col + totalSpitWidth/2, row) = oldSlice(col, row);
    }
  }
  Vec2u sliceSize = slice.GetSize();
  for (unsigned m = 0; m < materials.size(); m++) {
    for (unsigned x = 0; x < spitWidth; x++) {
      unsigned dstx = (sliceSize[0] - 1) - spitWidth * (unsigned(materials.size()) - m) + x;
      if (dstx < 0 || dstx >= sliceSize[0]) {
        continue;
      }
      for (unsigned y = 0; y < sliceSize[1]; y++) {
        slice(dstx, y) = materials[m];
      }
    }
  }
  for (unsigned m = 0; m < materials.size(); m++) {
    for (unsigned x = 0; x < spitWidth; x++) {
      unsigned dstx = spitWidth * m + x;
      if (dstx < 0 || dstx >= sliceSize[0]) {
        continue;
      }
      for (unsigned y = 0; y < sliceSize[1]; y++) {
        slice(dstx, y) = materials[m];
      }
    }
  }  
}

Array2D8u DrawText(const std::string& s, uint8_t val)
{
  unsigned width = 0;
  unsigned MAXLEN = 10000;
  unsigned len = std::min(unsigned(s.size()), MAXLEN);
  std::vector<FontGlyph> glyphs(len);
  unsigned height = 0;
  Array2D8u pic;
  if (s.size() == 0) {
    return pic;
  }
  for (unsigned i = 0; i < len; i++ ) {
    glyphs[i] = avenir.GetBitmap(s[i]);
    width += glyphs[i].width;
  }  
  height = glyphs[0].height;
  pic.Allocate(width, height, 0);
  unsigned col0 = 0;
  for (unsigned i = 0; i < len; i++) {
    const FontGlyph& g = glyphs[i];
    for (unsigned row = 0; row < height; row++) {
      for (unsigned col = 0; col < g.width; col++) {
        uint8_t input = g(col, row);
        if (input > 0) {
          pic(col + col0, row) = val;
        }
      }
    }
    col0 += g.width;
  }
  return pic;
}

void FullPlateCalib(const ConfigFile& conf)
{
  std::string outDir = conf.getString("outDir");
  std::vector<float> printRes = conf.getFloatVector("printRes");
  std::vector<float> traySize = conf.getFloatVector("traySize");
  std::vector<float> partSize = conf.getFloatVector("partSize");
  std::vector<float> margin = conf.getFloatVector("margin");
  std::vector<float> coating = conf.getFloatVector("coating");
  const unsigned DIM = 3;
  float raft = 1.0f;
  conf.getFloat("raft", raft);
  float totalHeight = raft + partSize[2]+coating[2];
  size_t numLayers = size_t(totalHeight/ printRes[2]);
  //each cell contains a part
  Vec3f cellSize;
  cellSize[0] = partSize[0] + 2 * margin[0] + 2*coating[0];
  cellSize[1] = partSize[1] + 2 * margin[1] + 2 * coating[1];
  cellSize[2] = partSize[2] + raft + coating[2];
  Vec3u coatingPix;
  Vec3u cellGridSize;
  size_t raftLayers = raft / printRes[2];
  for (size_t i = 0; i < DIM; i++) {
    cellGridSize[i] = cellSize[i] / printRes[i];
    coatingPix[i] = coating[i] / printRes[i];
  }
  Vec2u marginPix;
  marginPix[0] = margin[0] / printRes[0];
  marginPix[1] = margin[1] / printRes[1];
  /// cell grid size without the margins
  Vec3u partGridSize;
  partGridSize[0] = (partSize[0] + 2 * coating[0]) / printRes[0];
  partGridSize[1] = (partSize[1] + 2 * coating[1]) / printRes[1];
  partGridSize[2] = (partSize[2]) / printRes[2];

  uint8_t matId = 2;
  if (conf.hasOpt("matId")) {
    matId = conf.getInt("matId");
  }
  Vec2u raftPatSize(400, 200);
  Array3D8u raftPat=MakeRaftPattern(raftPatSize[0], raftPatSize[1], raftLayers);
  Array2D8u raftSlice(raftPat.GetSize()[0], raftPat.GetSize()[1]);
  for (size_t y = 0; y < raftPat.GetSize()[1]; y++) {
    for (size_t x = 0; x < raftPat.GetSize()[0]; x++) {
      raftSlice(x, y) = 50*raftPat(x, y, 0);
    }
  }
  SavePngGrey(outDir + "/raft.png", raftSlice);
  Vec2u numParts;
  Vec3u trayGridSize;
  for (size_t i = 0; i < DIM; i++) {
    trayGridSize[i] = traySize[i] / printRes[i];
  }
  numParts[0] = trayGridSize[0] / cellGridSize[0];
  numParts[1] = trayGridSize[1] / cellGridSize[1];

  Vec2u printSize;
  printSize[0] = numParts[0] * cellGridSize[0];
  printSize[1] = numParts[1] * cellGridSize[1];
  std::vector<RectPart> parts(numParts[0]*numParts[1]);

  unsigned textLayers = 20;
  unsigned textScaleX = 6;
  unsigned textScaleY = 3;
  for (unsigned cx = 0; cx < numParts[0]; cx++) {
    //vertical,expanding or contracting 
    int shape = cx % 3;
    int erode = (cx / 3) % 3;
    for (unsigned cy = 0; cy < numParts[1]; cy++) {
      RectPart& p = parts[cy * numParts[0] + cx];
      p.erode = erode;
      p.height = partGridSize[2];
      p.topSize[0] = unsigned(partSize[0] / printRes[0] + 0.5f);
      p.topSize[1] = unsigned(partSize[1] / printRes[1] + 0.5f);
      p.botSize[0] = unsigned(partSize[0] / printRes[0] + 0.5f);
      p.botSize[1] = unsigned(partSize[1] / printRes[1] + 0.5f);
      if (shape == 1) {
        p.botSize[0] /= 2;
        p.botSize[1] /= 2;
      }
      else if (shape == 2) {
        p.topSize[0] /= 2;
        p.topSize[1] /= 2;
      }
      p.mat = matId;
    }
  }

  for (size_t z = 0; z < numLayers; z++) {
    std::cout << z << "\n";
    Array2D8u slice(printSize[0], printSize[1]);
    slice.Fill(0);
    for (unsigned cx = 0; cx < numParts[0]; cx++) {
      for (unsigned cy = 0; cy < numParts[1]; cy++) {
        unsigned x0 = cx * cellGridSize[0] + marginPix[0];
        unsigned y0 = cy * cellGridSize[1] + marginPix[1];
        if (z <= raftLayers) {
          Array2D8u cellSlice = MakeRaft(matId, raftPat, z, partGridSize);
          CopyImage(slice, cellSlice, x0, y0);
        }
        else {
          RectPart& p = parts[cy * numParts[0] + cx];
          FillRect(slice, x0, x0 + partGridSize[0], y0, y0 + partGridSize[1], 1);
          p.Draw(slice, x0 + coatingPix[0], y0 + coatingPix[1], z-raftLayers-1);
          if (z-raftLayers<textLayers) {
            std::ostringstream oss;
            oss << cx << "," << cy<<" er"<<p.erode;
            std::string label = oss.str();
            Array2D8u text = DrawText(label, 1);
            //text = Scale(text, textScaleX, textScaleY);
            CopyTransparentImage(slice, text, x0 + coatingPix[0] + partGridSize[0] / 3, y0 + coatingPix[1] + partGridSize[1] / 3);
          }
        }
      }
    }
    addSpitBar(slice);
    std::string outname = outDir + std::to_string(z)+".png";
    //for (unsigned y = 0; y < slice.GetSize()[1]; y++) {
    //  for (unsigned x = 0; x < slice.GetSize()[0]; x++) {
    //    slice(x, y) = slice(x, y) * 50;
    //  }
    //}
    SavePngGrey(outname, slice);
  }
}

void TestWritePng() {
  Array2D8u img(500, 400);
  SavePngGrey("test.png", img);
}

void TestDrawText()
{
  std::ostringstream oss;
  oss << "x " << 15 << " y " << 2 << " erode " << 2;
  std::string label = oss.str();
  Array2D8u text = DrawText(label, 50);
  text = Scale(text, 6, 3);
  SavePngGrey("testText.png", text);
}

void TestFont() {
  char c0 = avenir.unicode_first;
  char c1 = avenir.unicode_last;
  for (char c = c0; c <= c1; c++) {
    FontGlyph g = avenir.GetBitmap(c);
    for (uint32_t row = 0; row < g.height; row++) {
      for (uint32_t col = 0; col < g.width; col++) {
        uint8_t bit = g(col, row);
        if (bit) {
          std::cout << "*";
        }
        else {
          std::cout << " ";
        }
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}
