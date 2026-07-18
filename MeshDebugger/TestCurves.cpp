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
    size_t origSize = curve.points.size();
    if (curve.isLoop && curve.points.size() > 10) {
      const float minDistSq = 0.1f * 0.1f;
      while (curve.points.size() > 10) {
        const auto& lastPt = curve.points.back();
        bool tooClose = false;
        for (size_t k = 0; k < 10; k++) {
          Vec3f d = lastPt - curve.points[k];
          if (d.dot(d) < minDistSq) {
            tooClose = true;
            break;
          }
        }
        if (tooClose) {
          curve.points.pop_back();
        } else {
          break;
        }
      }
    }
    if (curve.points.size() < origSize) {
      std::cout << "  curve " << c << " trimmed " << (origSize - curve.points.size())
                << " trailing pts (" << curve.points.size() << " remain)\n";
    }
    curves.push_back(std::move(curve));
  }
  return curves;
}
static void WriteTubeCurve(std::ofstream& out, const LinearCurve& curve, float radius, int radialSegments, size_t& globalVertBase) { 
    if (curve.points.size() < 2) { return; } 
    
    std::vector<Vec3f> path = curve.points; 
    size_t numPath = path.size(); 
    int ringVerts = radialSegments + 1; 

    // 1. Calculate continuous tangents with seamless wrap-around for loops
    std::vector<Vec3f> tangents(numPath);
    for (size_t i = 0; i < numPath; i++) {
        Vec3f t(0.0f, 0.0f, 0.0f);
        if (curve.isLoop) {
            // Smoothly blend the tangents using cyclic array boundaries
            size_t prevIdx = (i == 0) ? (numPath - 1) : (i - 1);
            size_t nextIdx = (i == numPath - 1) ? 0 : (i + 1);
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
        up = Vec3f(0.0f, 0.0f, 1.0f);
    }
    
    normals[0] = t0.cross(up).normalizedCopy();
    binormals[0] = t0.cross(normals[0]).normalizedCopy();

    // 3. Discrete Parallel Transport via Frame Projection
    for (size_t i = 1; i < numPath; i++) { 
        Vec3f t = tangents[i];
        Vec3f n = normals[i - 1] - t * t.dot(normals[i - 1]);
        normals[i] = n.normalizedCopy(); 
        binormals[i] = t.cross(normals[i]).normalizedCopy(); 
    } 

    // 4. Generate & write vertices
    size_t ringsToWrite = numPath; 
    const float tau = 6.28318530f; 
    
    for (size_t i = 0; i < ringsToWrite; i++) { 
        for (int j = 0; j <= radialSegments; j++) { 
            float angle = tau * float(j) / float(radialSegments); 
            float ca = std::cos(angle); 
            float sa = std::sin(angle); 
            
            Vec3f n = normals[i] * ca + binormals[i] * sa; 
            Vec3f pos = path[i] + n * radius; 
            
            out << "v " << pos[0] << " " << pos[1] << " " << pos[2] << "\n"; 
        } 
    } 

    // 5. Determine the nearest-matching vertex offset for loop closure
    int closeOffset = 0;
    if (curve.isLoop && ringsToWrite >= 2) {
        float minDstSq = 1e30f;
        // Reference point from the last written ring (ringsToWrite - 1)
        Vec3f lastV0 = path[ringsToWrite - 1] + normals[ringsToWrite - 1] * radius;

        // Find the matching nearest vertex index 'k' in the first ring (i = 0)
        for (int k = 0; k < radialSegments; k++) {
            float angle = tau * float(k) / float(radialSegments);
            Vec3f firstVk = path[0] + (normals[0] * std::cos(angle) + binormals[0] * std::sin(angle)) * radius;
            
            Vec3f diff = lastV0 - firstVk;
            float dstSq = diff.dot(diff);
            if (dstSq < minDstSq) {
                minDstSq = dstSq;
                closeOffset = k;
            }
        }
    }

    // 6. Connect Quad Faces with Seam Alignment
    size_t facesBeg = curve.isLoop ? ringsToWrite : (ringsToWrite - 1); 

    for (size_t i = 0; i < facesBeg; i++) { 
        size_t cur = i;
        size_t nxt = (i + 1) % ringsToWrite; 
        
        size_t curBase = globalVertBase + cur * ringVerts; 
        size_t nxtBase = globalVertBase + nxt * ringVerts; 

        bool isClosingStrip = (curve.isLoop && (i == facesBeg - 1));

        for (int j = 0; j < radialSegments; j++) { 
            unsigned a = curBase + j; 
            unsigned b = curBase + j + 1; 
            
            unsigned c, d;
            if (isClosingStrip) {
                // Apply the index offset to connect straight across the closing gap
                c = nxtBase + ((j + closeOffset) % radialSegments);
                d = nxtBase + ((j + 1 + closeOffset) % radialSegments);
            } else {
                c = nxtBase + j;
                d = nxtBase + j + 1;
            }

            out << "f " << a + 1 << " " << b + 1 << " " << c + 1 << "\n"; 
            out << "f " << b + 1 << " " << d+ 1 << " " << c + 1 << "\n"; 
        } 
    } 

    globalVertBase += ringsToWrite * ringVerts; 
}

static LinearCurve TrimCurve(const LinearCurve& src, size_t c) {
  const float minDistSq = 0.1f * 0.1f;
  const size_t lookBack = 20;

  LinearCurve dst;
  dst.isLoop = src.isLoop;
  dst.points.reserve(src.points.size());

  for (const auto& pt : src.points) {
    bool tooClose = false;
    size_t start = dst.points.size() > lookBack ? dst.points.size() - lookBack : 0;
    for (size_t i = start; i < dst.points.size(); i++) {
      Vec3f d = pt - dst.points[i];
      if (d.dot(d) < minDistSq) {
        tooClose = true;
        break;
      }
    }
    if (!tooClose) {
      dst.points.push_back(pt);
    }
  }
  size_t removed = src.points.size() - dst.points.size();
  if (removed > 0) {
    std::cout << "  curve " << c << " trimmed " << removed
              << " pts (" << src.points.size() << " -> " << dst.points.size() << ")\n";
  }
  return dst;
}

void CurvesToObj(const std::string& inFile, const std::string& outFile) {
  std::vector<LinearCurve> curves = LoadCurves(inFile);
  if (curves.empty()) {
    return;
  }

  // Trim all curves, save trimmed points
  std::vector<LinearCurve> trimmed(curves.size());
  size_t totalTrimmedPts = 0;
  {
    std::ofstream ptsOut("points.obj");
    ptsOut.precision(6);
    for (size_t ci = 0; ci < curves.size(); ci++) {
      trimmed[ci] = TrimCurve(curves[ci], ci);
      for (const auto& p : trimmed[ci].points) {
        ptsOut << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
      }
      totalTrimmedPts += trimmed[ci].points.size();
    }
    ptsOut.close();
    std::cout << "trimmed points.obj: " << totalTrimmedPts << " verts\n";
  }

  // Write tube mesh from trimmed curves
  std::ofstream out(outFile);
  out << std::fixed;
  out.precision(6);

  float radius = 0.35f;
  int radialSegments = 8;
  size_t globalVertBase = 0;
  size_t totalVerts = 0;
  size_t totalFaces = 0;

  for (size_t ci = 0; ci < trimmed.size(); ci++) {
    const auto& curve = trimmed[ci];
    if (curve.points.size() < 2) continue;
    size_t rings = curve.points.size();
    totalVerts += rings * (radialSegments + 1);
    totalFaces += (rings - 1) * radialSegments * 2;
  }
  std::cout << "estimated: " << totalVerts << " verts, " << totalFaces
            << " tris\n";

  for (size_t ci = 0; ci < trimmed.size(); ci++) {
    const auto& curve = trimmed[ci];
    if (curve.points.size() < 2) continue;
    WriteTubeCurve(out, curve, radius, radialSegments, globalVertBase);
    if (ci % 5 == 0) {
      std::cout << "  curve " << ci << "/" << trimmed.size()
                << " done, verts so far: " << globalVertBase << "\n";
    }
  }
  out.close();
  std::cout << "total: " << globalVertBase << " verts, " << totalFaces
            << " tris -> " << outFile << "\n";
}
