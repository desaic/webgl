#pragma once
#include "Matrix3f.h"
#include "RigidTransform.h"
#include "TrigMesh.h"
#include "Quat4f.h"
#include "eig3x3.h"

// helpers
Matrix3f eigVecFromVal(const Matrix3f &A_in, const double *eigenvalues);

// rigid body in reference configuration.
class RigidBody {
  public:
    Vec3f oldOrigin;
    // going from inertia frame back to input pose.
    // R0^T to get mesh from input frame to inertia frame.
    Matrix3f R0;

    Matrix3f inertia;

    float vol = 0.0f;

    void SetMesh(TrigMesh &input);

    void ToInertiaFrame(TrigMesh & mesh) const;
    // back to input pose of the mesh
    void ToInputFrame(TrigMesh & mesh) const;

    // get transformation relative to input pose instead of inertia pose.
    RigidTransform GetInputTran(RigidTransform & input) const ;
};

void TestInertiaFrame();