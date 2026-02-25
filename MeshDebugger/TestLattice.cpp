#include "TrigMesh.h"
#include "stddef.h"

float LuneRho(float R, float r, float epsI) {
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

float WToRho(float w, float cellSize) { 
  float g = 0.2 / w;
  float r = g / 2;
  float cylLen = w / 1.41421;
  float pi = 3.14159265f;
  float cylVol = 12 * 1.5f * cylLen * pi * r * r;
  float cubeVol = cellSize * cellSize * cellSize;
  float rhombScale = (cellSize / 2 * 1.41421 - w) / 2.0f;
  float refRhombVol = 5.649409;  
  float rhomVol = 3 * refRhombVol * rhombScale * rhombScale * rhombScale;
  float rho = (cubeVol - rhomVol - cylVol) / cubeVol;
  return rho;
}
float lerp(float a, float b, float t) { return a + t * (b - a); }
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
  for (unsigned i = 0; i < wToRho.size(); i++) {
    float wi = w0 + i * wStep;
    float rho = WToRho(wi, cellSize);
    w[i] = wi;
    wToRho[i] = rho;
  }

  int numSamples = 50;
  std::vector<float> rhoUniform(numSamples);
  std::vector<float> wMapped(numSamples);

  float rhoMin = wToRho.front();
  float rhoMax = wToRho.back();
  float rhoStep = (rhoMax - rhoMin) / (numSamples - 1);

  // --- 3. The Inverse Mapping (Interpolation) ---
  for (int j = 0; j < numSamples; j++) {
    float targetRho = rhoMin + j * rhoStep;
    rhoUniform[j] = targetRho;

    // Find where targetRho sits in the original wToRho array
    auto it = std::lower_bound(wToRho.begin(), wToRho.end(), targetRho);

    if (it == wToRho.begin()) {
      wMapped[j] = w.front();
    } else if (it == wToRho.end()) {
      wMapped[j] = w.back();
    } else {
      // Perform linear interpolation between the two surrounding points
      size_t idx1 = std::distance(wToRho.begin(), it) - 1;
      size_t idx2 = idx1 + 1;

      float r1 = wToRho[idx1];
      float r2 = wToRho[idx2];

      // Calculate how far targetRho is between r1 and r2 (0.0 to 1.0)
      float t = (targetRho - r1) / (r2 - r1);

      // Map that same "t" to the w values
      wMapped[j] = lerp(w[idx1], w[idx2], t);
    }
  }


  std::string rhombicFile = "F:/meshes/lune_rhombic/rhombus.obj";
  rhombic.LoadObj(rhombicFile);
  TrigMesh cyl;
  std::string cylFile = "F:/meshes/lune_rhombic/cyl.obj";
  cyl.LoadObj(cylFile);
  
  size_t numCells = 21;
  size_t gridSize = numCells * 2 + 1;
  float epsI = 2.35;
  for (unsigned z = 0; z < gridSize; z++) {
    for (unsigned y = 0; y < gridSize; y++) {
      for (unsigned x = 0; x < gridSize; x++) {
        if ((x + y + z) % 2 == 1){
          continue;
        }
        //only place at face center and vertices.
        float fx = (float(x) - numCells)/ float(numCells);
        float fy = (float(y) - numCells)/ float(numCells);
        float fz = (float(z) - numCells)/ float(numCells);
        float r = std::sqrt(fx * fx + fy * fy + fz * fz);
        float rho = LuneRho(1, r, epsI);
      }
    }
  }
}