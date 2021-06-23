#ifndef OBB_SLICER
#define OBB_SLICER

#include "BBox.h"

#include <vector>

template <typename T>
struct Interval {
  T lb, ub;
  Interval() :lb(T(0)), ub(T(0))
  {}

  Interval(T l, T u) :lb(l), ub(u)
  {}
};

template < typename T>
struct SparseSlice{
  T ymin;
  std::vector<Interval<T> > rows;
  SparseSlice<T>() : ymin(0) {}
};

template < typename T>
struct SparseVoxel 
{
  T zmin;
  std::vector<SparseSlice<T> > slices;
  SparseVoxel<T>() : zmin(0) {}
};

class OBBSlicer
{
public:
  OBBSlicer() {}
  void Compute(OBBox& obb, SparseVoxel<int> & voxels);

};


#endif