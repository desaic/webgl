#include "RigidTransform.h"
#include <sstream>
#include <iomanip>

std::string RigidTransform::toString() const {
  std::ostringstream oss;
  oss << std::setprecision(6);
  oss << "pos " << position[0] << " " << position[1] << " " << position[2] << " rot " << rotation(0, 0) << " "
      << rotation(0, 1) << " " << rotation(0, 2) << " " << rotation(1, 0) << " " << rotation(1, 1) << " "
      << rotation(1, 2) << " " << rotation(2, 0) << " " << rotation(2, 1) << " " << rotation(2, 2) << " scale "
      << scale;
  return oss.str();
}

void RigidTransform::FromRigidMatrix(const Matrix4f & rigidMat)
{
  rotation = rigidMat.getSubmatrix3x3(0, 0);
  position[0] = rigidMat(0, 3);
  position[1] = rigidMat(1, 3);
  position[2] = rigidMat(2, 3);
}

Matrix4f RigidTransform::ToMatrixRigid() const
{
  Matrix4f M = Matrix4f::identity();
  M.setSubmatrix3x3(0, 0, rotation);
  M(0, 3) = position[0];
  M(1, 3) = position[1];
  M(2, 3) = position[2];
  return M;
}

void RigidTransform::RightMultRigid(const RigidTransform & b){
  Matrix4f A = ToMatrixRigid();
  Matrix4f B = b.ToMatrixRigid();
  Matrix4f C = A * B;
  FromRigidMatrix(C);
}