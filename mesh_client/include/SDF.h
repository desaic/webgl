#pragma once

#include "GridTree.h"
#include "BBox.h"

#include <thread>
#include <mutex>

class TrigMesh;

//signed distance function computed from a trig mesh.
struct SDFMesh
{
  SDFMesh() :band(3) {}
  BBox box;
  TrigMesh* mesh;

  //origin at 0
  GridTree<float> sdf;

  std::mutex idxMtx;

  //index into trig list
  GridTree<size_t> idxGrid;
  
  //list of triangles within each non-empty voxel.
  std::vector < std::vector<size_t> >trigList;

  //voxel size h
  //origin at box.min - 0.5h - band.
  Vec3f gridOrigin;
  
  ///h. only supports uniform voxel size.
  float voxelSize;
  
  ///multiples of h
  float band;
};
