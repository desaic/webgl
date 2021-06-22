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

///oriented bounding box
struct OBBox {
  Vec3f origin;
  Vec3f axes[3];
  OBBox() : origin(0) {
    axes[0][0] = 1;
    axes[1][1] = 1;
    axes[2][2] = 1;
  }
};

void UpdateBBox(const std::vector<float> & verts, BBox & box);

void ComputeBBox(const std::vector<float>& verts, BBox& box);
