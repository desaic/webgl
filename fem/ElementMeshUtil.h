#ifndef ELEMENTMESH_UTIL_HPP
#define ELEMENTMESH_UTIL_HPP

#include <memory>
#include <string>
#include <vector>

#include "ElementMesh.h"
#include "TrigMesh.h"
#include "Vec3.h"

/// <summary>
/// Unimplemented
/// </summary>
/// <param name="em"></param>
/// <param name="m"></param>
/// <param name="meshToEMVert"></param>
/// <param name="drawingScale"></param>
void ComputeSurfaceMesh(const ElementMesh& em, TrigMesh& m,
                        std::vector<uint32_t>& meshToEMVert,
                        float drawingScale);
/// <summary>
/// 
/// </summary>
/// <param name="em"></param>
/// <param name="m"></param>
/// <param name="edges">computed by this call but also used by UpdateWireframePosition</param>
/// <param name="drawingScale"></param>
void ComputeWireframeMesh(const ElementMesh& em, TrigMesh& m,
                          std::vector<Edge>& edges, float drawingScale,
                          float beamThickness=0.5f);
void UpdateWireframePosition(const ElementMesh& em, TrigMesh& m,
                             std::vector<Edge>& edges, float drawingScale,
                             float beamThickness);
#endif