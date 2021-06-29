#pragma once
#include "GridTree.h"
#include "SDF.h"

class TrigMesh;

/// initializes sdf with closest point transformation
int cpt(SDFMesh & mesh);
int ComputeOBB(const std::vector<float>& trig, OBBox& obb, float band);