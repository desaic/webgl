#ifndef MATERIAL_MODEL_H
#define MATERIAL_MODEL_H

#include "Array2D.h"
#include "Quadrature.h"
#include "StrainEne.h"
#include <memory>

class ElementMesh;
class Element;

// computes elastic material model using geometric info from Element.
class MaterialModel {
 public:
  MaterialModel();
  MaterialModel(std::shared_ptr<StrainEne> ene) : enePtr(ene) {}
  float GetEnergy(size_t eidx, const ElementMesh& mesh) const;
  std::vector<Vec3f> GetForce(int eidx, const ElementMesh& mesh)const;
  Array2Df GetStiffness(int eidx, const ElementMesh& mesh) const;
  Array2Df Stiffness(unsigned qi, const Element* ele, const ElementMesh& em)const;
  QUADRATURE_TYPE quadType = QUADRATURE_TYPE::GAUSS2;

  std::shared_ptr<StrainEne> enePtr;
};
#endif