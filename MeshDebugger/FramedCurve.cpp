#include "FramedCurve.h"
#include "CDT.h"
#include <fstream>
void SaveCurveObj(const std::string& file, const FramedCurve& curve,
                  float vecLen) {
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

void SetVert(size_t vi, std::vector<float>& verts, const Vec3f& v) {
  verts[3 * vi] = v[0];
  verts[3 * vi + 1] = v[1];
  verts[3 * vi + 2] = v[2];
}

void AddTrig(std::vector<unsigned>& t, unsigned t0, unsigned t1, unsigned t2) {
  t.push_back(t0);
  t.push_back(t1);
  t.push_back(t2);
}

// x in normal axis, y in binormal, z in tangent
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
      Vec3f x =
          O + poly[j][0] * fc[i].N + poly[j][1] * B + poly[j][2] * fc[i].T;
      SetVert(vi, m.v, x);
      vi++;
    }
  }
  size_t numCols = poly.size();
  for (size_t i = 0; i < fc.size() - 1; i++) {
    for (size_t j = 0; j < numCols; j++) {
      unsigned col1 = (j + 1) % numCols;
      unsigned v0 = i * numCols + j;
      unsigned v1 = v0 + numCols;
      unsigned v2 = i*numCols + col1;
      unsigned v3 = (i+1)*numCols + col1;
      AddTrig(m.t, v0, v2, v1);
      AddTrig(m.t, v1, v2, v3);
    }
  }
  return m;
}

TrigMesh TriangulatePolygon(const SimplePolygon& poly) {
  TrigMesh m;


  return m;
}
