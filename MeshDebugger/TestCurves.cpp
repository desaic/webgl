#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Vec3.h"

struct LinearCurve {
  bool isLoop = false;
  std::vector<Vec3f> points;
};

static std::vector<LinearCurve> LoadCurves(const std::string& filename) {
  std::vector<LinearCurve> curves;
  std::ifstream in(filename);
  if (!in) {
    std::cerr << "failed to open " << filename << "\n";
    return curves;
  }
  unsigned numCurves = 0;
  in >> numCurves;
  std::cout << numCurves << " curves\n";

  for (unsigned c = 0; c < numCurves; c++) {
    unsigned isLoopVal = 0, numPts = 0;
    in >> isLoopVal >> numPts;
    if (!in) {
      std::cerr << "read error at curve header " << c << "\n";
      break;
    }
    LinearCurve curve;
    curve.isLoop = (isLoopVal != 0);
    curve.points.reserve(numPts);

    for (unsigned i = 0; i < numPts; i++) {
      float x, y, z;
      in >> x >> y >> z;
      if (!in) {
        std::cerr << "read error at curve " << c << " point " << i << "\n";
        break;
      }
      curve.points.emplace_back(x, y, z);
    }
    curves.push_back(std::move(curve));
  }
  return curves;
}

void CurvesToObj(const std::string& inFile, const std::string& outFile) {
  auto curves = LoadCurves(inFile);
  if (curves.empty()) {
    return;
  }
  std::ofstream out(outFile);
  out << std::fixed;
  out.precision(6);
  size_t vertCount = 0;

  for (size_t ci = 0; ci < curves.size(); ci++) {
    const auto& curve = curves[ci];
    for (size_t i = 0; i < curve.points.size(); i++) {
      const auto& p = curve.points[i];
      out << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
      vertCount++;
    }
    if (curve.isLoop && curve.points.size() >= 2) {
      const auto& p0 = curve.points.front();
      const auto& p1 = curve.points.back();
      Vec3f d = p1 - p0;
      float dist = d.norm();
      std::cout << "curve " << ci << " loop, "
                << curve.points.size() << " pts, end-to-end dist: "
                << dist << "\n";
      out << "l " << (vertCount - curve.points.size() + 1)
          << " " << vertCount << "\n";
    }
  }
  out.close();
  std::cout << "wrote " << vertCount << " verts to " << outFile << "\n";
}
