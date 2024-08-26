#include "FETrigMesh.hpp"

#include "ElementMeshUtil.h"

void FETrigMesh::UpdateMesh(ElementMesh* em, float drawingScale) {
  if (!em) {
    return;
  }
  _drawingScale = drawingScale;
  _em = em;
  if (!_mesh) {
    _mesh = std::make_shared<TrigMesh>();
  }
  if (_showWireFrame) {
    ComputeWireframeMesh(*_em, *_mesh, _edges, _drawingScale, _beamThickness);
  } else {
    ComputeSurfaceMesh(*_em, *_mesh, _meshToEMVert, _drawingScale);
  }
  _updated = true;
}

void FETrigMesh::UpdatePosition() {
  if (!(_em && _mesh)) {
    return;
  }
  _updated = true;
  if (_showWireFrame) {
    UpdateWireframePosition(*_em, *_mesh, _edges, _drawingScale,
                            _beamThickness);
    return;
  }
  for (size_t i = 0; i < _meshToEMVert.size(); i++) {
    unsigned emVert = _meshToEMVert[i];
    _mesh->v[3 * i] = _drawingScale * _em->x[emVert][0];
    _mesh->v[3 * i + 1] = _drawingScale * _em->x[emVert][1];
    _mesh->v[3 * i + 2] = _drawingScale * _em->x[emVert][2];
  }
}

void FETrigMesh::UpdateWireFrame(bool b) {
  if (b != _showWireFrame) {
    _showWireFrame = b;
    UpdateMesh(_em, _drawingScale);
    _updated = true;
  }
}
