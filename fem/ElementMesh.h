#ifndef ELEMENTMESH_HPP
#define ELEMENTMESH_HPP

#include <string>
#include <vector>
#include <memory>

#include "Element.h"
#include "Vec3.h"

class ElementMesh {
 public:
  //undeformed positions
  std::vector<Vec3f> X;
  //deformed positions. initialized to X.
  std::vector<Vec3f> x;
  std::vector<std::unique_ptr<Element> > e;
  //load a mesh stored in plain text
  //removes existing mesh.
  void LoadTxt(const std::string & file);

  void SaveTxt(const std::string& file);
};

#endif