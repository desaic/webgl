#pragma once

#include "GridTree.h"
#include "BBox.h"

#include <thread>
#include <mutex>

class TrigMesh;

//signed distance function computed from a trig mesh.
struct SDFMesh
{
  BBox box;
  TrigMesh* mesh;

  //origin at 0
  GridTree<float> sdf;

  std::mutex idxMtx;

  //index into trig list
  GridTree<size_t> idxGrid;
  //list of triangles within each non-empty voxel.
  std::vector < std::vector<size_t> >trigList;
  //origin at -half voxel size.
  Vec3f gridOrigin;

  Vec3f voxelSize;
};
