#include <fstream>
#include <iostream>
#include <string>
#include "Matrix3f.h"

#include "TrigMesh.h"
#include "stddef.h"

float LuneRho(float R, float r, float epsI) {
  if (r >= R) {
    return 0;
  }
  float r2 = r * r;
  float rho = (R * R - r2) * ((epsI + 4) * R * R - 2 * r2);
  rho /= 3 * R * R * (epsI - 1) * (2 * R * R - r2);
  if (rho < 0) {
    rho = 0;
  }
  if (rho > 1) {
    rho = 1;
  }
  return rho;
}

float clamp(float x, float lb, float ub) {
  if (x < lb) {
    return lb;
  }
  if (x > ub) {
    return ub;
  }
  return x;
}

float rhombusScale(float w, float cellSize) {
  return (cellSize / 2 * 1.41421 - w) / 2.0f;
}

float WToRho(float w, float cellSize) {
  float g = 0.2 / w;
  float r = g / 2;
  float cylLen = w / 1.41421;
  float pi = 3.14159265f;
  float cylVol = 12 * 1.5f * cylLen * pi * r * r;
  float cubeVol = cellSize * cellSize * cellSize;
  float rhombScale = rhombusScale(w, cellSize);
  float refRhombVol = 5.649409;
  float rhomVol = 3 * refRhombVol * rhombScale * rhombScale * rhombScale;
  float rho = (cubeVol - rhomVol - cylVol) / cubeVol;
  return rho;
}

void SaveVector(const std::string& file, const std::vector<float>& v) {
  std::ofstream out(file);
  for (size_t i = 0; i < v.size(); i++) {
    out << v[i] << "\n";
  }
  out.close();
}

float lerp(float a, float b, float t) { return a + t * (b - a); }

class LinearInterp {
 public:
  LinearInterp() {}
  float x0 = 0;
  float dx = 1;
  std::vector<float> y;
  float interp(float x) const {
    if (x < x0) {
      return y[0];
    }
    size_t N = y.size();
    if (x >= x0 + dx * (N - 1)) {
      return y.back();
    }

    unsigned idx = unsigned((x - x0) / dx);
    float alpha = 1 - (x - idx * dx);
    return lerp(y[idx], y[idx + 1], alpha);
  }
};

float wToDrain(float w) { return 1.0 / (5 * w); }

// unit length reference cylinder
// along x axis.
class RefCylinder {
public:
  TrigMesh mesh;
  //0 for left, 1 for right.
  std::vector<uint32_t> vertSide;
  
  void GroupVerts() {
    size_t nV = mesh.GetNumVerts();
    vertSide.resize(nV, 0);
    for (unsigned i = 0; i < mesh.GetNumVerts(); i++) {      
      if (mesh.v[3 * i] < 0.1) {
        vertSide[i] = 0;
      } else {
        vertSide[i] = 1;
      }
    }
  }

  TrigMesh MakeScaledCopy(float leftDiameter, float rightDiameter, float length) {
    TrigMesh out = mesh;
    size_t nV = mesh.GetNumVerts();
    for (size_t i = 0; i < nV; i++) {
      Vec3f v = out.GetVertex(i);
      if (vertSide[i] == 0) {
        v[1] *= leftDiameter;
        v[2] *= leftDiameter;
      } else {
        v[0] *= length;
        v[1] *= rightDiameter;
        v[2] *= rightDiameter;
      }
      out.v[3 * i] = v[0];
      out.v[3 * i+1] = v[1];
      out.v[3 * i + 2] = v[2];
    }
    return out;
  }
};

void Transform(TrigMesh& mesh, const Matrix3f& mat) {
  size_t nV = mesh.GetNumVerts();
  for (size_t i = 0; i < nV; i++) {
    Vec3f v = mesh.GetVertex(i);
    v = mat * v;
    mesh.v[3 * i] = v[0];
    mesh.v[3 * i + 1] = v[1];
    mesh.v[3 * i + 2] = v[2];
  }
}

