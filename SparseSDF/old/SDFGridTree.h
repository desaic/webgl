#ifndef SDF_GRIDTREE_H
#define SDF_GRIDTREE_H

#include "SDFMesh.h"
#include "GridTree.h"
///implements sdf using GridTree data structure.
class SDFGridTree:public SDFImp{

	public:
  float DistNN(Vec3f coord) override;

  /// get distance using triliear interpolation. queries 8 vertices.
  /// coordinate in origininal mesh model space.
  float DistTrilinear(Vec3f coord) override;

  void MarchingCubes(float level, TrigMesh* surf) override;

  int Compute() override;
  GridTree<float>* GetTree() { return &sdf; }

  void Allocate(Vec3u gridSize);

  GridTree<float> sdf;

  //  temporary variables that can be cleared out
  //  if memory is an issue

  ///index into trig list
  GridTree<size_t> idxGrid;

  ///list of triangles within each non-empty voxel.
  std::vector < std::vector<size_t> > trigList;

};

#endif