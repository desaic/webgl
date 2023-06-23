#pragma once
#include "GridTree.h"
#include "SDFMesh.h"
#include "OBBSlicer.h"
#include "SDFGridTree.h"

class TrigMesh;

/// initializes sdf with closest point transformation
int cpt(SDFGridTree & mesh);
int ComputeOBB(const std::vector<float>& trig, OBBox& obb, float band);