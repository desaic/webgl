#pragma once

// rigid body nudge pieces shared by PackingScene::Nudge and the hull based nudge.

#include "Matrix3f.h"
#include "Quat4f.h"
#include "TrigGrid.h"
#include "Vec3.h"

#include <vector>

struct Contact {
    Vec3f worldPos;
    Vec3f normal;
    float distance;
    int meshIndex = -1;
};

struct RBSimParams {
  size_t steps = 100;
  float damping = 0.85f;
  float pgsPasses = 8;
  float minVel = 1e-4f;
  unsigned contactGatherInterval = 1;
  RBSimParams(){}
};

/// step budget, damping and solver passes by the fruit's smallest box extent.
void SetSimParams(RBSimParams &params, float minBoxSize);

Quat4f IntegrateAngularVelocity(Vec3f angVel, float dt);

/// keeps the deepest few contacts per direction cluster.
std::vector<Contact> reduceContactManifold(const std::vector<Contact> &rawContacts,
                                           float contactAngleThreshDeg);

/// sequential impulses on the contacts. updates linearVel and angularVel (world frame).
void SolveContactConstraintsPGS(const std::vector<Contact> &contacts,
                                const Vec3f &currentT,
                                const Matrix3f &currentRotMat,
                                const Matrix3f &Rinv,
                                float invMass,
                                const Vec3f &invI_local,
                                float eps,
                                int pgsPasses,
                                float dt,
                                Vec3f &linearVel,
                                Vec3f &angularVel);
