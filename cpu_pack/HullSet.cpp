#include "HullSet.h"

#include "Log.h"
#include "MeshOps.h"
#include "PackingScene.h"

#include <algorithm>
#include <cmath>
#include <fstream>

float HullSet::Distance(const Vec3f &p, float maxDist, Vec3f *normal) const {
  float best = maxDist;
  const HullPlane *bestPlane = nullptr;
  for (const ConvexHull &h : hulls) {
    // cheap reject by the hull box grown by the range of interest.
    if (p[0] < h.box.vmin[0] - best || p[0] > h.box.vmax[0] + best || p[1] < h.box.vmin[1] - best ||
        p[1] > h.box.vmax[1] + best || p[2] < h.box.vmin[2] - best || p[2] > h.box.vmax[2] + best) {
      continue;
    }
    float d = -1e30f;
    const HullPlane *deciding = nullptr;
    for (const HullPlane &pl : h.planes) {
      const float v = pl.n.dot(p) - pl.h;
      if (v > d) {
        d = v;
        deciding = &pl;
        if (d >= best) break;  // already farther than the best hull
      }
    }
    if (d < best) {
      best = d;
      bestPlane = deciding;
    }
  }
  if (normal && bestPlane) *normal = bestPlane->n;
  return best;
}

namespace {

struct RawHull {
  std::vector<Vec3f> v;
  std::vector<unsigned> t;  // 3 per triangle
};

bool ReadHulls(const std::string &file, std::vector<RawHull> &out) {
  std::ifstream in(file);
  if (!in.good()) return false;
  std::string tag;
  unsigned n = 0;
  if (!(in >> tag >> n) || tag != "hulls") return false;
  out.resize(n);
  for (unsigned k = 0; k < n; k++) {
    unsigned nv = 0, nf = 0;
    if (!(in >> tag >> nv >> nf) || tag != "hull") return false;
    out[k].v.resize(nv);
    for (unsigned i = 0; i < nv; i++) {
      float x, y, z;
      in >> x >> y >> z;
      out[k].v[i] = Vec3f(x, y, z);
    }
    out[k].t.resize(3 * size_t(nf));
    for (size_t i = 0; i < 3 * size_t(nf); i++) in >> out[k].t[i];
  }
  return !in.fail();
}

}  // namespace

bool LoadHullSet(const std::string &file, const RigidBody &rb, float inset, float sampleSpacing,
                 HullSet &out) {
  std::vector<RawHull> raw;
  if (!ReadHulls(file, raw) || raw.empty()) return false;
  const Matrix3f Rt = rb.R0.transposed();
  out = HullSet();
  std::vector<std::vector<SamplePoint>> perHullSamples;
  for (RawHull &r : raw) {
    for (Vec3f &v : r.v) v = Rt * (v - rb.oldOrigin);  // input frame to inertia frame
    Vec3f c(0, 0, 0);
    for (const Vec3f &v : r.v) c += v;
    c = c * (1.0f / float(std::max<size_t>(1, r.v.size())));

    ConvexHull hull;
    std::vector<Vec3f> vertNormal(r.v.size(), Vec3f(0, 0, 0));
    std::vector<SamplePoint> samples;
    for (size_t i = 0; i + 2 < r.t.size(); i += 3) {
      const Vec3f &a = r.v[r.t[i]], &b = r.v[r.t[i + 1]], &d = r.v[r.t[i + 2]];
      Vec3f n = (b - a).cross(d - a);
      const float area2 = n.norm();
      if (area2 < 1e-9f) continue;
      n = n * (1.0f / area2);
      float h = n.dot(a);
      // the file is wound outward, but make sure with the centroid.
      if (n.dot(c) - h > 0.0f) {
        n = n * -1.0f;
        h = -h;
      }
      for (int e = 0; e < 3; e++) vertNormal[r.t[i + e]] += n * area2;
      // coplanar triangles of a face share one plane.
      bool dup = false;
      for (const HullPlane &pl : hull.planes) {
        if (pl.n.dot(n) > 0.9999f && std::fabs(pl.h - (h - inset)) < 1e-3f) {
          dup = true;
          break;
        }
      }
      if (!dup) hull.planes.push_back({n, h - inset});
      // face center sample, moved inside by the inset.
      samples.push_back(SamplePoint((a + b + d) * (1.0f / 3.0f) - n * inset, n));
    }
    for (size_t i = 0; i < r.v.size(); i++) {
      const float len = vertNormal[i].norm();
      if (len < 1e-9f) continue;
      Vec3f n = vertNormal[i] * (1.0f / len);
      Vec3f moved = n * inset;
      moved = r.v[i] - moved;
      samples.push_back(SamplePoint(moved, n));
    }
    if (hull.planes.size() < 4) continue;
    hull.box = ComputeBBox(r.v);
    out.hulls.push_back(hull);
    perHullSamples.push_back(std::move(samples));
  }
  if (out.hulls.empty()) return false;
  out.box = out.hulls[0].box;
  for (const ConvexHull &h : out.hulls)
    for (int a = 0; a < 3; a++) {
      out.box.vmin[a] = std::min(out.box.vmin[a], h.box.vmin[a]);
      out.box.vmax[a] = std::max(out.box.vmax[a], h.box.vmax[a]);
    }
  // samples on the outer surface only: drop those inside another hull of the set.
  std::vector<SamplePoint> all;
  for (size_t k = 0; k < out.hulls.size(); k++) {
    for (const SamplePoint &s : perHullSamples[k]) {
      bool inside = false;
      for (size_t j = 0; j < out.hulls.size() && !inside; j++) {
        if (j == k) continue;
        float d = -1e30f;
        for (const HullPlane &pl : out.hulls[j].planes) d = std::max(d, pl.n.dot(s.x) - pl.h);
        inside = d < -0.05f;
      }
      if (!inside) all.push_back(s);
    }
  }
  out.samples = DownsamplePoints(all, sampleSpacing);
  return !out.samples.empty();
}

const HullSet *HullLibrary::Get(unsigned itemIdx, const MeshInfo &item) {
  auto it = sets_.find(itemIdx);
  if (it != sets_.end()) return &it->second;
  if (missing_.count(itemIdx)) return nullptr;
  HullSet set;
  if (!LoadHullSet(dir_ + "/" + item.name + ".hulls.txt", item.rb, inset_, spacing_, set)) {
    LOGI("no hulls for " << item.name << " in " << dir_ << char(10));
    missing_[itemIdx] = true;
    return nullptr;
  }
  return &sets_.emplace(itemIdx, std::move(set)).first->second;
}

bool SaveHullsObj(const std::string &file, const RigidBody &rb, const RigidTransform &tran,
                  const std::string &outFile) {
  std::vector<RawHull> raw;
  if (!ReadHulls(file, raw) || raw.empty()) return false;
  const Matrix3f Rt = rb.R0.transposed();
  std::ofstream out(outFile);
  if (!out.good()) return false;
  size_t base = 1;
  for (size_t k = 0; k < raw.size(); k++) {
    out << "o hull" << k << char(10);
    for (const Vec3f &v0 : raw[k].v) {
      const Vec3f v = tran.rotation * (Rt * (v0 - rb.oldOrigin)) + tran.position;
      out << "v " << v[0] << " " << v[1] << " " << v[2] << char(10);
    }
    for (size_t i = 0; i + 2 < raw[k].t.size(); i += 3) {
      out << "f " << base + raw[k].t[i] << " " << base + raw[k].t[i + 1] << " " << base + raw[k].t[i + 2] << char(10);
    }
    base += raw[k].v.size();
  }
  return true;
}
