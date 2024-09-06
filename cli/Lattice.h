#pragma once
#include <string>

#include "Array3D.h"

namespace slicer {

enum class LatticeType { DIAMOND = 0, CUBE, GYROID, FLUORITE, OCTET };
std::string toString(LatticeType type);
LatticeType ParseLatticeType(const std::string& s);
Array3Df MakeCubeCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize);
Array3Df MakeDiamondCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize);
Array3Df MakeGyroidCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize);
Array3Df MakeFluoriteCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize);
Array3Df MakeOctetCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize);
}  // namespace slicer
