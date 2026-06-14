#pragma once

#include "Vec3.h"
#include "Matrix3f.h"
#include "Matrix4f.h"
#include <vector>

// Structure to store transformation data
struct RigidTransform {
    Vec3f position;
    Matrix3f rotation;    
    float scale = 1.0f;
    RigidTransform():rotation(Matrix3f::identity()){}
    RigidTransform(const Vec3f & pos, const Matrix3f & rot):position(pos),
    rotation(rot){}
    /// @brief 
    /// @return rotation matrix is printed in row major order. even though internally stored in column major order.
    std::string toString() const;

    // this = this * b
    // scale is ignored.
    void RightMultRigid(const RigidTransform & b);
    void FromRigidMatrix(const Matrix4f & rigidMat);
    void Apply( std::vector<float> & v)const;
    // scale is ignored. rigid transformation only.
    Matrix4f ToMatrixRigid()const;
};