void MakeRhombicLattice() {
  TrigMesh rhombic;
  std::vector<float> w(46);
  std::vector<float> wToRho(w.size());
  float wMin = 0.205;
  float wMax = 0.633;
  float w0 = 0.1;
  float w1 = 1;
  float wStep = 0.02;
  const float cellSize = 2.5;
  const float PI = 3.14159265f;
  for (unsigned i = 0; i < wToRho.size(); i++) {
    float wi = w0 + i * wStep;
    float rho = WToRho(wi, cellSize);
    w[i] = wi;
    wToRho[i] = rho;
  }

  int numSamples = 50;
  std::vector<float> rhoUniform(numSamples);  

  float rhoMin = wToRho.front();
  float rhoMax = wToRho.back();
  float rhoStep = (rhoMax - rhoMin) / (numSamples - 1);

  std::string dataDir = "F:/meshes/lune_rhombic/";
  std::string rhombicFile = "F:/meshes/lune_rhombic/rhombus.obj";
  rhombic.LoadObj(rhombicFile);
  RefCylinder cyl;
  std::string cylFile = "F:/meshes/lune_rhombic/cyl.obj";
  cyl.mesh.LoadObj(cylFile);
  cyl.GroupVerts();

  LinearInterp rhoToW;
  rhoToW.x0 = rhoMin;
  rhoToW.dx = rhoStep;
  rhoToW.y.resize(numSamples);
  // Inverse Mapping
  for (int j = 0; j < numSamples; j++) {
    float targetRho = rhoMin + j * rhoStep;
    rhoUniform[j] = targetRho;

    // Find where targetRho sits in the original wToRho array
    auto it = std::lower_bound(wToRho.begin(), wToRho.end(), targetRho);

    if (it == wToRho.begin()) {
      rhoToW.y[j] = w.front();
    } else if (it == wToRho.end()) {
      rhoToW.y[j] = w.back();
    } else {
      // Perform linear interpolation between the two surrounding points
      size_t idx1 = std::distance(wToRho.begin(), it) - 1;
      size_t idx2 = idx1 + 1;

      float r1 = wToRho[idx1];
      float r2 = wToRho[idx2];

      // Calculate how far targetRho is between r1 and r2 (0.0 to 1.0)
      float t = (targetRho - r1) / (r2 - r1);

      // Map that same "t" to the w values
      rhoToW.y[j] = lerp(w[idx1], w[idx2], t);
    }
  }

  size_t numCells = 21;
  size_t gridSize = numCells * 2 + 1;
  float epsI = 2.35;
  float spRadius = (float(numCells) / 2.0f) * cellSize;
  TrigMesh all;
  // place rhombic dodechedrons
  for (unsigned z = 0; z < gridSize; z++) {
    for (unsigned y = 0; y < gridSize; y++) {
      for (unsigned x = 0; x < gridSize; x++) {
        if ((x + y + z) % 2 == 1) {
          continue;
        }
        // only place at face center and vertices.
        float fx = (float(x) - numCells) / float(numCells);
        float fy = (float(y) - numCells) / float(numCells);
        float fz = (float(z) - numCells) / float(numCells);
        float r = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (r * spRadius - spRadius > 0.5f * cellSize) {
          continue;
        }
        float rho = LuneRho(1, r, epsI);
        float w = rhoToW.interp(rho);
        w = clamp(w, wMin, wMax);
        float scale = rhombusScale(w, cellSize);
        TrigMesh rCopy = rhombic;
        Vec3f coord(float(x) - numCells, float(y) - numCells,
                    float(z) - numCells);
        coord = 0.5f * cellSize * coord;
        rCopy.scale(scale);
        rCopy.translate(coord[0], coord[1], coord[2]);
        all.append(rCopy);
      }
    }
  }
  float rMax = wToDrain(wMin);
  float rMin = wToDrain(wMax);
  TrigMesh drainMesh;
  // place drain cylinders in xyz directions
  for (unsigned z = 0; z < gridSize; z++) {
    for (unsigned y = 0; y < gridSize; y++) {
      float zCoord = 0.5f * cellSize * (float(z) - numCells);
      float yCoord = 0.5f * cellSize * (float(y) - numCells);
      float yzDist2 = zCoord * zCoord + yCoord * yCoord;
      float radius2 = spRadius * spRadius;
      float xDist = std::sqrt(radius2 - std::min(radius2, yzDist2));
      float yzDist = std::sqrt(yzDist2);
      if (yzDist - spRadius >  0.5f * cellSize) {
        continue;
      }
      //make sure drainage reach all the way out
      xDist += wMax;
      float rho = LuneRho(spRadius, yzDist, epsI);
      float w = rhoToW.interp(rho);
      w = clamp(w, wMin, wMax);
      float r0 = wToDrain(w);
      r0 = std::max(r0, rMin);
      float r1 = rMax;
      TrigMesh cylPositive = cyl.MakeScaledCopy(r0, r1, xDist);
      TrigMesh cylNeg = cylPositive;
      Matrix3f rot = Matrix3f::rotateZ(PI);
      Transform(cylNeg, rot);
      cylPositive.translate(0, yCoord, zCoord);
      cylNeg.translate(0, yCoord, zCoord);
      drainMesh.append(cylPositive);
      drainMesh.append(cylNeg);

      Matrix3f xToY = Matrix3f::rotateZ(0.5f * PI);
      TrigMesh yPositive = cylPositive;
      TrigMesh yNeg = cylNeg;
      Transform(yPositive, xToY);
      Transform(yNeg, xToY);
      drainMesh.append(yPositive);
      drainMesh.append(yNeg);

      Matrix3f xToZ = Matrix3f::rotateY(0.5f * PI);
      Transform(cylPositive, xToZ);
      Transform(cylNeg, xToZ);
      drainMesh.append(cylPositive);
      drainMesh.append(cylNeg);
    }
  }
  all.append(drainMesh);
  all.SaveObj(dataDir + "/rhombus_all.obj");
  
}