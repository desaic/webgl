#pragma once
#include "CSparseMat.h"
#include "ElementMesh.h"

// things helpful to be stored across time steps
struct SimState {
  CSparseMat K;
};

class FESim {
 public:
  void StepGrad(ElementMesh& em);

  void StepCG(ElementMesh& em, SimState& state);

  void StepGS(ElementMesh& em, SimState& state);
  void InitCG(ElementMesh& em, SimState& state) ;
};
