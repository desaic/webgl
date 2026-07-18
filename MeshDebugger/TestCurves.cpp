#include <cmath>
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

static void WriteTubeCurve(std::ofstream& out, const LinearCurve& curve, float radius, int radialSegments, size_t& globalVertBase) { 
    if (curve.points.size() < 2) { return; } 
    
    std::vector<Vec3f> path = curve.points; 
    if (curve.isLoop) { 
        path.push_back(path[0]); 
    } 
    size_t numPath = path.size(); 
    int ringVerts = radialSegments + 1; 

    // 1. Calculate continuous tangents
    std::vector<Vec3f> tangents(numPath);
    for (size_t i = 0; i < numPath; i++) {
        Vec3f t(0.0f, 0.0f, 0.0f);
        if (curve.isLoop) {
            // For loops, wrap around smoothly to avoid tangent seam pops
            size_t prevIdx = (i == 0) ? (numPath - 2) : (i - 1);
            size_t nextIdx = (i == numPath - 1) ? 1 : (i + 1);
            t = t + (path[i] - path[prevIdx]).normalizedCopy();
            t = t + (path[nextIdx] - path[i]).normalizedCopy();
        } else {
            if (i > 0)           t = t + (path[i] - path[i - 1]).normalizedCopy();
            if (i < numPath - 1) t = t + (path[i + 1] - path[i]).normalizedCopy();
        }
        tangents[i] = t.normalizedCopy();
    }

    // 2. Build the initial reference frame
    std::vector<Vec3f> normals(numPath); 
    std::vector<Vec3f> binormals(numPath); 

    Vec3f t0 = tangents[0];
    Vec3f up(0.0f, 1.0f, 0.0f);
    if (std::abs(t0[0]) > 0.9f) {
        up = Vec3f(0.0f, 0.0f, 1.0f); // Fallback to Z-axis if tangent is aligned with X
    }
    
    normals[0] = t0.cross(up).normalizedCopy();
    binormals[0] = t0.cross(normals[0]).normalizedCopy();

    // 3. Discrete Parallel Transport via Frame Projection
    for (size_t i = 1; i < numPath; i++) { 
        Vec3f t = tangents[i];
        
        // Project the previous normal onto the plane perpendicular to the current tangent
        Vec3f n = normals[i - 1] - t * t.dot(normals[i - 1]);
        normals[i] = n.normalizedCopy(); 
        
        // Recompute binormal to guarantee perfect orthogonality
        binormals[i] = t.cross(normals[i]).normalizedCopy(); 
    } 

    // 4. Generate & write vertices
    size_t ringsToWrite = curve.isLoop ? (numPath - 1) : numPath; 
    const float tau = 6.28318530f; 
    
    for (size_t i = 0; i < ringsToWrite; i++) { 
        for (int j = 0; j <= radialSegments; j++) { 
            float angle = tau * float(j) / float(radialSegments); 
            float ca = std::cos(angle); 
            float sa = std::sin(angle); 
            
            // Normals and binormals are now perfectly orthogonal and uniform
            Vec3f n = normals[i] * ca + binormals[i] * sa; 
            Vec3f pos = path[i] + n * radius; 
            
            out << "v " << pos[0] << " " << pos[1] << " " << pos[2] << "\n"; 
        } 
    } 

    // 5. Connect Quad Faces
    size_t facesBeg = ringsToWrite - 1; 
    if (curve.isLoop && ringsToWrite >= 2) { 
        facesBeg = ringsToWrite; 
    } 

    for (size_t i = 0; i < facesBeg; i++) { 
        size_t cur = i % ringsToWrite; 
        size_t nxt = (i + 1) % ringsToWrite; 
        size_t curBase = globalVertBase + cur * ringVerts; 
        size_t nxtBase = globalVertBase + nxt * ringVerts; 

        for (int j = 0; j < radialSegments; j++) { 
            unsigned a = curBase + j; 
            unsigned b = curBase + j + 1; 
            unsigned c = nxtBase + j; 
            unsigned d = nxtBase + j + 1; 

            // Standard CCW Winding Order
            out << "f " << a + 1 << " " << c + 1 << " " << b + 1 << "\n"; 
            out << "f " << b + 1 << " " << c + 1 << " " << d + 1 << "\n"; 
        } 
    } 

    globalVertBase += ringsToWrite * ringVerts; 
}

void CurvesToObj(const std::string& inFile, const std::string& outFile) {
  auto curves = LoadCurves(inFile);
  if (curves.empty()) {
    return;
  }
  std::ofstream out(outFile);
  out << std::fixed;
  out.precision(6);

  float radius = 0.35f;
  int radialSegments = 8;
  size_t globalVertBase = 0;
  size_t totalVerts = 0;
  size_t totalFaces = 0;

  for (size_t ci = 0; ci < curves.size(); ci++) {
    const auto& curve = curves[ci];
    if (curve.points.size() < 2) continue;

    size_t rings = curve.isLoop ? curve.points.size() : curve.points.size();
    totalVerts += rings * (radialSegments + 1);
    totalFaces += (rings - 1) * radialSegments * 2;
  }
  std::cout << "estimated: " << totalVerts << " verts, " << totalFaces
            << " tris\n";

  for (size_t ci = 0; ci < curves.size(); ci++) {
    const auto& curve = curves[ci];
    if (curve.points.size() < 2) continue;
    WriteTubeCurve(out, curve, radius, radialSegments, globalVertBase);
    if (ci % 5 == 0) {
      std::cout << "  curve " << ci << "/" << curves.size()
                << " done, verts so far: " << globalVertBase << "\n";
    }
  }
  out.close();
  std::cout << "total: " << globalVertBase << " verts, " << totalFaces
            << " tris -> " << outFile << "\n";
}
