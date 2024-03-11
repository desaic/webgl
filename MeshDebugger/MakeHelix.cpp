#define _USE_MATH_DEFINES
#include <cmath>

#include "cgal_wrapper.h"

#include "FramedCurve.h"
#include "MakeHelix.h"
#include "MeshUtil.h"
#include "TriangulateContour.h"
#include <algorithm>
#include <fstream>

/// <summary>
/// 
/// </summary>
/// <param name="y0"></param>
/// <param name="y1"></param>
/// <param name="r"></param>
/// <param name="divs"></param>
/// <param name="pitch"></param>
/// <returns></returns>
FramedCurve MakeHelixCurve(float y0, float y1, float r, unsigned divs, float pitch) {
  float y = y0;
  float dyPerStep = pitch / divs;
  FramedCurve curve;
  unsigned vCount = 0;
  bool started = false;
  float dydTheta = pitch / (2 * 3.14159265f);
  while (y < y1) {
    float phase = vCount % divs / float(divs);
    float theta = phase * (2 * M_PI);
    float st = std::sin(theta);
    float x = r * st;
    float ct = std::cos(theta);
    float z = r * ct;
    y = y0 + dyPerStep * vCount;
    FrameR3 frame;
    frame.p = Vec3f(x, y, z);
    frame.N = Vec3f(st, 0, ct);
    frame.T = Vec3f(z, dydTheta, -x);
    frame.T.normalize();
    curve.frames.push_back(frame);
    vCount++;
  }
  return curve;
}

SimplePolygon MakeTrapezoid(float l0, float l1, float h) {
  SimplePolygon poly;
  poly.p.resize(4);
  poly.p[0] = Vec3f(0, -l0 / 2, 0);
  poly.p[1] = Vec3f(h, -l1 / 2, 0);
  poly.p[2] = Vec3f(h, l1 / 2, 0);
  poly.p[3] = Vec3f(0, l0 / 2, 0);
  return poly;
}

//   y      v3
//  /|\    \  v2
//   |      | 
//   |     /  v1
//   |     v0
int MakeHelix(const HelixSpec& spec, TrigMesh& mesh) {
  unsigned MAX_DIVS_PER_REV = 180;
  //helix is cut off from below this value
  float ymin = 0;
  float y1 = spec.length;
  unsigned divs = std::min(spec.divs, MAX_DIVS_PER_REV);
  float r0 = spec.inner_diam / 2;
  float r1 = spec.outer_diam / 2;
  float slopeWidth = std::abs(spec.inner_width - spec.outer_width) / 2;
  FramedCurve helix;
  helix = MakeHelixCurve(0, y1, r0, divs, spec.pitch);
  SimplePolygon poly;
  float l0 = spec.inner_width;
  float l1 = spec.outer_width;
  float h = r1 - r0;
  poly = MakeTrapezoid(l0, l1, h);
  //SaveCurveObj("F:/dump/helix_curve.obj", helix, 0.5);

  mesh = Sweep(helix, poly);
  mesh.SaveObj("F:/dump/helix_pt.obj");
  MergeCloseVertices(mesh);
  return 0;
}
