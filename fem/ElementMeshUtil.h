#ifndef ELEMENTMESH_UTIL_HPP
#define ELEMENTMESH_UTIL_HPP

#include <memory>
#include <string>
#include <vector>

#include "ElementMesh.h"
#include "TrigMesh.h"
#include "Vec3.h"
#include "Array3D.h"

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

void PullRight(const Vec3f& force, float dist, ElementMesh& em);
void PullLeft(const Vec3f& force, float dist, ElementMesh& em);
void AddGravity(const Vec3f& G, ElementMesh& em);

void FixLeft(float dist, ElementMesh& em);

void FixRight(float dist, ElementMesh& em);
// for x > xmax-range, x-=distance
void MoveRightEnd(float range, float distance, ElementMesh& em);
void MoveLeftEnd(float range, float distance, ElementMesh& em);
void FixMiddle(float ratio, ElementMesh& em);

void PullMiddle(const Vec3f& force, float ratio, ElementMesh& em);
void FixFloorLeft(float ratio, ElementMesh& em);

void SetFloorForRightSide(float y1, float ratio, ElementMesh& em);

void EmbedMesh();
int LoadX(ElementMesh& em, const std::string& inFile);

void SaveHexTxt(const std::string& file, const std::vector<Vec3f>& X,
                const std::vector<std::array<unsigned, 8> > elts);

void VoxGridToHexMesh(const Array3D8u& grid, const Vec3f& dx,
                      const Vec3f& origin, std::vector<Vec3f>& X,
                      std::vector<std::array<unsigned, 8> >& elts);

ElementMesh MakeHexMeshBlock(Vec3u size, const Vec3f& dx);

#endif