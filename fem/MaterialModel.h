#ifndef MATERIAL_MODEL_H
#define MATERIAL_MODEL_H

#include "Quadrature.h"
#include "StrainEne.h"
#include <memory>

class ElementMesh;

class MaterialModel {
 public:
  MaterialModel();
  MaterialModel(std::shared_ptr<StrainEne> ene) : enePtr(ene) {}
  float GetEnergy(int eidx, ElementMesh* mesh) const;

  QUADRATURE_TYPE quadType = QUADRATURE_TYPE::GAUSS2;

  std::shared_ptr<StrainEne> enePtr;
};
#endif