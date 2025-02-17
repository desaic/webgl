#include "MedianBlur.h"
#include "ImageIO.h"
#include "TrigMesh.h"
#include "cpu_voxelizer.h"
#include "Array3D.h"
#include "BBox.h"
#include "Matrix3f.h"
#include "MeshUtil.h"
#include "Grid3DfIO.h"

#include <fstream>
#include <iostream>

//shoe designing stuff.

float SquaredDist(Vec3b a, Vec3b b) {
  float diff = 0;
  for (unsigned c = 0; c < 3; c++) {
    float d = float(a[c]) - b[c];
    diff += d * d;
  }
  return diff;
}

Vec3b rgb2hsl(Vec3b color) {
  float r = color[0] / 255.0f;
  float g = color[1] / 255.0f;
  float b = color[2] / 255.0f;
  uint8_t maxIdx = 0;
  float maxVal = r;
  for (unsigned d = 1; d < 3; d++) {
    if (color[d] > maxVal) {
      maxVal = color[d];
      maxIdx = d;
    }
  }
  float max = std::max(r, std::max(g, b));
  float min = std::min(r, std::min(g, b));
  float c = max - min;
  float hue;
  float segment, shift;
  if (c == 0) {
    hue = 0;
  } else {
    switch (maxIdx) {
      case 0:
        segment = (g - b) / c;
        shift = 0 / 60;      // R° / (360° / hex sides)
        if (segment < 0) {   // hue > 180, full rotation
          shift = 360 / 60;  // R° / (360° / hex sides)
        }
        hue = segment + shift;
        break;
      case 1:
        segment = (b - r) / c;
        shift = 120 / 60;  // G° / (360° / hex sides)
        hue = segment + shift;
        break;
      case 2:
        segment = (r - g) / c;
        shift = 240 / 60;  // B° / (360° / hex sides)
        hue = segment + shift;
        break;
    }
  }
  hue *= 60;
  if (hue < 0) {
    hue += 360;
  }
  if (hue > 360) {
    hue -= 360;
  }
  // Calculate lightness
  float l = (max + min) / 2;
  float delta = max - min;
  // Calculate saturation
  float s = delta == 0 ? 0 : delta / (1 - std::abs(2 * l - 1));
  Vec3b hsl(uint8_t(hue / 360 * 255), uint8_t(s * 255), uint8_t(l * 255));
  return hsl;
}

uint8_t ColorToGray(Vec3b color) {
  Vec3b hsl = rgb2hsl(color);
  float val = (255 - hsl[0]);
  if (hsl[1] < 50) {
    return 0;
  }
  return uint8_t(val);
}

Array2D8u MapColorToGray(const Array2D8u& colorImg) {
  Vec2u size = colorImg.GetSize();
  size[0] /= 3;
  Array2D8u out(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      Vec3b color(colorImg(3 * x, y), colorImg(3 * x + 1, y),
                  colorImg(3 * x + 2, y));
      out(x, y) = ColorToGray(color);
    }
  }
  return out;
}

void SplitLeftRightImage(const Array2D8u& grayIn, Array2D8u& left,
                         Array2D8u& right) {
  Vec2u size = grayIn.GetSize();
  size[0] /= 2;
  left.Allocate(size[0], size[1]);
  right.Allocate(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      left(x, y) = grayIn(x, y);
      right(x, y) = grayIn(x + size[0], y);
    }
  }
}

void MapColorToGrayScale() {
  std::string prefix = "F:/meshes/shoe/walking_gif/frame_";
  std::string leftPrefix = "F:/meshes/shoe/walk_left/";
  std::string rightPrefix = "F:/meshes/shoe/walk_right/";
  int startIdx = 1;
  int endIdx = 1514;
  for (int i = startIdx; i <= endIdx; i++) {
    std::string pngFile = prefix + std::to_string(i) + ".png";
    Array2D8u image;
    LoadPngColor(pngFile, image);
    Array2D8u gray = MapColorToGray(image);
    MedianBlur(gray, gray, 2);
    Array2D8u leftImg, rightImg;
    SplitLeftRightImage(gray, leftImg, rightImg);
    std::string leftFile = leftPrefix + std::to_string(i) + ".png";
    std::string rightFile = rightPrefix + std::to_string(i) + ".png";
    SavePngGrey(leftFile, leftImg);
    SavePngGrey(rightFile, rightImg);
  }
}

