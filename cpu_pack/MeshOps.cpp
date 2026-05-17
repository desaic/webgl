#include "MeshOps.h"
#include "Matrix3f.h"

TrigMesh MakeTransformedMesh(const TrigMesh & mesh, const Transformation & tran){
  TrigMesh transformedMesh = mesh;
  transformedMesh.scale(tran.scale);
  const auto & rot = tran.rotation;
  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TransformVerts(mesh.v, transformedMesh.v, rotMat);
  const auto & pos = tran.position;
  transformedMesh.translate(pos[0], pos[1], pos[2]);
  // Append to merged mesh for this item type
  return transformedMesh;
}

TrigMesh MakeMergedMesh(const TrigMesh & item, const std::vector<Transformation> & trans){
  TrigMesh merged;
  for(const auto tran: trans){
    TrigMesh inst = MakeTransformedMesh(item, tran);
    merged.append(inst);
  }
  return merged;
}

Matrix3f RotationMatrixRad(float rx, float ry, float rz) {
  Matrix3f mat;
  mat = Matrix3f::rotateX(rx);
  mat = mat * Matrix3f::rotateY(ry);
  mat = mat * Matrix3f::rotateZ(rz);
  return mat;
}

void TransformVerts(const std::vector<float> &verts, std::vector<float> &dst, const Matrix3f mat) {
  size_t nV = verts.size() / 3;
  dst.resize(verts.size());
  for (size_t i = 0; i < nV; i++) {
    Vec3f v0(verts[3 * i], verts[3 * i + 1], verts[3 * i + 2]);
    Vec3f v1 = mat * v0;
    dst[3 * i] = v1[0];
    dst[3 * i + 1] = v1[1];
    dst[3 * i + 2] = v1[2];
  }
}

void VoxelizeMesh(const TrigMesh &m, Array3D8u &grid, VoxConf conf)
{
  grid.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &m, grid);
}