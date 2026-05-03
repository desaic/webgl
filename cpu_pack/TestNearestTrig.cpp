
#include <vector>
#include "TrigGrid.h"
#include <random>
#include <iostream>
#include <fstream>
#include "Stopwatch.h"
#include "TrigMesh.h"

std::vector<Vec3f> SamplePoints(const Box3f & box){
  // Sample 1 million random points within the bounding box
  const size_t numSamples = 1000000;
  std::vector<Vec3f> samples;
  samples.reserve(numSamples);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> distX(box.vmin[0], box.vmax[0]);
  std::uniform_real_distribution<float> distY(box.vmin[1], box.vmax[1]);
  std::uniform_real_distribution<float> distZ(box.vmin[2], box.vmax[2]);

  for (size_t i = 0; i < numSamples; i++) {
    samples.push_back(Vec3f(distX(gen), distY(gen), distZ(gen)));
  }
  return samples;
}

void TestVox(){
  TrigMesh mesh;
  std::string dataDir = "/media/desaic/ssd2/data/";
  mesh.LoadObj(dataDir + "/cat.obj");
  Utils::Stopwatch timer;
  
  Box3f box = ComputeBBox(mesh.v);
  float dx = 0.02;
  std::cout<<box.vmin[0]<<" "<<box.vmin[1]<<" "<<box.vmin[2]<<"\n";
  Vec3f boxSize = box.vmax - box.vmin;
  std::cout<<boxSize[0]<<" "<<boxSize[1]<<" "<<boxSize[2]<<"\n";

  TrigGrid trigGrid;
  trigGrid.Build(mesh, dx);

  std::vector<Vec3f> samples = SamplePoints(box);

  // Compute distances for all sample points
  std::vector<float> distances(samples.size());
  const float MAX_DIST = 1e6f;

  timer.Start();
  for (size_t si = 0; si < samples.size(); si++) {
    Vec3f n;
    distances[si] = trigGrid.NearestTriangle(samples[si], MAX_DIST, n);
  }

  float elapsedMs = timer.ElapsedMS();
  std::cout << "distance query time ms " << elapsedMs << "\n";
  std::cout << "average time per query us " << (elapsedMs * 1000.0f / samples.size()) << "\n";

  // Save close points to OBJ file
  std::ofstream outFile(dataDir + "/out/close_points.obj");
  if (outFile.is_open()) {
    size_t closeCount = 0;
    for (size_t si = 0; si < samples.size(); si++) {
      if (distances[si] <= dx) {
        const Vec3f& p = samples[si];
        outFile << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
        closeCount++;
      }
    }
    outFile.close();
    std::cout << "Saved " << closeCount << " close points (within " << dx << ") to close_points.obj\n";
  } else {
    std::cout << "Failed to open output file\n";
  }
}