void MaxValPerPix() {
  std::string leftPrefix = "F:/meshes/shoe/walk_right/";
  std::string outPrefix = "F:/meshes/shoe/right";
  int startIdx = 1;
  int endIdx = 1514;
  Array2D8u maxImage;
  Array2D8u image0;
  std::string pngFile = leftPrefix + "1.png";
  LoadPngGrey(pngFile, image0);
  maxImage = image0;
  Vec2u size = image0.GetSize();
  for (int i = startIdx; i <= endIdx; i++) {
    pngFile = leftPrefix + std::to_string(i) + ".png";
    Array2D8u image;
    LoadPngGrey(pngFile, image);
    if (image.Empty()) {
      continue;
    }
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        maxImage(x, y) = std::max(maxImage(x, y), image(x, y));
      }
    }
  }

  std::string outImage = outPrefix + "_max.png";
  SavePngGrey(outImage, maxImage);
}

void AvgValPerPix() {
  std::string leftPrefix = "F:/meshes/shoe/walk_left/";
  std::string outPrefix = "F:/meshes/shoe/left";
  std::string maxImageFile = outPrefix + "_max.png";
  int startIdx = 1;
  int endIdx = 1514;
  Array2D8u maxImage;
  Array2D<unsigned> sumImage;
  Array2D<unsigned> countImage;
  LoadPngGrey(maxImageFile, maxImage);
  Vec2u size = maxImage.GetSize();
  sumImage.Allocate(size[0], size[1], 0);
  countImage.Allocate(size[0], size[1], 0);
  for (int i = startIdx; i <= endIdx; i++) {
    std::string pngFile = leftPrefix + std::to_string(i) + ".png";
    Array2D8u image;
    LoadPngGrey(pngFile, image);
    if (image.Empty()) {
      continue;
    }
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (image(x, y) > maxImage(x, y) / 2) {
          sumImage(x, y) += image(x, y);
          countImage(x, y)++;
        }
      }
    }
  }
  Array2D8u avgImage(size[0], size[1]);
  avgImage.Fill(0);
  std::string avgImageFile = outPrefix + "_avg.png";
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      if (countImage(x, y) > 0) {
        avgImage(x,y) = uint8_t(sumImage(x,y)/countImage(x,y));
      }
    }
  }
  SavePngGrey(avgImageFile, avgImage);
}

struct SimpleVoxCb : public VoxCallback {
  SimpleVoxCb(Array3D8u& grid, uint8_t matId) : _grid(grid), _matId(matId) {}
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override {
    _grid(x, y, z) = _matId;
  }
  Array3D8u& _grid;
  uint8_t _matId = 1;
};

void MeshHeightMap() {
  //std::string meshFile = "F:/meshes/shoe_design_kit/sole_r_deform.obj";
  std::string meshFile = "F:/meshes/shoe_design_kit/l_pos.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  voxconf conf;
  Array3D8u voxGrid;
  SimpleVoxCb cb(voxGrid, 1);
  BBox box;
  ComputeBBox(mesh.v, box);

  float h = 0.5;
  float margin = h * 0.5f;
  for (size_t i = 0; i < mesh.GetNumVerts(); i++) {
    mesh.v[3 * i] -= box.vmin[0] - margin;
    mesh.v[3 * i + 1] -= box.vmin[1] - margin;
    mesh.v[3 * i + 2] -= box.vmin[2] - margin;
  }
  //mesh.SaveObj("F:/meshes/shoe_design_kit/r_pos.obj");
  Vec3f boxSize = box.vmax - box.vmin;
  Vec3u gridSize(unsigned(boxSize[0] / h) + 2, unsigned(boxSize[1] / h) + 2,
                 unsigned(boxSize[2] / h) + 2);
  conf.unit = h;
  voxGrid.Allocate(gridSize, 0);
  conf.gridSize = gridSize;
  cpu_voxelize_mesh(conf, &mesh, cb);
  //SaveVolAsObjMesh("F:/meshes/shoe/shoe_vol.obj", voxGrid, (float*)(&conf.unit),
  //                 1);
  Array2D8u height(gridSize[0], gridSize[1]);
  for (unsigned y = 0; y < gridSize[1]; y++) {
    for (unsigned x = 0; x < gridSize[0]; x++) {
      for (unsigned z = gridSize[2] - 1; z > 0; z--) {
        if (voxGrid(x, y, z) > 0) {
          height(x, y) = z;
          break;
        }
      }
    }
  }
  SavePngGrey("F:/meshes/shoe_design_kit/height.png", height);
}

