#pragma once

#include "TrigMesh.h"
#include "Vec3.h"

#include <string>
#include <vector>

/// A connected group of coarse shell triangles with similar normals. Patches
/// overlap: a triangle may belong to several of them.
struct ShellPatch {
  std::vector<unsigned> trigs;
  Vec3f center; // seed triangle centroid
  Vec3f normal; // seed triangle normal, pointing away from the container
  float area = 0.0f;
};

/// Region-grows patches over the shell. Each patch takes triangles connected to
/// its seed whose centroid is within radius of the seed centroid and whose
/// normal is within maxAngleDeg of the seed normal. Triangles within
/// radius / 2 of a seed count as covered, so the next seed is always outside
/// the previous cores and the patches overlap by about half a radius.
std::vector<ShellPatch> BuildShellPatches(const TrigMesh &shell, float radius, float maxAngleDeg);

/// One OBJ group per patch (vertices duplicated per patch).
void SavePatchesObj(const std::string &path, const TrigMesh &shell,
                    const std::vector<ShellPatch> &patches);
