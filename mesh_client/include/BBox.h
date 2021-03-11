#pragma once

#include "Vec3.h"
#include <vector>

struct BBox{
	Vec3f mn, mx;
	BBox():mn(0),mx(0){}
};

struct BBoxInt {
	Vec3i mn, mx;
	BBoxInt() :mn(0), mx(0) {}
};

void UpdateBBox(const std::vector<float> & verts, BBox & box);

void ComputeBBox(const std::vector<float>& verts, BBox& box);
