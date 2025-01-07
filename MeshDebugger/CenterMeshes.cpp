#include "TrigMesh.h"
#include "BBox.h"
#include <iostream>

void CenterMeshes(const std::string& buildDir) {
  int numMeshes = 5;
  // for (int i = 0; i < 18; i++) {
  // std::string modelDir = buildDir + std::to_string(i) + "/models";
  std::vector<TrigMesh> m(numMeshes);
  for (int j = 0; j < numMeshes; j++) {
    std::string meshFile = buildDir + "/" + std::to_string(j) + ".stl";
    m[j].LoadStl(meshFile);
  }

  BBox box;
  ComputeBBox(m[0].v, box);
  for (int j = 1; j < numMeshes; j++) {
    UpdateBBox(m[j].v, box);
  }
  Vec3f center = 0.5f * (box.vmin + box.vmax);
  std::cout << center[0] << " " << center[1] << " " << center[2] << "\n";
  for (int mi = 0; mi < numMeshes; mi++) {
    for (size_t j = 0; j < m[mi].v.size(); j += 3) {
      m[mi].v[j] -= center[0];
      m[mi].v[j + 1] -= center[1];
      m[mi].v[j + 2] -= center[2];
    }
    std::string meshFile = buildDir + "/" + std::to_string(mi) + ".stl";
    m[mi].SaveStl(meshFile);
  }
  //}
}
