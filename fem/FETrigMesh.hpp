#pragma once
#include "TrigMesh.h"
#include "ElementMesh.h"

// triangle mesh for rendering an fe mesh.
class FETrigMesh {
 public:
  FETrigMesh() {}
  void UpdateMesh(ElementMesh* em, float drawingScale);

  // copy "x" from _em to _mesh
  void UpdatePosition();

  // only updates mesh on change.
  void UpdateWireFrame(bool b);

  ElementMesh* _em = nullptr;
  std::shared_ptr<TrigMesh> _mesh;
  std::vector<uint32_t> _meshToEMVert;
  std::vector<Edge> _edges;
  bool _showWireFrame = true;
  bool _updated = false;
  float _drawingScale = 1;
  float _beamThickness = 0.5f;
};
