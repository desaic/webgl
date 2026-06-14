#pragma once
#include "Matrix3f.h"
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

    RigidBody(TrigMesh &input);

    void ToInertiaFrame(TrigMesh & mesh) const;
    // back to input pose of the mesh
    void ToInputFrame(TrigMesh & mesh) const;
};

struct RigidBodyInfo{
    Vec3f centerOfMass;
    // going from inertia frame back to input pose.
    Matrix3f R0;

    Matrix3f inertia;

    float vol = 0.0f;
    RigidBodyInfo(){}
    void Set(const RigidBody &rb) {
      centerOfMass = rb.oldOrigin;
      R0 = rb.R0;
      inertia = rb.inertia;
      vol = rb.vol;
    }
};


void TestInertiaFrame();