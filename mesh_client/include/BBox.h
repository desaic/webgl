#pragma once

#include "Vec2.h"
#include "Vec3.h"
#include <vector>

struct BBox2D {
	Vec2f mn, mx;
	BBox2D():mn(0,0), mx(0,0){}
	void Compute(const std::vector<Vec2f> & verts);
};

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
