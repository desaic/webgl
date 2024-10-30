#include "TrigMesh.h"
#include "Simplify.h"
void TestMeshSimp() {
  Simplify simp;
  TrigMesh m;
  m.LoadObj("F:/meshes/tank/trackUnitJoin_0.1_0.01deflate.obj");
  simp.triangles.resize(m.GetNumTrigs());
  simp.vertices.resize(m.GetNumVerts());
  for (size_t i = 0; i < m.GetNumTrigs(); i++) {
    for (unsigned j = 0; j < 3; j++) {
      simp.triangles[i].v[j] = m.GetIndex(i, j);
    }
  }
  for (size_t i = 0; i < m.GetNumVerts(); i++) {
    simp.vertices[i].p.x = m.GetVertex(i)[0];
    simp.vertices[i].p.y = m.GetVertex(i)[1];
    simp.vertices[i].p.z = m.GetVertex(i)[2];
  }
  int startSize = simp.triangles.size();
  simp.simplify_mesh(120000, 7, false);
  TrigMesh out;
  out.t.resize(3 * simp.triangles.size());
  out.v.resize(3 * simp.vertices.size());
  for (size_t i = 0; i < out.GetNumTrigs(); i++) {
    bool good = true;
    for (unsigned j = 0; j < 3; j++) {
      if (simp.triangles[i].v[j] >= out.GetNumVerts()) {
        good = false;
        break;
      }
    }
    if (!good) continue;

    for (unsigned j = 0; j < 3; j++) {
      out.t[3 * i + j] = simp.triangles[i].v[j];
    }
  }
  for (size_t i = 0; i < out.GetNumVerts(); i++) {
    out.v[3 * i] = simp.vertices[i].p.x;
    out.v[3 * i + 1] = simp.vertices[i].p.y;
    out.v[3 * i + 2] = simp.vertices[i].p.z;
  }
  out.SaveObj("F:/dump/simplify_test.obj");
}
