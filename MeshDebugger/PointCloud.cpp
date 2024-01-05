#include "PointCloud.h"

#include "BBox.h"
#include "Array2D.h"
#include "ImageIO.h"
#include "ImageUtils.h"
#include "StringUtil.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

void LoadPCD(const std::string& file, PcdPoints& points) {
  std::cout << "loading " << file << "\n";
  std::ifstream in(file, std::ios_base::binary);
  std::string line;
  size_t numPoints = 0;
  std::string dataType = "binary";
  while (std::getline(in, line)) {
    if (line.size() == 0) {
      continue;
    }
    if (line[0] == '#') {
      continue;
    }
    std::istringstream iss(line);
    std::string tok;
    iss >> tok;
    if (tok == "DATA") {
      iss >> dataType;      
      break;
    }
    if (tok == "POINTS") {
      iss >> numPoints;      
    } else if (tok == "SIZE") {
      unsigned count = 0;
      unsigned bytes = 0;
      while (iss >> bytes) {
        count++;
      }
      points.floatsPerPoint = count;
    }
    continue;
  }
  if (dataType == "binary") {
    points.Allocate(points.floatsPerPoint * numPoints);
    in.read((char*)points.data(), points.NumBytes());
  } else {
    std::cout<<"unimplemented data type "<<dataType<<"\n";
  }
}

void ComputeBBox(BBox & box, const PcdPoints& points) {
  if (points.NumPoints() == 0) {
    return;
  }
  
  Vec3f * pt = (Vec3f*)points[0] ;
  box.vmin = *pt;
  box.vmax = *pt;
  for (size_t i = 1; i < points.NumPoints(); i++) {
    pt = (Vec3f*)(points[i]);
    for (unsigned d = 0; d < 3; d++) {
      box.vmin[d] = std::min((*pt)[d], box.vmin[d]);
      box.vmax[d] = std::max((*pt)[d], box.vmax[d]);
    }
  }
}

void PreviewPCDToPng(std::string pcdfile) { 
  PcdPoints points;
  LoadPCD(pcdfile, points); 
  BBox box;
  ComputeBBox(box,points);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  Vec3f at = 0.5f * (box.vmin + box.vmax);
  Vec3f size = box.vmax = box.vmin;
  float diagLen = size.norm();
  float eyeDist = 5 * diagLen;
  Vec3f eye = box.vmax + eyeDist * ((box.vmax-box.vmin).normalizedCopy());

  const unsigned IMAGE_WIDTH = 800;
  Array2D8u image(IMAGE_WIDTH, IMAGE_WIDTH);
  std::filesystem::path p(pcdfile);
  std::string parent = p.parent_path().string();
  std::string filename = p.filename().string();
  std::string name = remove_suffix(filename, ".");
  std::string imageFile = parent + "/" + name + ".png";
  
  image.Fill(0);

  Vec3f up(0, 1, 0);

  Vec3f dir = at-eye;
  dir.normalize();
  Vec3f xAxis = up.cross(dir);
  xAxis.normalize();
  up = dir.cross(xAxis);

  Vec2u imageSize = image.GetSize();
  float aspect = imageSize[0] / (float)imageSize[1];
  float fov = 0.785;
  float xLen = eyeDist * std::tan(fov);
  bool hasColor = (points.floatsPerPoint == 4);
  unsigned drawn = 0;
  for (size_t i = 0; i < points.NumPoints(); i++) {
    float * ptr = points[i];
    Vec3f pos = *(Vec3f*)ptr;
    uint32_t color = 128;
    if (hasColor) {
      color = *(uint32_t*)(ptr+3);
      RGBToGrey(color);
    }
    Vec3f d = pos - eye;
    float dist = d.dot(dir);
    if (dist < 0.0001f) {
      continue;
    }
    if (!hasColor) {
      color = 255 - uint8_t(std::min(255.0f, 128.0f * dist / eyeDist));
    }
    float u = (1.0f / dist) * d.dot(xAxis);
    float v = (1.0f / dist) * d.dot(up);
    unsigned col = imageSize[0] / 2 + imageSize[0] * u / xLen;
    unsigned row = imageSize[1] / 2 + imageSize[1] * v / xLen;
    if (col < imageSize[0] && row < imageSize[1]) {
      image(col, row) = uint8_t(color);
      drawn++;
    }
  }
  std::cout << "draw "<<drawn<<" out of " << points.NumPoints() << "\n";
  SavePngGrey(imageFile, image);
}

int pcdDirId = -1;

void ProcessPCDDir(const std::string & dir) {
  try {
    for (auto it = std::filesystem::directory_iterator(dir);
         it != std::filesystem::directory_iterator{}; ++it) {
      if (it->is_regular_file()) {
        std::string pcdfile = it->path().string();
        std::string suffix = get_suffix(pcdfile);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       ::tolower);
        if (suffix != "pcd") {
          continue;
        }
        PreviewPCDToPng(pcdfile);
      } else {
        ProcessPCDDir(it->path().string());
      }
    }
  } catch (std::exception e) {
    std::cout << e.what() << "\n";
  }
}

void TestPCD() {
  std::string dir =
      "H:/packing/cardinal_bin_packing_"
      "dataset/partial_gocarts_dataset";
  ProcessPCDDir(dir);
}
