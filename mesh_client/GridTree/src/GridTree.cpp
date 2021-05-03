#include "GridTree.h"
#include "GridNodeLeaf.h"
#include "GridNodeInternal.h"
#include "TrigMesh.h"

#include <iostream>

void TreePointer::Init(GridTreeAbs* t)
{
  tree = t;
  nodes.resize(tree->GetNumLevels());
  indices.resize(tree->GetNumLevels(), Vec3uc(0,0,0));
  nodes[0] = tree->GetRoot();
  valid = true;
}

///point pointer to sequence of nodes to reach voxel (x,y,z).
bool TreePointer::PointTo(unsigned x, unsigned y, unsigned z)
{
  valid = true;
  //which block does (x,y,z) fall in.
  //the block index at the leaf node is the input voxel index.
  Vec3u blockIndex(x, y, z);
  unsigned log2Bf = tree->GetLog2BF();
  unsigned indexMask = (1 << log2Bf) - 1;
  for (size_t i = 0; i < indices.size(); i++) {
    unsigned level = indices.size() - i - 1;
    for (unsigned d = 0; d < 3; d++) {
      indices[level][d] = blockIndex[d] & indexMask;
      blockIndex[d]=blockIndex[d] >> 3;
    }
  }
  for (size_t i = 1; i < indices.size(); i++) {
    nodes[i] = nullptr;
  }
  for (size_t i = 1; i < indices.size(); i++) {
    GridNode* parent = nodes[i-1];
    GridNode * child = parent->GetChild(indices[i-1][0], indices[i-1][1], indices[i-1][2]);
    if (child == nullptr) {
      return false;
    }
    nodes[i] = child;
  }
  return true;
}

void TreePointer::CreateLeaf(unsigned x, unsigned y, unsigned z)
{
  bool exists = PointTo(x, y, z);
  if (!exists) {
    CreatePath();
  }
}

bool TreePointer::HasValue()const
{
  if (nodes[nodes.size() - 1] == nullptr) {
    return false;
  }
  Vec3uc idx = indices.back();
  return nodes[nodes.size() - 1]->HasValue(idx[0], idx[1], idx[2]);
}

void TreePointer::MoveWithinNode(unsigned x, unsigned y, unsigned z)
{
  unsigned level = indices.size() - 1;
  unsigned blockSize = tree->GetBlockSize(level);
  indices[level][0] = x % blockSize;
  indices[level][1] = y % blockSize;
  indices[level][2] = z % blockSize;
}

bool TreePointer::Increment(unsigned axis)
{
  unsigned log2BF = tree->GetLog2BF();
  unsigned gridSize = 1 << log2BF;
  unsigned level = unsigned(indices.size()) - 1;
  
  for (unsigned i = 0; i < indices.size(); i++) {
    level = unsigned(indices.size()) - 1 - i;
    unsigned childIdx = indices[level][axis] + 1;
    if (childIdx >= gridSize) {
      if (level == 0) {
        //root level. no more block to jump to .
        valid = false;
        return false;
      }
      //move on to the next block.
      indices[level][axis] = 0;
    }
    else {
      indices[level][axis] = childIdx;
      break;
    }
  }

  //update node pointers.
  for (unsigned l = level; l < indices.size() - 1; l++) {
    GridNode* parent = nodes[l];
    if (parent != nullptr) {
      nodes[l + 1] = parent->GetChild(indices[l][0], indices[l][1], indices[l][2]);
    } else {
      nodes[l + 1] = nullptr;
    }
  }
  return true;
}

bool TreePointer::Decrement(unsigned axis)
{
  unsigned level = unsigned(indices.size()) - 1;
  unsigned log2BF = tree->GetLog2BF();
  unsigned gridSize = 1 << log2BF;
  for (unsigned i = 0; i < indices.size(); i++) {
    level = unsigned(indices.size()) - 1 - i;
    int childIdx = int(indices[level][axis]) - 1;
    if (childIdx < 0) {
      if (level == 0) {
        //root level. no more block to jump to .
        valid = false;
        return false;
      }
      //move on to the next block.
      indices[level][axis] = gridSize - 1;
    }
    else {
      indices[level][axis] = unsigned(childIdx);
      break;
    }
  }

  //update node pointers.
  for (unsigned l = level; l < indices.size() - 1; l++) {
    GridNode* parent = nodes[l];
    if (parent != nullptr) {
      nodes[l + 1] = parent->GetChild(indices[l][0], indices[l][1], indices[l][2]);
    }
    else {
      nodes[l + 1] = nullptr;
    }
  }
  return true;
}

