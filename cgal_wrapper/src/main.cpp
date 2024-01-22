#include "cgal_wrapper.h"
#include <iostream>
#include "TrigMesh.h"

mesh_ptr TrigMeshToPtr(TrigMesh& mesh) {
  mesh_ptr ptr;
  ptr.v = mesh.v.data();
  ptr.t = mesh.t.data();
  ptr.nv = mesh.GetNumVerts();
  ptr.nt = mesh.GetNumTrigs();
  return ptr;
}

void PtrToTrigMesh(mesh_ptr ptr, TrigMesh& mesh) {
  mesh.v.resize(ptr.nv * 3);
  mesh.t.resize(ptr.nt * 3);
  for (size_t i = 0; i < mesh.v.size(); i++) {
    mesh.v[i] = ptr.v[i];
  }
  for (size_t i = 0; i < mesh.t.size(); i++) {
    mesh.t[i] = ptr.t[i];
  }
}

int main(int argc, char*argv[]) {
  TrigMesh mesh;
  std::string file1 = "F:/dolphin/meshes/screw/screw_big.obj";
  mesh.LoadObj(file1);
  TrigMesh cube = MakeCube(Vec3f(-10, -10, -10), Vec3f(10, 10, 10));
  mesh_ptr ma = TrigMeshToPtr(mesh);
  mesh_ptr mb = TrigMeshToPtr(cube);
  mesh_ptr mc;
  int ret = mesh_union(ma, mb, mc);
  TrigMesh outMesh;
  PtrToTrigMesh(mc, outMesh);
  outMesh.SaveObj("F:/dump/inter.obj");
  std::cout << "lol\n";
  mc.free();
  return 0;
}