void MapDrawingToThickness() { 
  
  //std::string pressureFile = "F:/meshes/shoe/right_drawing_1mm.png";
  std::string pressureFile = "F:/meshes/shoe_design_kit/draw_r_1mm.png";
  Array2D8u pressureImage; 
  LoadPngGrey(pressureFile, pressureImage);

  std::string meshFile = "F:/meshes/shoe_design_kit/r_pos.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  voxconf conf;
  Array3D8u voxGrid;
  SimpleVoxCb cb(voxGrid, 1);
  BBox box;
  ComputeBBox(mesh.v, box);
  float h = 1;
  float margin = 0.25f * h;
  for (size_t i = 0; i < mesh.GetNumVerts(); i++) {
    mesh.v[3 * i] -= box.vmin[0] - margin;
    mesh.v[3 * i + 1] -= box.vmin[1] - margin;    
    mesh.v[3 * i + 2] -= box.vmin[2] - margin;    
  }
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  
  Vec3f boxSize = box.vmax - box.vmin;
  Vec3u size(unsigned(boxSize[0] / h) + 1, unsigned(boxSize[1] / h) + 1,
                 unsigned(boxSize[2] / h) + 1);
  conf.unit = h;
  voxGrid.Allocate(size, 0);
  conf.gridSize = size;
  cpu_voxelize_mesh(conf, &mesh, cb);

  //for diamond cell
  float cellSize = 2.0;
  // about 50% density
  float defaultThickness = 0.35;
  float maxThickness = 0.6;
  float minThickness = 0.18;
  //thickness grid
  slicer::Grid3Df tGrid(size, 0);
  Vec2u imageSize = pressureImage.GetSize();

  Array2D8u zStartImage(size[0], size[1]);
  zStartImage.Fill(0);
  Array2D8u zEndImage(size[0], size[1]);
  zEndImage.Fill(0);
  uint8_t maxPressure = 0;
  for (auto v : pressureImage.GetData()) {
    maxPressure = std::max(maxPressure, v);
  }
  std::cout << int(maxPressure )<<"\n";
  for (unsigned y = 0; y < size[1]; y++) {
    if (y >= imageSize[1]) {
      continue;
    }
    for (unsigned x = 0; x < size[0]; x++) {
      if (x >= imageSize[0]) {
        continue;
      }

      uint8_t pressure = pressureImage(x, y);
      unsigned zStart = 0;
      unsigned zEnd = 0;
      bool hasVox = false;
      for (unsigned z = 0; z < size[2]; z++) {
        if (voxGrid(x, y, z) > 0) {
          hasVox = true;
          zStart = z;
          break;
        }
      }
      if (!hasVox) {
        continue;
      }
      for (int z = int(size[2]) - 1; z >= 0; z--) {
        if (voxGrid(x, y, z) > 0) {
          zEnd = z;
          break;
        }
      }
      if (!hasVox) {
        continue;
      }
      //zStartImage(x, y) = zStart;
      //zEndImage(x, y) = zEnd;
      float alpha = pressure / float(maxPressure);
      float startThickness =
          maxThickness * alpha + (1 - alpha) * defaultThickness;
      float endThickness =
          minThickness * alpha + (1 - alpha) * defaultThickness;
      if (zEnd <= zStart) {
        tGrid(x, y, zStart) = startThickness;
        continue;
      }
      for (unsigned z = zStart; z <= zEnd; z++) {
        float beta = (z - zStart) / float(zEnd - zStart);
        float t = (1 - beta) * startThickness + beta * endThickness;
        tGrid(x, y, z) = t;
        if (z == zStart) {
          zStartImage(x, y) = t * 50;          
        } else if (z == zEnd) {
          zEndImage(x, y) = t*50;
        }
      }
    }
  }
  std::ofstream gridOut("F:/meshes/shoe_design_kit/thick_diamond_r.txt");
  slicer::SaveTxt(tGrid, gridOut, 3);
  SavePngGrey("F:/meshes/shoe_design_kit/zStart.png", zStartImage);
  SavePngGrey("F:/meshes/shoe_design_kit/zEnd.png", zEndImage);
}