///Creates nodes from root to pointer if the node doesn't exist
void TreePointer::CreatePath()
{
  if (nodes.size() == 1) {
    return;
  }
  unsigned numLevels = tree->GetNumLevels();
  Vec3u origin(0, 0, 0);
  for (size_t i = 0; i < indices.size() - 1; i++) {
    for (unsigned d = 0; d < 3; d++) {
      origin[d] += indices[i][d] * tree->GetBlockSize(i + 1);
    }
    if (nodes[i + 1] != nullptr) {
      continue;
    }
    GridNode* node = tree->MakeNode(i+1);
    node->SetOrigin(origin[0], origin[1], origin[2]);
    nodes[i + 1] = node;
    nodes[i]->AddChild(node, indices[i][0], indices[i][1], indices[i][2]);
    //std::cout << indices[i][0] << " " << indices[i][1] << " " << indices[i][2] << "\n";
  }
}

void TreeToTrigMesh(GridTreeAbs* tree, TrigMesh* mesh, TrigMesh * cube)
{
  unsigned numLevels = tree->GetNumLevels();
  std::vector<GridNode* > parents(1, tree->GetRoot());
  unsigned totalSize = tree->GetBlockSize(0);
  Vec3f voxSize (1.0f / totalSize, 1.0f / totalSize, 1.0f / totalSize);
  
  for (unsigned level = 0; level < numLevels; level++) {
    std::vector<GridNode* > children;
    unsigned blockSize = tree->GetBlockSize(level);
    float cubeSize = float(blockSize) / totalSize;

    for (size_t i = 0; i < parents.size(); i++) {
      Vec3u origin;
      GridNode* parent = parents[i];
      parent->GetOrigin(origin[0], origin[1], origin[2]);
      if (level > 0) {
        TrigMesh nodeCube = *cube;
        nodeCube.scale(cubeSize);
        nodeCube.translate(voxSize[0] * origin[0], voxSize[2] * origin[2], voxSize[1] * origin[1]);
        mesh->append(nodeCube);
      }
      unsigned gridSize = parent->GetGridSize();
      for (size_t x = 0; x < gridSize; x++) {
        for (size_t y = 0; y < gridSize; y++) {
          for (size_t z = 0; z < gridSize; z++) {
            GridNode* child = parent->GetChild(x, y, z);
            if (child != nullptr) {
              children.push_back(child);
            }
          }
        }
      }
    }
    parents = children;
  }
}

bool isNbrDifferent(const TreePointer & ptr, uint8_t val)
{
  if (!ptr.HasValue()) {
    return true;
  }
  else {
    uint8_t nbrVal = 0;
    GetVoxelValue(ptr, nbrVal);
    if (val != nbrVal) {
      return true;
    }
  }
  return false;
}

void WriteInternalNode(GridNodeInternal<unsigned char>* node, std::ostream& out)
{
  Vec3u origin;
  node->GetOrigin(origin[0], origin[1], origin[2]);
  out << "origin " << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  unsigned numValues = node->GetNumValues();
  out << "numValues " << numValues << "\n";
  out << node->val.mask.ToString() << "\n";

}

void WriteLeafNode(GridNodeLeaf<unsigned char>* node, std::ostream& out)
{
  Vec3u origin;
  node->GetOrigin(origin[0], origin[1], origin[2]);
  out << "origin " << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  unsigned numValues = node->GetNumValues();
  out << "numValues " << numValues << "\n";
  out << node->val.ToString() << "\n";
}

void WriteToTxt(GridTree<unsigned char>* tree, std::ostream& out) {
  Vec3u size = tree->GetSize();
  out << size[0] << " " << size[1] << " " << size[2] << "\n";
  unsigned numLevels = tree->GetNumLevels();
  out << "numLevels " << numLevels << "\n";
  unsigned log2BF = tree->GetLog2BF();
  out << "log2BF " << log2BF << "\n";
  std::vector<GridNode* > parents;
  parents.push_back(tree->GetRoot());
  for (size_t l = 1; l < numLevels; l++) {
    out << "level " << (l-1) << "\n";
    out << "numNodes " << parents.size() << "\n";
    std::vector<GridNode*> children;
    for (size_t i = 0; i < parents.size(); i++) {
      GridNode* parent = parents[i];
      WriteInternalNode((GridNodeInternal<unsigned char> * )parent, out);
      GridNode** childList = parent->GetChildren();
      unsigned numChildren = parent->GetNumChildren();
      children.insert(children.end(), childList, childList + numChildren);
    }
    parents = children;
  }

  //save leaves
  out << "level " << (numLevels-1) << "\n";
  out << "numNodes " << parents.size() << "\n";
  size_t numValues = 0;
  for (size_t i = 0; i < parents.size(); i++) {
    GridNode* parent = parents[i];
    WriteLeafNode((GridNodeLeaf<unsigned char>*)parent, out);
    numValues += parent->GetNumValues();
  }
  std::cout << "Total number of values " << numValues << "\n";
}

void LoadFromTxt(GridTree<unsigned char>* tree, std::istream& in)
{
}


