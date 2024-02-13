#define _USE_MATH_DEFINES
#include <cmath>

#include "cgal_wrapper.h"

#include "MakeHelix.h"
#include "MeshUtil.h"
#include "TriangulateContour.h"
#include  <algorithm>
#include <fstream>

struct FrameR3 {
  Vec3f p;
  Vec3f T;
  Vec3f N;
  // Binormal B= TxN
};

//closed. v_0 connects to v_n-1
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

void SaveCurveObj(const std::string & file, const FramedCurve& curve, float vecLen) {
  std::ofstream out(file);
  for (size_t i = 0; i < curve.frames.size(); i++) {
    unsigned v0 = 3 * i;
    const FrameR3& f = curve.frames[i];
    out << "v " << f.p[0] << " " << f.p[1] << " " << f.p[2] << "\n";
    Vec3f N = f.p + vecLen * f.N;
    Vec3f T = f.p + vecLen * f.T;
    out << "v " << N[0] << " " << N[1] << " " << N[2] << "\n";
    out << "v " << T[0] << " " << T[1] << " " << T[2] << "\n";
    out << "l " << (v0 + 1) << " " << (v0 + 2) << "\n";
    out << "l " << (v0 + 1) << " " << (v0 + 3) << "\n";
    if (i >= 1) {
      out << "l " << (v0 + 1) << " " << (v0 - 2) << "\n";
    }
  }
}

void AddVerts(const std::vector<Vec3f>& vec, TrigMesh& mesh) {
  for (const auto v : vec) {
    mesh.AddVert(v[0], v[1], v[2]);
  }
}

void SetVert(size_t vi, std::vector<float> & verts, const Vec3f & v) {
  verts[3 * vi] = v[0];
  verts[3 * vi + 1] = v[1];
  verts[3 * vi + 2] = v[2];
}

//x in normal axis, y in binormal, z in tangent
TrigMesh Sweep(const FramedCurve& fc, const SimplePolygon& poly) {
  TrigMesh m;
  if (fc.size() < 2) {
    return m;
  }

  size_t numPt = fc.size() * poly.size();
  m.v.resize(numPt * 3);
  size_t vi = 0;
  for (size_t i = 0; i < fc.size(); i++) {
    Vec3f O = fc[i].p;
    Vec3f B = fc[i].T.cross(fc[i].N);
    for (size_t j = 0; j < poly.size(); j++) {
      Vec3f x = O + poly[j][0] * fc[i].N + poly[j][1] * B + poly[j][2]*fc[i].T;
      SetVert(vi, m.v, x);
      vi++;
    }
  }
  for (size_t i = 0; i < fc.size() - 1; i++) {
    
  }
  return m;
}

SimplePolygon MakeTrapezoid(float l0, float l1, float h) {
  SimplePolygon poly;
  Vec3f v(0, -l0 / 2, 0);
  poly.p.resize(4);
  poly.p[0]= v;
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
  mesh = MakeCube(Vec3f(0, 0, 0), Vec3f(1, 1, 1));
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

  TrigMesh m = Sweep(helix, poly);
  m.SaveObj("F:/dump/helix_pt.obj");
  MergeCloseVertices(mesh);
  return 0;
}
