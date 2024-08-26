#include "FESim.hpp"
#include "ArrayUtil.h"

#include <iostream>

void FESim::StepGrad(ElementMesh& em) {
  if (em.X.empty()) {
    return;
  }
  std::vector<Vec3f> force = em.GetForce();

  Add(force, em.fe);
  Vec3f maxAbsForce = MaxAbs(force);
  std::cout << "max abs(force) " << maxAbsForce[0] << " " << maxAbsForce[1]
            << " " << maxAbsForce[2] << "\n";
  float h = 0.0001f;
  const float maxh = 0.0001f;
  h = maxh / maxAbsForce.norm();
  h = std::min(maxh, h);
  std::cout << "h " << h << "\n";
  std::vector<Vec3f> dx = h * force;
  Fix(dx, em.fixedDOF);
  Add(em.x, dx);
}

void FESim::StepCG(ElementMesh& em, SimState& state) {
  if (em.X.empty()) {
    return;
  }
  // elastic and external force combined
  std::vector<Vec3f> force = em.GetForce();
  Add(force, em.fe);
  size_t numDOF = em.x.size() * 3;
  float eleSize = em.X[1][2] - em.X[0][2];
  std::vector<double> dx(numDOF, 0), b(numDOF);
  float boundaryTol = 1e-4f;
  std::vector<bool> fixed = em.fixedDOF;
  for (size_t i = 0; i < em.x.size(); i++) {
    for (unsigned j = 0; j < 3; j++) {
      unsigned l = 3 * i + j;
      // within or under lower bound and the total force is not pulling it
      // away
      if (em.x[i][j] < em.lb[i][j] + boundaryTol) {  //&& force[i][j]<0) {
        fixed[l] = true;
      }
    }
  }

  for (size_t i = 0; i < fixed.size(); i++) {
    if (fixed[i]) {
      b[i] = 0;
    } else {
      b[i] = force[i / 3][i % 3];
    }
  }
  float identityScale = 1000;
  em.ComputeStiffness(state.K);
  FixDOF(fixed, state.K, identityScale);
  CG(state.K, dx, b, 100);
  float h0 = 10.0f;
  const unsigned MAX_LINE_SEARCH = 10;
  double E0 = em.GetPotentialEnergy();

  double newE = E0;
  auto oldx = em.x;
  float h;
  bool updated = false;
  float maxdx = Linf(dx);
  float maxb = Linf(b);
  float bScale = 0;
  if (maxb > 0) {
    bScale = maxdx / maxb;
    for (size_t i = 0; i < b.size(); i++) {
      dx[i] += bScale * b[i];
    }
  }
  h = std::min(h0, 0.5f * eleSize / maxdx);
  // std::cout << "h " << h << "\n";
  for (unsigned li = 0; li < MAX_LINE_SEARCH; li++) {
    em.x = oldx;
    AddTimes(em.x, dx, h);
    newE = em.GetPotentialEnergy();
    // std::cout << newE << "\n";
    if (newE < E0) {
      updated = true;
      break;
    }
    h /= 2;
  }
  // std::cout << "h " << h << "\n";
  if (!updated) {
    em.x = oldx;
  }
  std::cout << newE << "\n";
  // std::vector<Vec3f> dx = h * force;
}

void FESim::StepGS(ElementMesh& em, SimState& state) {
  if (em.X.empty()) {
    return;
  }
  // elastic and external force combined
  std::vector<Vec3f> force = em.GetForce();
  Add(force, em.fe);
  size_t numDOF = em.x.size() * 3;
  float eleSize = em.X[1][2] - em.X[0][2];
  std::vector<double> dx(numDOF, 0), b(numDOF);
  float boundaryTol = 1e-4f;
  std::vector<bool> fixed = em.fixedDOF;
  for (size_t i = 0; i < em.x.size(); i++) {
    for (unsigned j = 0; j < 3; j++) {
      unsigned l = 3 * i + j;
      // within or under lower bound and the total force is not pulling it
      // away
      if (em.x[i][j] < em.lb[i][j] + boundaryTol) {  //&& force[i][j]<0) {
        fixed[l] = true;
      }
    }
  }
}

void FESim::InitCG(ElementMesh& em, SimState& state) {
  em.InitStiffnessPattern();
  em.CopyStiffnessPattern(state.K);
}