#include "DebugTools.h"
#include "PackingConfig.h"
#include "PackingDriver.h"

#include <iostream>

// Standalone harness for the finger container: loads configs/pack_finger.cfg
// (currently pointed at the pack saved right before step 3) and saves the
// shrinkwrap mesh at step 3's own shrinkwrapRadius/shrinkwrapVoxelSize
// (PackingPlan.cpp) -- the same surface step 3's useShrinkwrapSurfacePoints
// targeting samples, so this is exactly "what step 3 will see" before it
// runs, for a direct before/after comparison against the post-step-3 pack.
//
//   ./finger_debug

int main() {
  std::cout.setf(std::ios::unitbuf);

  PackingConfig cfg;
  cfg.LoadFromFile("configs/pack_finger.cfg");
  std::cout << cfg.toString();

  DebugShrinkWrap(cfg, 3.0f, 1.0f);
  DebugShrinkwrapAttractionPoints(cfg, 3.0f, 1.0f);
  return 0;
}
