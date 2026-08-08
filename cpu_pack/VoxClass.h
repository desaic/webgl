#pragma once
#include <stdint.h>

// one byte per voxel in PackingScene::classGrid. bg.vox stays the only
// source of truth for free space; this is metadata about what occupies a
// voxel, read by SurfaceCoverage and never by collision code.
enum VoxClass : uint8_t {
  VOX_FREE = 0,
  VOX_CONTAINER = 1,
  VOX_ITEM_BIG = 2,
  VOX_ITEM_SMALL = 3,
  VOX_INNER = 4,
};

// the single definition of "small" shared with PlanPackingSteps' SIZE_THRESH
// (its SIZE_THRESH.back()).
inline constexpr float VOX_SMALL_THRESH_CM = 3.0f;

inline uint8_t VoxClassOfExtent(float maxExtent, float smallThresh) {
  return maxExtent <= smallThresh ? VOX_ITEM_SMALL : VOX_ITEM_BIG;
}
