#pragma once

#include "PackingConfig.h"

/// V-HACD 4 convex decomposition of every mesh in cfg.MeshDir() (mirrored "_m" meshes skipped).
/// per mesh, in cfg.OutputFolder():
///   <name>.obj        preview, one object (o hull_k) per convex hull
///   <name>.hulls.txt  plain text: "hulls N", then per hull "hull V F", V lines "x y z" and
///                     F lines "a b c" (triangle indices into that hull, 0-based, outward winding)
/// the meshes keep the coordinate frame of the source.
bool RunVhacd(const PackingConfig &cfg);
