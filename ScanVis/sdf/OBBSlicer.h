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

  /// expand to minimal interval containing this and i1
  void Expand(const Interval<T> & i1) {
    if (i1.IsEmpty()) {
      return;
    }
    if (i1.lb < lb) {
      lb = i1.lb;
    }
    if (i1.ub > ub) {
      ub = i1.ub;
    }
  }

  bool IsEmpty() const{
    return ub < lb;
  }
};

template < typename T>
struct SparseSlice{
  T ymin;
  std::vector<Interval<T> > rows;
  SparseSlice<T>() : ymin(0) {}
  ///expand to minimal convex set containing this and s1
  void Expand(const SparseSlice<T>& s1) {
    //if this set is empty , copy the other set.
    if (rows.size() == 0) {
      ymin = s1.ymin;
      rows = s1.rows;
      return;
    }
    
    int ymin0 = ymin;
    if (ymin > s1.ymin) {
      ymin = s1.ymin;
    }

    int ymax = int(ymin0) + int(rows.size());
    int ymax1 = int(s1.ymin) + +int(s1.rows.size());
    if (ymax1 > ymax) {
      ymax = ymax1;
    }

    int numRows = ymax - ymin;
    std::vector<Interval<T> > newRows( size_t(numRows), Interval<T>( T(0), T(-1) ) );
    size_t rowOffset = size_t(ymin0 - ymin);
    for (size_t row = 0; row < rows.size(); row++) {
      newRows[row + rowOffset] = rows[row];
    }

    rowOffset = size_t(s1.ymin - ymin);
    for (size_t row = 0; row < s1.rows.size(); row++) {
      Interval<T> i0 = newRows[row + rowOffset];
      if (i0.IsEmpty()) {
        newRows[row + rowOffset] = s1.rows[row];
      }
      else {
        newRows[row + rowOffset].Expand(s1.rows[row]);
      }
    }
    rows = newRows;
  }
  bool IsEmpty() const{
    return rows.size() == 0;
  }
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

int obb_ceil(float f);
#endif