#pragma once

#include "GridTree.h"
#include "BBox.h"

class TrigMesh;

//signed distance function computed from a trig mesh.
struct SDFMesh
{
  SDFMesh() : voxelSize(0.25f), 
exactBand(1), band(3), initialized(false) 
  {}

  /// get distance from nearest neighbor vertex.
  /// values are defined on cube corners.
  /// coordinate in origininal mesh model space.
  float DistNN(Vec3f coord);

  /// get distance using triliear interpolation. queries 8 vertices.
  /// coordinate in origininal mesh model space.
  float Dist(Vec3f coord);

  //  configs

  ///multiples of h
  float band;
  float exactBand;

  ///h. only supports uniform voxel size.
  float voxelSize;

  //  computed quantities

  BBox box;

  ///origin at box.min - 0.5h - band.
  Vec3f gridOrigin;

  bool initialized;

  TrigMesh* mesh;

  GridTree<float> sdf;

  //  temporary variables that can be cleared out
  //  if memory is an issue

  ///index into trig list
  GridTree<size_t> idxGrid;

  ///list of triangles within each non-empty voxel.
  std::vector < std::vector<size_t> > trigList;

};

///computes unsigned distance field using fast marching algo.
void FastMarch(SDFMesh& sdf);

void MarchingCubes(SDFMesh & sdf, float level, TrigMesh* surf);
