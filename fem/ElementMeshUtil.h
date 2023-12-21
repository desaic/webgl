#ifndef ELEMENTMESH_UTIL_HPP
#define ELEMENTMESH_UTIL_HPP

#include <memory>
#include <string>
#include <vector>

#include "ElementMesh.h"
#include "TrigMesh.h"
#include "Vec3.h"

void ComputeSurfaceMesh(const ElementMesh& em, TrigMesh& m,
                        std::vector<uint32_t>& meshToEMVert);

void ComputeWireframeMesh(const ElementMesh& em, TrigMesh& m,
                          std::vector<Edge>& edges);
void UpdateWireframePosition(const ElementMesh& em, TrigMesh& m,
                             std::vector<Edge>& edges);
#endif