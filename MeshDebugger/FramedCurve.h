#pragma once

#include "TrigMesh.h"

#include <vector>
#include <string>

struct FrameR3 {
  Vec3f p;
  Vec3f T;
  Vec3f N;
  // Binormal B= TxN
};

// closed. v_0 connects to v_n-1
struct SimplePolygon {
  std::vector<Vec3f> p;
  size_t size() const { return p.size(); }
  const Vec3f& operator[](size_t i) const { return p[i]; }
  Vec3f& operator[](size_t i) { return p[i]; }
};

struct FramedCurve {
  std::vector<FrameR3> frames;
  bool closed = false;
  size_t size() const { return frames.size(); }
  const FrameR3& operator[](size_t i) const { return frames[i]; }
  FrameR3& operator[](size_t i) { return frames[i]; }
};

void SaveCurveObj(const std::string& file, const FramedCurve& curve,
                  float vecLen);

// x in normal axis, y in binormal, z in tangent
TrigMesh Sweep(const FramedCurve& fc, const SimplePolygon& poly);
//calls cdt
TrigMesh TriangulatePolygon(const SimplePolygon& poly);