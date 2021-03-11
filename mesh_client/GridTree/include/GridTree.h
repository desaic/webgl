#pragma once

#include "Vec3.h"
#include "GridNode.h"
#include "GridNodeInternal.h"
#include "GridNodeLeaf.h"
#include "BitArray.h"

#include <iostream>

#define MAX_LOG2_BRANCHING_FACTOR 10

#define DEFAULT_LOG2_BF 3

class TrigMesh;

class GridTreeAbs {
public:
  virtual unsigned GetNumLevels()const = 0;

  virtual GridNode* GetRoot() = 0;
  ///get log 2 of branching factor in one axis
  virtual unsigned GetLog2BF(unsigned level) = 0;
  virtual void SetLog2BF(unsigned level, unsigned bf) = 0;
  ///get size of a block at level
  ///root has a block size of 1<<(log2Bf * numLevels) for a
  ///tree with constant log2 branching factor at all levels.
  virtual unsigned GetBlockSize(unsigned level) const = 0;
  virtual GridNode* MakeNode(unsigned level) = 0;
  virtual ~GridTreeAbs() {}
  virtual Vec3u GetSize()const { return size; }

protected:
  //sizes in x y z axes.
  Vec3u size;
};

///if ValueT is pointer type, they are not freed
///upon destructor or reallocation.
///
template<typename ValueT>
class GridTree : public GridTreeAbs{
public:
  GridTree() :log2BF(DEFAULT_LOG2_BF), numLevels(1) {
  }

  void Allocate(unsigned x, unsigned y, unsigned z);
  
  void Free() {
    root.Free();
  }

  virtual ~GridTree() {
    Free();
  }

  void SetLog2BF(unsigned level, unsigned bf) override {
    if (bf <= MAX_LOG2_BRANCHING_FACTOR) {
      log2BF = bf;
    }
  }

  ///root is level 0. numLevels = 2 means having a root
  ///level + 1 leaf level.
  unsigned GetNumLevels()const override{ return numLevels; }

  unsigned GetBlockSize(unsigned level)const override { return blockSize[level]; }

  GridNode * GetRoot() override{ return (GridNode*)(&root); }
  
  unsigned GetLog2BF(unsigned level) override { return log2BF; }

  GridNode* MakeINode() {
    GridNodeInternal<ValueT> * inode = new GridNodeInternal<ValueT>();
    inode->SetLog2BF(log2BF);
    inode->Allocate();
    return (GridNode*)inode;
  }

  GridNode* MakeLeaf() {
    GridNodeLeaf<ValueT>* leaf = new GridNodeLeaf<ValueT>();
    leaf->SetLog2BF(log2BF);
    leaf->Allocate();
    return (GridNode*)leaf;
  }

  GridNode * MakeNode(unsigned level) override {
    if (level >= numLevels - 1) {
      return MakeLeaf();
    }
    else {
      return MakeINode();
    }
  }
  
private:

  ///log 2 of branching factor along one axis. 
  ///log2BF=1 gives an octree
  ///default value log2BF=3 assigns each internal node an 8x8x8 set of children.
  unsigned log2BF;
  GridNodeInternal<ValueT> root;

  unsigned numLevels;

  ///block size at each level.
  ///calculated during allocation.
  ///blockSize at voxel level = 1.
  std::vector<unsigned> blockSize;
};

class TreePointer {
public:
  ///child indices from root
  ///the size of indices must be greater than or equal to the size of nodes.
  std::vector<Vec3u> indices;
  std::vector<GridNode *> nodes;
  GridTreeAbs* tree;

  TreePointer(GridTreeAbs* t) {
    Init(t);
  }

  void Init(GridTreeAbs* t);
  ///point pointer to node at level containing voxel (x,y,z).
  ///root is level 0
  ///\return false if the value or node does not exist 
  bool PointTo(unsigned level, unsigned x, unsigned y, unsigned z);

  /// PointTo(numLevels - 1)
  bool PointToLeaf(unsigned x, unsigned y, unsigned z);

  /// creates path if non existent
  void CreateLeaf(unsigned x, unsigned y, unsigned z);

  bool HasValue()const;
  ///move to a voxel within the same node.
  ///assuming pointer is pointing at a valid path.
  void MoveToSameNode(unsigned x, unsigned y, unsigned z);
  ///move x y or z axis by +1.
  ///the new voxel may be empty.
  ///\return false if the move will be out of bound.
  bool Increment(unsigned axis);
  bool Decrement(unsigned axis);
  ///Creates nodes from root to pointer if the node doesn't exist
  void CreatePath();

};

template<typename ValueT>
void GridTree<ValueT>::Allocate(unsigned x, unsigned y, unsigned z) {
  Free();
  size = Vec3u(x, y, z);
  unsigned maxSize = std::max(x, y);
  maxSize = std::max(maxSize, z);
  unsigned log2Size = std::log2(maxSize);
  if ((1U << log2Size) < maxSize) {
    log2Size++;
  }
  numLevels = log2Size / log2BF + ( (log2Size % log2BF) > 0) ;

  blockSize.resize(numLevels+1);
  unsigned s = 1U << log2BF;
  for (size_t i = 0; i < numLevels; i++) {
    blockSize[numLevels - i - 1] = s;
    s = s << log2BF;
  }
  blockSize[blockSize.size() - 1] = 1;
  root.SetLog2BF(log2BF);
  root.SetOrigin(0, 0, 0);
  root.Allocate();
}

///if pointing to a leaf, sets grid to val. 
///Otherwise sets entire subtree to val.
template <typename ValueT>
void SetVoxelValue(TreePointer & ptr, const ValueT & val) {
  Vec3u idx = ptr.indices.back();
  GridNode* node = ptr.nodes.back();
  node->SetValue(idx[0], idx[1], idx[2], (void*)(&val));
}

template <typename ValueT>
void AddVoxelValue(TreePointer& ptr, const ValueT& val) {
  Vec3u idx = ptr.indices.back();
  GridNode* node = ptr.nodes.back();
  node->AddValue(idx[0], idx[1], idx[2], (void*)(&val));
}

template <typename ValueT>
void GetVoxelValue(const TreePointer& ptr, const ValueT& val) {
  Vec3u idx = ptr.indices.back();
  GridNode* node = ptr.nodes.back();
  node->GetValue(idx[0], idx[1], idx[2], (void*)(&val));
}

///draws the tree structure without values.
void TreeToTrigMesh(GridTreeAbs*tree, TrigMesh * mesh, TrigMesh * cube);

void WriteInternalNode(GridNodeInternal<unsigned char>* node, std::ostream& out);

///no one needs more than 8 bits of info.
void WriteToTxt(GridTree<unsigned char>* tree, std::ostream& out);

void LoadFromTxt(GridTree<unsigned char>* tree, std::istream& in);
