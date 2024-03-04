#ifndef ELEMENTMESH_HPP
#define ELEMENTMESH_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "MaterialModel.h"
#include "Element.h"
#include "Vec3.h"
#include "Array2D.h"
#include "CSparseMat.h"

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

  //for vertex i, map from neighbor index j to sparse index 
  std::vector<std::map<unsigned, unsigned> > sparseBlockIdx;

  double GetElasticEnergyEle(int eId) const;
  double GetElasticEnergy() const;
  //potential energy from external forces.
  double GetExternalEnergy() const;
  //elastic energy plus potential energy from external forces
  double GetPotentialEnergy() const;
  // nodal elastic forces for one element
  std::vector<Vec3f> GetForceEle(int eId) const;
  // get elastic forces. external forces are stored in fe.
  std::vector<Vec3f> GetForce() const;
  Array2Df GetStiffnessEle(unsigned eId) const;
  void CopyStiffnessEleToSparse(unsigned ei, const Array2Df& Ke, CSparseMat& K);
  void ComputeStiffness(CSparseMat & K);
  //for debugging matrix assembly
  void ComputeStiffnessDense(Array2Df& K);

  void InitStiffnessPattern();
  void CopyStiffnessPattern(CSparseMat& K);
  //load a mesh stored in plain text
  //removes existing mesh.
  void LoadTxt(const std::string & file);
  void SaveTxt(const std::string& file);  

  //has to be called before sim
  void InitElements();
};

#endif