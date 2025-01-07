#include "TrigMesh.h"
#include "TriangulateContour.h"

void TestCDT() {
  TrigMesh mesh;
  std::vector<float> v(8);
  mesh = TriangulateLoop(v.data(), v.size() / 2);
  mesh.SaveObj("F:/dump/loop_trigs.obj");
}
