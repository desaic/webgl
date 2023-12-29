#ifndef ELEMENTMESH_HPP
#define ELEMENTMESH_HPP

#include <string>
#include <vector>
#include <memory>

#include "MaterialModel.h"
#include "Element.h"
#include "Vec3.h"
#include "Array2D.h"

class ElementMesh {
 public:
  //undeformed positions
  std::vector<Vec3f> X;
  //deformed positions. initialized to X.
  std::vector<Vec3f> x;
  std::vector<std::unique_ptr<Element> > e;
  //material model for each element
  std::vector<MaterialModel> m;
  //external force
  std::vector<Vec3f> fe;
  std::vector<bool> fixedDOF;

  double GetElasticEnergyEle(int eId) const;
  double GetElasticEnergy() const;
  // nodal elastic forces for one element
  std::vector<Vec3f> GetForceEle(int eId) const;
  std::vector<Vec3f> GetForce() const;
  Array2Df GetStiffnessEle(int eId) const;
 
  //load a mesh stored in plain text
  //removes existing mesh.
  void LoadTxt(const std::string & file);
  void SaveTxt(const std::string& file);  

  //has to be called before sim
  void InitElements();
};

#endif