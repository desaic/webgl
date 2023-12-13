#pragma once
#include "Vec3.h"
#include <vector>

struct BBox{
	///vertices at min and max coordinates
	Vec3f vmin, vmax;
	BBox():vmin(0.0f,0.0f,0.0f),
		vmax(0.0f,0.0f,0.0f) {}
  void Merge(const BBox& b);
  bool _init = false;
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

void UpdateBBox(const std::vector<float>& verts, BBox& box);

void ComputeBBox(const std::vector<float>& verts, BBox& box);
