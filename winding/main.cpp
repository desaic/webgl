#include <iostream>
#include "UT_SolidAngle.h"
#include "TrigMesh.h"
#include "Array3D.h"
#include "BBox.h"
#include "MeshUtil.h"

using UTVec3 = HDK_Sample::UT_Vector3T<float> ;

int TestSimpCube() {
  std::cout << "lol\n";
  TrigMesh mesh;
  std::string meshFile = "F:/meshes/winding/broken_cube.stl";
  mesh.LoadStl(meshFile);

  if (mesh.GetNumTrigs() == 0) {
    std::cout << "failed to load mesh \n";
    return -1;
  }
  HDK_Sample::UT_SolidAngle<float, float> sa;
  sa.init(mesh.GetNumTrigs(), (const int*)mesh.t.data(), mesh.GetNumVerts(),
          (const HDK_Sample::UT_Vector3T<float>*)(mesh.v.data()));
  UTVec3 pt;
  pt[0] = 6.25f;
  pt[1] = -6.25f;
  pt[2] = -26;
  float ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = -25;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = -24;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = 0;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = 24;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = 25;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = 26;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  pt[2] = 30;
  ang = sa.computeSolidAngle(pt);
  std::cout << ang << "\n";
  return 0;
}


int TestGripper() {
  TrigMesh mesh;
  std::string meshFile = "F:/meshes/winding/gripper.stl";
  mesh.LoadStl(meshFile);

  if (mesh.GetNumTrigs() == 0) {
    std::cout << "failed to load mesh \n";
    return -1;
  }
  HDK_Sample::UT_SolidAngle<float, float> sa;
  sa.init(mesh.GetNumTrigs(), (const int*)mesh.t.data(), mesh.GetNumVerts(),
          (const HDK_Sample::UT_Vector3T<float>*)(mesh.v.data()));
  Box3f box = ComputeBBox(mesh.v);
  float h = 0.4;
  Vec3f boxSize = box.vmax - box.vmin;
  Vec3u gridSize(boxSize[0]/h + 1, boxSize[1]/h + 1, boxSize[2]/h + 1);
  Array3D8u grid(gridSize, 0);
  std::cout << gridSize[0] << " " << gridSize[1] << " " << gridSize[2] << "\n";
  for (unsigned z = 0; z < gridSize[2]; z++) {
    if (z % 10 == 0) {
      std::cout << z << "\n";
    }
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        Vec3f coord(x, y, z);
        Vec3f pt = box.vmin + h * (Vec3f(0.5f) + coord);
        UTVec3 query;
        query[0] = pt[0];
        query[1] = pt[1];
        query[2] = pt[2];
        float ang = sa.computeSolidAngle(query);
        grid(x, y, z) = ang >= 6.28;
      }
    }
  }
  float voxRes[3] = {h, h, h};
  SaveVolAsObjMesh("F:/meshes/winding/gripper_vox.obj", grid, voxRes, 1);
  return 0;
}

int main() {
  TestSimpCube();
  //TestGripper(); 
  return 0;
}