#include "SphereCover.h"

#include "AdapSDF.h"
#include "ContainerShell.h"
#include "CreviceFaces.h"
#include "CreviceMouths.h"
#include "Log.h"
#include "MeshOps.h"
#include "PackShrinkWrap.h"
#include "PointSample.h"
#include "ShellPasses.h"
#include "Stopwatch.h"

#include <algorithm>
#include <array>
#include <map>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>

bool LoadObjVertices(const std::string &path, std::vector<Vec3f> &out) {
  std::ifstream in(path);
  if (!in.good()) return false;
  std::string line;
  while (std::getline(in, line)) {
    if (line.size() < 2 || line[0] != 'v' || (line[1] != ' ' && line[1] != '\t')) continue;
    std::istringstream ls(line.substr(1));
    float x, y, z;
    if (ls >> x >> y >> z) out.push_back(Vec3f(x, y, z));
  }
  return true;
}

namespace {

const float kInf = std::numeric_limits<float>::max();
const float kCoverMargin = 0.05f;

float WorldBoxCenterX(const PackingScene &scene, const InstanceInfo &inst) {
  const TrigMesh world = MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran);
  const Box3f box = ComputeBBox(world.v);
  return 0.5f * (box.vmin[0] + box.vmax[0]);
}

// uniform spatial hash of point indices.
class PointHash {
public:
  void Init(float cellSize) { cell = cellSize; }
  void Add(unsigned id, const Vec3f &p) { cells[Key(Coord(p[0]), Coord(p[1]), Coord(p[2]))].push_back(id); }
  // calls f(id) for every id whose cell overlaps the box [p-r, p+r].
  template <class F> void Query(const Vec3f &p, float r, F f) const {
    const int x0 = Coord(p[0] - r), x1 = Coord(p[0] + r);
    const int y0 = Coord(p[1] - r), y1 = Coord(p[1] + r);
    const int z0 = Coord(p[2] - r), z1 = Coord(p[2] + r);
    for (int x = x0; x <= x1; ++x)
      for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z) {
          auto it = cells.find(Key(x, y, z));
          if (it == cells.end()) continue;
          for (unsigned id : it->second) f(id);
        }
  }

private:
  int Coord(float v) const { return int(std::floor(v / cell)); }
  static uint64_t Key(int x, int y, int z) {
    return (uint64_t(x + (1 << 20)) << 42) | (uint64_t(y + (1 << 20)) << 21) |
           uint64_t(z + (1 << 20));
  }
  float cell = 1.0f;
  std::unordered_map<uint64_t, std::vector<unsigned>> cells;
};

struct Sphere {
  Vec3f c;
  float r = 0.0f;
};

// signed volume of a closed mesh (divergence theorem).
float MeshVolume(const TrigMesh &m) {
  double vol = 0.0;
  for (size_t i = 0; i < m.GetNumTrigs(); ++i) {
    Triangle t = m.GetTriangleVerts(i);
    vol += double(t[0].dot(t[1].cross(t[2]))) / 6.0;
  }
  return float(std::fabs(vol));
}

// unit icosphere (icosahedron subdivided `levels` times).
void MakeIcosphere(unsigned levels, std::vector<Vec3f> &verts,
                   std::vector<std::array<unsigned, 3>> &faces) {
  const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
  const float base[12][3] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                             {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  verts.clear();
  for (const auto &b : base) {
    Vec3f v(b[0], b[1], b[2]);
    v.normalize();
    verts.push_back(v);
  }
  const unsigned f0[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
                              {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                              {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
                              {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
  faces.clear();
  for (const auto &f : f0) faces.push_back({f[0], f[1], f[2]});
  for (unsigned lv = 0; lv < levels; ++lv) {
    std::map<std::pair<unsigned, unsigned>, unsigned> mid;
    auto midpoint = [&](unsigned a, unsigned b) {
      const auto key = std::make_pair(std::min(a, b), std::max(a, b));
      auto it = mid.find(key);
      if (it != mid.end()) return it->second;
      Vec3f m = (verts[a] + verts[b]) * 0.5f;
      m.normalize();
      verts.push_back(m);
      return mid[key] = unsigned(verts.size() - 1);
    };
    std::vector<std::array<unsigned, 3>> next;
    for (const auto &f : faces) {
      const unsigned a = midpoint(f[0], f[1]), b = midpoint(f[1], f[2]), c = midpoint(f[2], f[0]);
      next.push_back({f[0], a, c});
      next.push_back({f[1], b, a});
      next.push_back({f[2], c, b});
      next.push_back({a, b, c});
    }
    faces.swap(next);
  }
}

// discrete sphere radii: effective (volume-equivalent) radius of each medium
// kind times each scale, merged when closer than 0.25 cm.
std::vector<float> BuildRadii(PackingScene &scene, const PackingStep &step,
                              const PackingConfig &cfg) {
  std::vector<float> raw;
  for (const auto &name : step.names) {
    auto it = scene.nameToIndex.find(name);
    if (it == scene.nameToIndex.end()) continue;
    const MeshInfo &item = scene.items[it->second];
    const Vec3f ctr = (item.box.vmin + item.box.vmax) * 0.5f;
    float outer = 0.0f;
    for (size_t i = 0; i < item.mesh.GetNumVerts(); ++i)
      outer = std::max(outer, (item.mesh.GetVertex(unsigned(i)) - ctr).norm());
    const float eff = std::cbrt(3.0f * MeshVolume(item.mesh) / (4.0f * 3.14159265f));
    LOGI("sphere cover: kind " << name << " outer radius " << outer << " effective radius "
                               << eff << "\n");
    for (float s : cfg.sphereScales) raw.push_back(eff * s);
  }
  std::sort(raw.begin(), raw.end());
  std::vector<float> radii;
  for (float r : raw)
    if (r > 0.1f && (radii.empty() || r - radii.back() > 0.25f)) radii.push_back(r);
  return radii;
}

} // namespace

namespace {

struct Cand {
  float score = 0.0f;
  Vec3f c;
  unsigned ri = 0;
  unsigned stamp = 0;
  bool operator<(const Cand &o) const { return score < o.score; }
};

class Coverer {
public:
  Coverer(PackingScene &s, const PackingConfig &c, std::vector<float> r)
      : scene(s), cfg(c), radii(std::move(r)) {
    regionFruit.resize(scene.instances.size(), 0);
    for (unsigned id = 0; id < scene.instances.size(); ++id) {
      const auto &inst = scene.instances[id];
      if (inst.isExclusion || scene.items[inst.itemId].BoxDiagonal() <= cfg.sphereSupportMinDiag)
        continue;
      regionFruit[id] = cfg.sphereMaxX == kInf || WorldBoxCenterX(scene, inst) < cfg.sphereMaxX;
    }
  }

  void SetTargets(std::vector<Vec3f> p, bool uniform) {
    pts = std::move(p);
    covered.assign(pts.size(), 0);
    tgtHash.Init(3.0f);
    for (unsigned i = 0; i < pts.size(); ++i) tgtHash.Add(i, pts[i]);
    // uniform weights: a point in a dense cluster counts for less.
    w.assign(pts.size(), 1.0f);
    for (unsigned i = 0; i < pts.size(); ++i) {
      unsigned n = 0;
      tgtHash.Query(pts[i], 1.5f, [&](unsigned j) {
        if ((pts[j] - pts[i]).norm2() <= 1.5f * 1.5f) ++n;
      });
      w[i] = uniform ? 1.0f : 1.0f / float(std::max(1u, n));
      totalW += w[i];
    }
    rmax = radii.back();
    sphHash.Init(2.0f * rmax + cfg.sphereContactTol + 0.01f);
  }

  void SetMouthGuidance(const std::vector<Vec3f> &mouths, float multiplier) {
    mouthCenters = mouths;
    mouthCovered.assign(mouths.size(), 0);
    mouthHash.Init(3.0f);
    for (unsigned i = 0; i < mouths.size(); ++i) mouthHash.Add(i, mouths[i]);
    mouthPriority.assign(pts.size(), 0);
    if (multiplier <= 1.0f) return;
    for (const Vec3f &m : mouths) {
      unsigned best = ~0u;
      float best2 = 2.0f * 2.0f;
      tgtHash.Query(m, 2.0f, [&](unsigned i) {
        const float d2 = (pts[i] - m).norm2();
        if (d2 < best2) { best2 = d2; best = i; }
      });
      if (best != ~0u) mouthPriority[best] = 1;
    }
    unsigned prioritized = 0;
    for (unsigned i = 0; i < pts.size(); ++i) {
      if (!mouthPriority[i]) continue;
      totalW += w[i] * (multiplier - 1.0f);
      w[i] *= multiplier;
      ++prioritized;
    }
    LOGI("sphere cover: " << prioritized << " targets prioritized near medium-width mouths, weight "
         << multiplier << "\n");
  }

  // target i is covered by sphere (c, r) when it lies inside it.
  float SegDist(const Vec3f &c, unsigned i) const { return (pts[i] - c).norm(); }
  bool Covers(const Vec3f &c, float r, unsigned i) const {
    return SegDist(c, i) <= r + kCoverMargin;
  }

  // drops points closer than minClear to a large fruit (or inside one), using
  // the exact fruit SDFs. the field's unsigned distance is positive inside fruit.
  std::vector<Vec3f> KeepClear(const std::vector<Vec3f> &in, float minClear) const {
    std::vector<Vec3f> out;
    for (const Vec3f &p : in) {
      float d;
      NearestFruit(p, 18.0f, d, nullptr);
      if (d >= minClear) out.push_back(p);
    }
    return out;
  }

  void Run();
  void FillMouths();
  void AuditOpenMouths(const char *stage);
  void RepairMouths();
  void Probe(const Vec3f &p);
  void Save() const;

  std::vector<Vec3f> pts;
  std::vector<float> w;
  std::vector<char> covered;
  std::vector<char> baseCovered;
  std::vector<Sphere> spheres;
  double totalW = 0.0, coveredW = 0.0;
  // evaluation rejection reasons.
  unsigned rejContainer = 0, rejExclusion = 0, rejFruitOverlap = 0, rejNoContact = 0,
           rejSphereOverlap = 0, rejFruitTriangle = 0, rejLowGain = 0, rejNoMouth = 0;

private:
  float Cost(float r) const { return cfg.sphereCost + cfg.sphereSizeCost * r; }

  // nearest non-exclusion fruit to p: distance (negative inside) and the
  // direction pointing away from its surface.
  bool NearestFruit(const Vec3f &p, float range, float &dist, Vec3f *away) const {
    Box3f q;
    q.vmin = q.vmax = p;
    dist = kInf;
    for (unsigned id : scene.broadPhase.GetNearby(q, range)) {
      const auto &inst = scene.instances[id];
      const auto &item = scene.items[inst.itemId];
      if (inst.isExclusion || !item.sdf) continue;
      if (!regionFruit[id]) continue;
      const float scale = std::max(inst.tran.scale, 0.001f);
      const Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position) / scale;
      const float d = scale * item.sdf->GetCoarseDist(local);
      if (d >= dist) continue;
      dist = d;
      if (!away) continue;
      const float h = 0.5f / scale;
      Vec3f g(item.sdf->GetCoarseDist(local + Vec3f(h, 0, 0)) -
                  item.sdf->GetCoarseDist(local - Vec3f(h, 0, 0)),
              item.sdf->GetCoarseDist(local + Vec3f(0, h, 0)) -
                  item.sdf->GetCoarseDist(local - Vec3f(0, h, 0)),
              item.sdf->GetCoarseDist(local + Vec3f(0, 0, h)) -
                  item.sdf->GetCoarseDist(local - Vec3f(0, 0, h)));
      if (g.norm2() < 1e-6f) continue;
      g.normalize();
      *away = inst.tran.rotation * g;
    }
    return dist < kInf;
  }

  // The coarse fruit SDF can overestimate clearance by several millimeters.
  // Query actual mesh triangles before accepting a candidate sphere.
  bool FitsFruitMesh(const Vec3f &c, float r) {
    Box3f q;
    q.vmin = q.vmax = c;
    for (unsigned id : scene.broadPhase.GetNearby(q, r + 1.0f)) {
      const auto &inst = scene.instances[id];
      if (inst.isExclusion || !regionFruit[id]) continue;
      auto it = scene.kindGrids.find(inst.itemId);
      if (it == scene.kindGrids.end()) {
        auto grid = std::make_shared<TrigGrid>();
        grid->Build(scene.items[inst.itemId].mesh, 1.0f);
        it = scene.kindGrids.emplace(inst.itemId, std::move(grid)).first;
      }
      const float scale = std::max(inst.tran.scale, 0.001f);
      const Vec3f local = inst.tran.rotation.transposed() * (c - inst.tran.position) / scale;
      const float limit = std::max(0.0f, (r - cfg.sphereOverlapCm) / scale);
      if (it->second->NearestTriangle(local, limit).dist < limit) return false;
    }
    return true;
  }

  float Gain(const Vec3f &c, float r) const {
    float g = 0.0f;
    tgtHash.Query(c, r + kCoverMargin, [&](unsigned i) {
      if (!covered[i] && Covers(c, r, i)) g += w[i];
    });
    if (cfg.mouthGainWeight > 0.0f)
      mouthHash.Query(c, r + kCoverMargin, [&](unsigned i) {
        if (!mouthCovered[i] && (mouthCenters[i] - c).norm2() <=
            (r + kCoverMargin) * (r + kCoverMargin))
          g += cfg.mouthGainWeight;
      });
    return g;
  }

  bool CoversNewMouth(const Vec3f &c, float r) const {
    bool hit = false;
    mouthHash.Query(c, r + kCoverMargin, [&](unsigned i) {
      if (!mouthCovered[i] && (mouthCenters[i] - c).norm2() <=
          (r + kCoverMargin) * (r + kCoverMargin)) hit = true;
    });
    return hit;
  }

  // score of sphere (c, radii[ri]) given the current cover, or -kInf if it
  // violates a proxy constraint.
  float Eval(const Vec3f &c, unsigned ri, int *reason = nullptr,
             const std::vector<char> *ignore = nullptr) {
    if (reason) *reason = 0;
    if (forceIdx >= 0 && !Covers(c, radii[ri], unsigned(forceIdx))) {
      if (reason) *reason = 6;
      return -kInf;
    }
    const float r = radii[ri];
    if (frontNeedsMouth && !CoversNewMouth(c, r)) {
      ++rejNoMouth; if (reason) *reason = 8;
      return -kInf;
    }
    const float tol = cfg.sphereContactTol;
    if (scene.sdf->GetCoarseDist(c) > -(1.0f - cfg.sphereHardFrac) * r) {
      ++rejContainer; if (reason) *reason = 1;
      return -kInf;
    }
    if (scene.exclusionItemIndex != ~0u) {
      const auto &excl = scene.items[scene.exclusionItemIndex];
      if (excl.sdf && excl.sdf->GetCoarseDist(c) < (1.0f - cfg.sphereHardFrac) * r) {
        ++rejExclusion; if (reason) *reason = 2;
        return -kInf;
      }
    }
    float d;
    NearestFruit(c, r + tol + 1.0f, d, nullptr);
    if (d < r - cfg.sphereOverlapCm) {
      ++rejFruitOverlap; if (reason) *reason = 3;
      return -kInf;
    }
    bool contact = d - r <= tol;
    bool bad = false;
    sphHash.Query(c, 2.0f * rmax + tol, [&](unsigned j) {
      if (ignore && (*ignore)[j]) return;
      const Sphere &o = spheres[j];
      const float gap = (o.c - c).norm() - r - o.r;
      if (gap <= tol) contact = true;
      if (-gap > cfg.sphereOverlapCm) bad = true;
    });
    if (bad) {
      ++rejSphereOverlap; if (reason) *reason = 4;
      return -kInf;
    }
    if (!FitsFruitMesh(c, r)) {
      ++rejFruitTriangle; if (reason) *reason = 7;
      return -kInf;
    }
    if (!contact) {
      ++rejNoContact; if (reason) *reason = 5;
      return -kInf;
    }
    return Gain(c, r) - Cost(r);
  }

  void Refine(Cand &cand) {
    static std::vector<Vec3f> dirs;
    if (dirs.empty())
      for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
          for (int z = -1; z <= 1; ++z)
            if (x || y || z) {
              Vec3f d(x * 1.0f, y * 1.0f, z * 1.0f);
              d.normalize();
              dirs.push_back(d);
            }
    for (float hf : {0.4f, 0.2f, 0.1f}) {
      for (unsigned iter = 0; iter < 3; ++iter) {
        const float h = hf * radii[cand.ri];
        Cand best = cand;
        for (const Vec3f &d : dirs) {
          const Vec3f c2 = cand.c + h * d;
          const float s = Eval(c2, cand.ri);
          if (s > best.score) { best.score = s; best.c = c2; best.ri = cand.ri; }
        }
        for (int dr : {-1, 1}) {
          const int ri2 = int(cand.ri) + dr;
          if (ri2 < 0 || ri2 >= int(radii.size())) continue;
          const float s = Eval(cand.c, unsigned(ri2));
          if (s > best.score) { best.score = s; best.c = cand.c; best.ri = unsigned(ri2); }
        }
        if (best.score <= cand.score) break;
        cand = best;
      }
    }
  }

  void Accept(const Cand &cand) {
    const float r = radii[cand.ri];
    tgtHash.Query(cand.c, r + kCoverMargin, [&](unsigned i) {
      if (!covered[i] && Covers(cand.c, r, i)) {
        covered[i] = 1;
        coveredW += w[i];
      }
    });
    mouthHash.Query(cand.c, r + kCoverMargin, [&](unsigned i) {
      if ((mouthCenters[i] - cand.c).norm2() <=
          (r + kCoverMargin) * (r + kCoverMargin)) mouthCovered[i] = 1;
    });
    sphHash.Add(unsigned(spheres.size()), cand.c);
    spheres.push_back({cand.c, r});
  }

  PackingScene &scene;
  const PackingConfig &cfg;
  std::vector<float> radii;
  std::vector<char> regionFruit;
  std::vector<char> mouthPriority;
  std::vector<char> mouthCovered;
  std::vector<Vec3f> mouthCenters;
  float rmax = 0.0f;
  bool frontNeedsMouth = false;
  // when set, only spheres that cover this target are valid.
  int forceIdx = -1;
  PointHash tgtHash, sphHash, mouthHash;
};

void Coverer::FillMouths() {
  if (cfg.mouthPlacementLimit == 0 || mouthCenters.empty()) return;
  Utils::Stopwatch clock;
  clock.Start();
  struct Guide { Vec3f p, away, inward, side; float fruitDist; unsigned mouthId; };
  std::vector<Guide> guides;
  guides.reserve(mouthCenters.size());
  for (unsigned mouthId = 0; mouthId < mouthCenters.size(); ++mouthId) {
    const Vec3f &p = mouthCenters[mouthId];
    float d;
    Vec3f away(0, 0, 0);
    if (!NearestFruit(p, 18.0f, d, &away) || away.norm2() < 1e-8f) continue;
    away.normalize();
    Vec3f inward = -scene.sdf->GetCoarseGrad(p);
    if (inward.norm2() < 1e-8f) inward = away;
    inward = inward - inward.dot(away) * away;
    if (inward.norm2() < 1e-8f)
      inward = std::fabs(away[0]) < 0.8f ? Vec3f(1, 0, 0) : Vec3f(0, 1, 0);
    inward = inward - inward.dot(away) * away;
    inward.normalize();
    Vec3f side = away.cross(inward);
    side.normalize();
    guides.push_back({p, away, inward, side, d, mouthId});
  }
  unsigned placed = 0;
  for (; placed < cfg.mouthPlacementLimit && spheres.size() < cfg.sphereMaxCount; ++placed) {
    Cand best;
    best.score = 0.0f;
    unsigned bestGuide = ~0u;
    unsigned openMouths = 0;
    for (unsigned mi = 0; mi < guides.size(); ++mi) {
      const Guide &g = guides[mi];
      if (mouthCovered[g.mouthId]) continue;
      ++openMouths;
      for (unsigned ri = 0; ri < radii.size(); ++ri) {
        const float r = radii[ri];
        const Vec3f tangent = g.p + (r - g.fruitDist) * g.away;
        const Vec3f centers[] = {g.p, tangent,
                                 tangent + 0.3f * r * g.inward,
                                 tangent - 0.3f * r * g.inward,
                                 tangent + 0.3f * r * g.side,
                                 tangent - 0.3f * r * g.side,
                                 tangent + 0.6f * r * g.inward,
                                 tangent - 0.6f * r * g.inward,
                                 tangent + 0.6f * r * g.side,
                                 tangent - 0.6f * r * g.side,
                                 tangent + 0.4f * r * (g.inward + g.side),
                                 tangent + 0.4f * r * (g.inward - g.side),
                                 tangent - 0.4f * r * (g.inward + g.side),
                                 tangent - 0.4f * r * (g.inward - g.side)};
        for (const Vec3f &c : centers) {
          if ((c - g.p).norm2() > (r + kCoverMargin) * (r + kCoverMargin)) continue;
          const float score = Eval(c, ri);
          if (score > best.score) {
            best = {score, c, ri};
            bestGuide = mi;
          }
        }
      }
    }
    if (bestGuide == ~0u) break;
    Cand refined = best;
    Refine(refined);
    if (refined.score > best.score &&
        (refined.c - guides[bestGuide].p).norm2() <=
            (radii[refined.ri] + kCoverMargin) * (radii[refined.ri] + kCoverMargin))
      best = refined;
    Accept(best);
    if ((placed + 1) % 50 == 0)
      LOGI("sphere cover: mouth phase " << placed + 1 << " spheres, " << openMouths
           << " open centers, " << clock.ElapsedMS() / 1000.0 << " s\n");
  }
  LOGI("sphere cover: mouth phase placed " << placed << " spheres in "
       << clock.ElapsedMS() / 1000.0 << " s\n");
}

void Coverer::Run() {
  Utils::Stopwatch clock;
  clock.Start();
  // targets inside or on the surface of an existing fruit are already covered.
  unsigned initiallyCovered = 0;
  {
    unsigned hb[7] = {0};
    const float he[6] = {0.0f, 0.2f, 0.5f, 1.0f, 2.0f, 4.0f};
    for (unsigned i = 0; i < pts.size(); ++i) {
      float d;
      NearestFruit(pts[i], 18.0f, d, nullptr);
      unsigned k = 0;
      while (k < 6 && d >= he[k]) ++k;
      ++hb[k];
    }
    LOGI("sphere cover: target fruit-dist hist <0 " << hb[0] << " <.2 " << hb[1] << " <.5 " << hb[2]
         << " <1 " << hb[3] << " <2 " << hb[4] << " <4 " << hb[5] << " >=4 " << hb[6] << "\n");
  }
  for (unsigned i = 0; i < pts.size(); ++i) {
    float d;
    NearestFruit(pts[i], 1.0f, d, nullptr);
    if (d <= 0.2f) {
      covered[i] = 1;
      totalW -= w[i];
      ++initiallyCovered;
    }
  }
  LOGI("sphere cover: " << initiallyCovered << " targets already covered by existing fruit\n");
  baseCovered = covered;

  FillMouths();
  frontNeedsMouth = cfg.sphereFrontRequireMouth;

  // advancing front of spheres. seeds touch a fruit; every later sphere is
  // grown from the front so that it touches an existing sphere. a front sphere
  // with no uncovered targets within reach is retired.
  std::vector<unsigned> order;
  for (unsigned i = 0; i < pts.size(); ++i)
    if (!covered[i]) order.push_back(i);
  std::sort(order.begin(), order.end(), [&](unsigned a, unsigned b) {
    if (!mouthPriority.empty() && mouthPriority[a] != mouthPriority[b])
      return mouthPriority[a] > mouthPriority[b];
    if (pts[a][0] != pts[b][0]) return pts[a][0] < pts[b][0];
    if (pts[a][1] != pts[b][1]) return pts[a][1] < pts[b][1];
    return pts[a][2] < pts[b][2];
  });
  std::vector<char> blocked(pts.size(), 0);
  const float tol = cfg.sphereContactTol;
  const float reachExtra = 2.0f * rmax;

  // best valid seed sphere that contains target i and touches a fruit.
  auto seedFor = [&](unsigned i, Cand &best) {
    const Vec3f t = pts[i];
    forceIdx = int(i);
    float d0 = 0.0f;
    Vec3f away = -scene.sdf->GetCoarseGrad(t);
    if (!NearestFruit(t, 18.0f, d0, &away)) d0 = 0.0f;
    d0 = std::max(d0, 0.0f);
    Vec3f inward = -scene.sdf->GetCoarseGrad(t);
    if (away.norm2() > 1e-6f) away.normalize();
    if (inward.norm2() > 1e-6f) inward.normalize();
    best = Cand();
    best.score = -kInf;
    for (unsigned ri = 0; ri < radii.size(); ++ri) {
      const float r = radii[ri];
      const Vec3f seedsC[3] = {t + (r - d0) * away, t + r * inward, t};
      for (const Vec3f &s0 : seedsC) {
        Vec3f c = s0;
        for (unsigned fix = 0; fix < 2; ++fix) {
          const float need = scene.sdf->GetCoarseDist(c) + (1.0f - cfg.sphereHardFrac) * r + 0.05f;
          if (need <= 0.0f) break;
          Vec3f g = scene.sdf->GetCoarseGrad(c);
          if (g.norm2() < 1e-6f) break;
          g.normalize();
          c = c - need * g;
        }
        Cand cand;
        cand.c = c;
        cand.ri = ri;
        cand.score = Eval(c, ri);
        if (cand.score <= -kInf) continue;
        Refine(cand);
        if (cand.score > best.score) best = cand;
      }
    }
    forceIdx = -1;
    if (best.score <= 0.0f && best.score > -kInf) ++rejLowGain;
    return best.score > 0.0f;
  };

  // best valid sphere that touches sphere S and contains target i.
  auto growFrom = [&](const Sphere &S, unsigned i, Cand &best) {
    const Vec3f t = pts[i];
    forceIdx = int(i);
    Vec3f dirT = t - S.c;
    best = Cand();
    best.score = -kInf;
    if (dirT.norm2() < 1e-8f) { forceIdx = -1; return false; }
    dirT.normalize();
    Vec3f u = std::fabs(dirT[0]) < 0.9f ? Vec3f(1, 0, 0) : Vec3f(0, 1, 0);
    u = u - dirT.dot(u) * dirT;
    u.normalize();
    const Vec3f v = dirT.cross(u);
    std::vector<Vec3f> dirs = {dirT};
    const struct { float ang; int n; } rings[2] = {{0.436f, 6}, {0.873f, 8}};
    for (const auto &ring : rings)
      for (int k = 0; k < ring.n; ++k) {
        const float phi = 6.2831853f * float(k) / float(ring.n);
        const Vec3f perp = std::cos(phi) * u + std::sin(phi) * v;
        dirs.push_back(std::cos(ring.ang) * dirT + std::sin(ring.ang) * perp);
      }
    for (unsigned ri = 0; ri < radii.size(); ++ri) {
      const float r = radii[ri];
      for (const Vec3f &d : dirs) {
        const Vec3f c = S.c + (S.r + r - 0.05f) * d;
        const float s = Eval(c, ri);
        if (s > best.score) {
          best.score = s;
          best.c = c;
          best.ri = ri;
        }
      }
    }
    forceIdx = -1;
    if (best.score <= 0.0f && best.score > -kInf) ++rejLowGain;
    return best.score > 0.0f;
  };

  // uncovered, unblocked targets within reach of S, nearest to its surface first.
  auto nearbyUncovered = [&](const Sphere &S) {
    std::vector<std::pair<float, unsigned>> out;
    const float reach = S.r + reachExtra + tol;
    tgtHash.Query(S.c, reach, [&](unsigned i) {
      if (covered[i] || blocked[i]) return;
      const float d = (pts[i] - S.c).norm();
      if (d <= reach) out.push_back({d - S.r, i});
    });
    std::sort(out.begin(), out.end());
    return out;
  };

  std::deque<unsigned> front;
  for (unsigned i = 0; i < spheres.size(); ++i) front.push_back(i);
  unsigned seedsPlaced = 0, grown = 0, retired = 0, seedFailed = 0;
  size_t next = 0;
  while (spheres.size() < cfg.sphereMaxCount) {
    if (front.empty()) {
      // start a new chain from the next uncovered target in sweep order.
      while (next < order.size() && (covered[order[next]] || blocked[order[next]])) ++next;
      if (next >= order.size()) break;
      const unsigned i = order[next];
      Cand seed;
      if (seedFor(i, seed)) {
        Accept(seed);
        front.push_back(unsigned(spheres.size() - 1));
        ++seedsPlaced;
      } else {
        blocked[i] = 1;
        ++seedFailed;
      }
      continue;
    }
    const unsigned si = front.front();
    front.pop_front();
    const Sphere S = spheres[si];
    const auto near = nearbyUncovered(S);
    if (near.empty()) {
      ++retired;
      continue;
    }
    // try the nearest few targets; keep S on the front if one of them worked.
    bool placed = false;
    for (size_t k = 0; k < near.size() && k < 6 && !placed; ++k) {
      Cand c;
      if (!growFrom(S, near[k].second, c)) continue;
      Accept(c);
      ++grown;
      placed = true;
      front.push_back(unsigned(spheres.size() - 1));
      front.push_back(si);
    }
    if (!placed) ++retired;
    if (spheres.size() % 200 == 0 && placed)
      LOGI("sphere cover: " << spheres.size() << " spheres, " << 100.0 * coveredW / totalW
                            << "% covered, front " << front.size() << ", "
                            << clock.ElapsedMS() / 1000.0 << " s\n");
  }
  LOGI("sphere cover: " << seedsPlaced << " seeds, " << grown << " grown, " << retired
                        << " retired, " << seedFailed << " targets with no valid seed\n");
  unsigned nCovered = 0;
  for (char c : covered) nCovered += c;
  LOGI("sphere cover: " << spheres.size() << " spheres cover " << nCovered << "/" << pts.size()
                        << " targets (" << 100.0 * coveredW / totalW << "% by weight) in "
                        << clock.ElapsedMS() / 1000.0 << " s\n");
  {
    // diagnostic: fruit / container distance of uncovered targets.
    unsigned fb[6] = {0}, cb[6] = {0}, n = 0;
    const float fe[5] = {0.0f, 0.5f, 1.0f, 2.0f, 4.0f}, ce[5] = {-16.0f, -8.0f, -4.0f, -2.0f, 0.0f};
    for (size_t i = 0; i < pts.size(); i += 1) {
      if (covered[i]) continue;
      float d;
      NearestFruit(pts[i], 18.0f, d, nullptr);
      unsigned k = 0;
      while (k < 5 && d >= fe[k]) ++k;
      ++fb[k];
      const float cd = scene.sdf->GetCoarseDist(pts[i]);
      k = 0;
      while (k < 5 && cd >= ce[k]) ++k;
      ++cb[k];
      ++n;
    }
    LOGI("sphere cover: uncovered fruit-dist hist <0 " << fb[0] << " <.5 " << fb[1] << " <1 " << fb[2]
                                                       << " <2 " << fb[3] << " <4 " << fb[4] << " >=4 " << fb[5] << "\n");
    LOGI("sphere cover: uncovered container-dist hist <-16 " << cb[0] << " <-8 " << cb[1] << " <-4 " << cb[2]
                                                             << " <-2 " << cb[3] << " <0 " << cb[4] << " >=0 " << cb[5] << "\n");
  }
  LOGI("sphere cover: rejections container " << rejContainer << " exclusion " << rejExclusion
                                             << " fruit_overlap " << rejFruitOverlap
                                             << " fruit_triangle " << rejFruitTriangle
                                             << " no_contact " << rejNoContact
                                             << " sphere_overlap " << rejSphereOverlap
                                             << " low_gain " << rejLowGain
                                             << " no_mouth " << rejNoMouth << "\n");
}

void Coverer::RepairMouths() {
  if (cfg.sphereMouthRepairPasses == 0 || mouthCenters.empty()) return;
  Utils::Stopwatch clock;
  clock.Start();
  unsigned applied = 0;
  for (unsigned pass = 0; pass < cfg.sphereMouthRepairPasses; ++pass) {
    std::vector<unsigned> support(mouthCenters.size(), 0);
    std::vector<char> fruitContact(spheres.size(), 0);
    for (unsigned si = 0; si < spheres.size(); ++si) {
      float d;
      NearestFruit(spheres[si].c, spheres[si].r + cfg.sphereContactTol + 1.0f, d, nullptr);
      fruitContact[si] = d - spheres[si].r <= cfg.sphereContactTol;
    }
    for (const Sphere &s : spheres)
      mouthHash.Query(s.c, s.r + kCoverMargin, [&](unsigned i) {
        const float rr = s.r + kCoverMargin;
        if ((mouthCenters[i] - s.c).norm2() <= rr * rr) ++support[i];
      });
    int bestDelta = 0, bestGain = 0;
    float bestScore = 0.0f;
    Cand best;
    std::vector<unsigned> bestConflicts;
    for (unsigned mi = 0; mi < mouthCenters.size(); ++mi) {
      if (mouthCovered[mi]) continue;
      const Vec3f p = mouthCenters[mi];
      for (int ix = -5; ix <= 5; ++ix)
        for (int iy = -5; iy <= 5; ++iy)
          for (int iz = -5; iz <= 5; ++iz) {
            const Vec3f c = p + Vec3f(float(ix), float(iy), float(iz));
            const float d2 = (c - p).norm2();
            for (unsigned ri = 0; ri < radii.size(); ++ri) {
              const float r = radii[ri];
              const float rr = r + kCoverMargin;
              if (d2 > rr * rr) continue;
              std::vector<unsigned> conflicts;
              for (unsigned si = 0; si < spheres.size(); ++si) {
                const Sphere &s = spheres[si];
                if ((s.c - c).norm() < s.r + r - cfg.sphereOverlapCm)
                  conflicts.push_back(si);
                if (conflicts.size() > 2) break;
              }
              if (conflicts.size() > 2) continue;
              std::vector<char> ignore(spheres.size(), 0);
              for (unsigned si : conflicts) ignore[si] = 1;
              const float score = Eval(c, ri, nullptr, &ignore);
              if (score <= 0.0f) continue;
              int gain = 0, lost = 0;
              for (unsigned mj = 0; mj < mouthCenters.size(); ++mj) {
                const bool hit = (mouthCenters[mj] - c).norm2() <= rr * rr;
                if (!mouthCovered[mj]) {
                  if (hit) ++gain;
                  continue;
                }
                if (hit) continue;
                unsigned removed = 0;
                for (unsigned si : conflicts) {
                  const Sphere &s = spheres[si];
                  const float sr = s.r + kCoverMargin;
                  if ((mouthCenters[mj] - s.c).norm2() <= sr * sr) ++removed;
                }
                if (removed == support[mj]) ++lost;
              }
              const int delta = gain - lost;
              const bool improves = delta > bestDelta ||
                  (delta == bestDelta && delta > 0 &&
                   (gain > bestGain || (gain == bestGain && score > bestScore)));
              if (improves) {
                // Removing a support sphere must not strand any sphere it touched.
                bool preservesContacts = true;
                for (unsigned sj = 0; sj < spheres.size() && preservesContacts; ++sj) {
                  if (ignore[sj] || fruitContact[sj]) continue;
                  const Sphere &old = spheres[sj];
                  bool affected = false;
                  for (unsigned ci : conflicts) {
                    const Sphere &gone = spheres[ci];
                    if ((old.c - gone.c).norm() - old.r - gone.r <= cfg.sphereContactTol)
                      affected = true;
                  }
                  if (!affected) continue;
                  bool stillContacts = (old.c - c).norm() - old.r - r <= cfg.sphereContactTol;
                  for (unsigned sk = 0; sk < spheres.size() && !stillContacts; ++sk) {
                    if (sk == sj || ignore[sk]) continue;
                    const Sphere &other = spheres[sk];
                    stillContacts = (old.c - other.c).norm() - old.r - other.r <= cfg.sphereContactTol;
                  }
                  if (!stillContacts) preservesContacts = false;
                }
                if (!preservesContacts) continue;
                bestDelta = delta; bestGain = gain; bestScore = score;
                best = {score, c, ri};
                bestConflicts = std::move(conflicts);
              }
            }
          }
    }
    if (bestDelta <= 0) break;
    std::vector<char> remove(spheres.size(), 0);
    for (unsigned si : bestConflicts) remove[si] = 1;
    std::vector<Sphere> next;
    next.reserve(spheres.size() + 1);
    for (unsigned si = 0; si < spheres.size(); ++si)
      if (!remove[si]) next.push_back(spheres[si]);
    next.push_back({best.c, radii[best.ri]});
    spheres.swap(next);
    sphHash = PointHash();
    sphHash.Init(2.0f * rmax + cfg.sphereContactTol + 0.01f);
    covered = baseCovered;
    coveredW = 0.0;
    mouthCovered.assign(mouthCenters.size(), 0);
    for (unsigned si = 0; si < spheres.size(); ++si) {
      const Sphere &s = spheres[si];
      tgtHash.Query(s.c, s.r + kCoverMargin, [&](unsigned i) {
        if (!covered[i] && Covers(s.c, s.r, i)) {
          covered[i] = 1;
          coveredW += w[i];
        }
      });
      mouthHash.Query(s.c, s.r + kCoverMargin, [&](unsigned i) {
        const float rr = s.r + kCoverMargin;
        if ((mouthCenters[i] - s.c).norm2() <= rr * rr) mouthCovered[i] = 1;
      });
      sphHash.Add(si, s.c);
    }
    ++applied;
    LOGI("sphere cover: repair " << applied << " net +" << bestDelta
         << " mouth centers, replaced " << bestConflicts.size() << " spheres, now "
         << spheres.size() << " spheres in " << clock.ElapsedMS() / 1000.0 << " s\n");
  }
  LOGI("sphere cover: repair applied " << applied << " swaps in "
       << clock.ElapsedMS() / 1000.0 << " s\n");
}

void Coverer::AuditOpenMouths(const char *stage) {
  if (!cfg.sphereAuditOpenMouths) return;
  Utils::Stopwatch clock;
  clock.Start();
  const unsigned oldRejects[] = {rejContainer, rejExclusion, rejFruitOverlap,
                                 rejNoContact, rejSphereOverlap, rejFruitTriangle,
                                 rejLowGain, rejNoMouth};
  std::ofstream out(scene.outputFolder + "/sphere_open_mouth_feasibility_" + stage + ".csv");
  out << "x,y,z,feasible,score,radius,cx,cy,cz,legal,container,exclusion,fruit_sdf,sphere_overlap,no_contact,fruit_mesh\n";
  unsigned open = 0, feasible = 0;
  unsigned totals[10] = {0};
  for (unsigned mi = 0; mi < mouthCenters.size(); ++mi) {
    if (mouthCovered[mi]) continue;
    ++open;
    const Vec3f p = mouthCenters[mi];
    Cand best;
    best.score = 0.0f;
    unsigned reasons[10] = {0};
    for (int ix = -5; ix <= 5; ++ix)
      for (int iy = -5; iy <= 5; ++iy)
        for (int iz = -5; iz <= 5; ++iz) {
          const Vec3f c = p + Vec3f(float(ix), float(iy), float(iz));
          const float d2 = (c - p).norm2();
          for (unsigned ri = 0; ri < radii.size(); ++ri) {
            const float rr = radii[ri] + kCoverMargin;
            if (d2 > rr * rr) continue;
            int reason = 0;
            const float s = Eval(c, ri, &reason);
            ++reasons[reason];
            if (s > best.score) best = {s, c, ri};
          }
        }
    if (best.score > 0.0f) ++feasible;
    for (unsigned k = 0; k < 10; ++k) totals[k] += reasons[k];
    out << p[0] << ',' << p[1] << ',' << p[2] << ',' << (best.score > 0.0f) << ','
        << best.score << ',' << (best.score > 0.0f ? radii[best.ri] : 0.0f) << ','
        << best.c[0] << ',' << best.c[1] << ',' << best.c[2] << ','
        << reasons[0] << ',' << reasons[1] << ',' << reasons[2] << ','
        << reasons[3] << ',' << reasons[4] << ',' << reasons[5] << ','
        << reasons[7] << '\n';
  }
  rejContainer = oldRejects[0]; rejExclusion = oldRejects[1];
  rejFruitOverlap = oldRejects[2]; rejNoContact = oldRejects[3];
  rejSphereOverlap = oldRejects[4]; rejFruitTriangle = oldRejects[5];
  rejLowGain = oldRejects[6]; rejNoMouth = oldRejects[7];
  LOGI("sphere cover: " << stage << " open-mouth feasibility 1cm grid " << feasible << '/' << open
       << " have a positive-score legal sphere; candidate reasons legal " << totals[0]
       << " container " << totals[1] << " exclusion " << totals[2]
       << " fruit_sdf " << totals[3] << " sphere_overlap " << totals[4]
       << " no_contact " << totals[5] << " fruit_mesh " << totals[7]
       << " in " << clock.ElapsedMS() / 1000.0 << " s\n");
}

void Coverer::Probe(const Vec3f &p) {
  LOGI("probe (" << p[0] << ", " << p[1] << ", " << p[2] << "):\n");
  float fd;
  Vec3f away(0.0f, 0.0f, 0.0f);
  NearestFruit(p, 18.0f, fd, &away);
  float ed = kInf;
  if (scene.exclusionItemIndex != ~0u && scene.items[scene.exclusionItemIndex].sdf)
    ed = scene.items[scene.exclusionItemIndex].sdf->GetCoarseDist(p);
  LOGI("  fruit dist " << fd << ", container dist " << scene.sdf->GetCoarseDist(p)
                       << ", exclusion dist " << ed << "\n");
  for (const Sphere &s : spheres) {
    const float d = (s.c - p).norm();
    if (d < 12.0f)
      LOGI("  sphere (" << s.c[0] << ", " << s.c[1] << ", " << s.c[2] << ") r " << s.r
                        << " center dist " << d << "\n");
  }
  for (size_t id = 0; id < scene.instances.size(); ++id) {
    const auto &inst = scene.instances[id];
    const auto &item = scene.items[inst.itemId];
    if (inst.isExclusion ||
        (cfg.sphereMaxX != kInf && WorldBoxCenterX(scene, inst) >= cfg.sphereMaxX)) continue;
    if (item.BoxDiagonal() <= cfg.sphereSupportMinDiag) continue;
    if ((inst.tran.position - p).norm() > 14.0f) continue;
    const Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position);
    float vd = kInf;
    for (size_t vi = 0; vi < item.mesh.GetNumVerts(); ++vi)
      vd = std::min(vd, (item.mesh.GetVertex(unsigned(vi)) - local).norm());
    LOGI("  fruit " << item.name << " #" << id << " pos (" << inst.tran.position[0] << ", "
                    << inst.tran.position[1] << ", " << inst.tran.position[2] << ") scale "
                    << inst.tran.scale << " box diag " << item.BoxDiagonal() << " sdf "
                    << item.sdf->GetCoarseDist(local) << " nearest vertex " << vd << '\n');
  }
  unsigned nt = 0, nu = 0;
  tgtHash.Query(p, 8.0f, [&](unsigned i) {
    if ((pts[i] - p).norm() > 8.0f) return;
    ++nt;
    if (!covered[i]) ++nu;
  });
  LOGI("  targets within 8cm: " << nt << ", uncovered " << nu << "\n");
  // all candidate centers within 5cm on a 1cm grid, every radius.
  unsigned reasons[7] = {0};
  unsigned validNoGain = 0;
  struct Best { float score = -kInf; Vec3f c; unsigned ri = 0; } best[3];
  for (int x = -5; x <= 5; ++x)
    for (int y = -5; y <= 5; ++y)
      for (int z = -5; z <= 5; ++z) {
        const Vec3f c = p + Vec3f(float(x), float(y), float(z));
        for (unsigned ri = 0; ri < radii.size(); ++ri) {
          int reason = 0;
          const float s = Eval(c, ri, &reason);
          ++reasons[reason];
          if (reason) continue;
          if (s <= 0.0f) ++validNoGain;
          for (int k = 0; k < 3; ++k)
            if (s > best[k].score) {
              for (int m = 2; m > k; --m) best[m] = best[m - 1];
              best[k].score = s; best[k].c = c; best[k].ri = ri;
              break;
            }
        }
      }
  LOGI("  candidates: ok " << reasons[0] << " (score<=0: " << validNoGain << "), container "
                           << reasons[1] << ", exclusion " << reasons[2] << ", fruit_overlap "
                           << reasons[3] << ", sphere_overlap " << reasons[4] << ", no_contact "
                           << reasons[5] << "\n");
  for (const Best &b : best)
    if (b.score > -kInf)
      LOGI("  best (" << b.c[0] << ", " << b.c[1] << ", " << b.c[2] << ") r " << radii[b.ri]
                      << " score " << b.score << "\n");
}

void Coverer::Save() const {
  const std::string dir = scene.outputFolder;
  std::ofstream out(dir + "/sphere_cover.txt");
  out << spheres.size() << "\n";
  for (const Sphere &s : spheres)
    out << s.c[0] << ' ' << s.c[1] << ' ' << s.c[2] << ' ' << s.r << "\n";
  std::vector<Vec3f> open;
  for (size_t i = 0; i < pts.size(); ++i)
    if (!covered[i]) open.push_back(pts[i]);
  SaveVec3fObj(dir + "/sphere_cover_uncovered.obj", open);
  {
    // all spheres as one low-poly mesh for Blender.
    std::vector<Vec3f> uv;
    std::vector<std::array<unsigned, 3>> uf;
    MakeIcosphere(2, uv, uf);
    TrigMesh m;
    for (const Sphere &sp : spheres) {
      const unsigned vbase = unsigned(m.v.size() / 3);
      for (const Vec3f &u : uv) {
        const Vec3f p = sp.c + sp.r * u;
        m.v.push_back(p[0]);
        m.v.push_back(p[1]);
        m.v.push_back(p[2]);
      }
      for (const auto &f : uf) {
        m.t.push_back(vbase + f[0]);
        m.t.push_back(vbase + f[1]);
        m.t.push_back(vbase + f[2]);
      }
    }
    m.SaveObj(dir + "/sphere_cover_spheres.obj");
  }
  LOGI("sphere cover: saved " << dir << "/sphere_cover.txt, " << open.size()
                              << " uncovered targets\n");
}

} // namespace

// Crevice targets as a lattice of free-space points: the volume closed by the
// shrinkwrap (radius cfg.sphereFieldRadius) minus the fruit themselves, at a
// fine voxel size, kept only where a flood from outside reaches (so fruit
// interiors and sealed pockets are excluded). Only large fixed fruit (and, for local experiments, only
// those with center x < cfg.sphereMaxX) shape the field. The container is only
// approximately obeyed, the exclusion mesh is a hard obstacle, and the inner
// container mesh is ignored.
static bool GenerateFieldTargets(PackingScene &scene, const PackingConfig &cfg,
                          std::vector<Vec3f> &out) {
  Utils::Stopwatch clock;
  clock.Start();
  std::vector<InstanceInfo> original;
  original.swap(scene.instances);
  unsigned used = 0;
  for (const auto &inst : original) {
    if (inst.isExclusion) continue;
    if (scene.items[inst.itemId].BoxDiagonal() <= cfg.sphereSupportMinDiag) continue;
    if (cfg.sphereMaxX != kInf && WorldBoxCenterX(scene, inst) >= cfg.sphereMaxX) continue;
    scene.instances.push_back(inst);
    ++used;
  }
  Array3D<short> closed, raw;
  Vec3f origin;
  float distUnit = 0.01f;
  try {
    if (used > 0)
      ComputeShrinkWrapDistField(scene, cfg.sphereFieldRadius, cfg.sphereFieldVoxel, closed,
                                 origin, distUnit, &raw);
  } catch (...) {
    scene.instances.swap(original);
    throw;
  }
  scene.instances.swap(original);
  if (used == 0 || raw.GetData().empty()) {
    LOGI("sphere cover: no large fruit selected for the field\n");
    return false;
  }
  const float vox = cfg.sphereFieldVoxel;
  const int stride = std::max(1, int(std::lround(cfg.sphereFieldSpacing / vox)));
  const Vec3u sz = raw.GetSize();
  const AdapSDF *excl = nullptr;
  if (scene.exclusionItemIndex != ~0u && scene.items[scene.exclusionItemIndex].sdf)
    excl = scene.items[scene.exclusionItemIndex].sdf.get();
  std::shared_ptr<AdapSDF> inner;
  if (!scene.containerInner.mesh.v.empty())
    inner = ComputeSDF(0.002f, 2.0f, scene.containerInner.mesh);
  // flood from outside through free space (not within a voxel of a fruit
  // surface). fruit interiors and sealed pockets are never reached, so the
  // reached set inside the closed hull is the open crevice volume.
  const size_t nVox = size_t(sz[0]) * sz[1] * sz[2];
  std::vector<unsigned char> reach(nVox, 0);
  auto lin = [&](unsigned i, unsigned j, unsigned k) {
    return (size_t(k) * sz[1] + j) * sz[0] + i;
  };
  const short thr = short(std::ceil(0.87f * vox / distUnit));
  std::vector<size_t> stack;
  auto push = [&](unsigned i, unsigned j, unsigned k) {
    const size_t id = lin(i, j, k);
    if (reach[id] || raw(i, j, k) < thr) return;
    reach[id] = 1;
    stack.push_back(id);
  };
  for (unsigned k = 0; k < sz[2]; ++k)
    for (unsigned j = 0; j < sz[1]; ++j)
      for (unsigned i = 0; i < sz[0]; ++i)
        if (i == 0 || j == 0 || k == 0 || i + 1 == sz[0] || j + 1 == sz[1] || k + 1 == sz[2])
          push(i, j, k);
  while (!stack.empty()) {
    const size_t id = stack.back();
    stack.pop_back();
    const unsigned i = unsigned(id % sz[0]);
    const unsigned j = unsigned((id / sz[0]) % sz[1]);
    const unsigned k = unsigned(id / (size_t(sz[0]) * sz[1]));
    if (i > 0) push(i - 1, j, k);
    if (i + 1 < sz[0]) push(i + 1, j, k);
    if (j > 0) push(i, j - 1, k);
    if (j + 1 < sz[1]) push(i, j + 1, k);
    if (k > 0) push(i, j, k - 1);
    if (k + 1 < sz[2]) push(i, j, k + 1);
  }
  const short minClearS = short(cfg.sphereFieldMinClear / distUnit);
  unsigned inMask = 0, outContainer = 0, inExclusion = 0, inInner = 0, tooThin = 0;
  for (unsigned k = 0; k < sz[2]; k += stride)
    for (unsigned j = 0; j < sz[1]; j += stride)
      for (unsigned i = 0; i < sz[0]; i += stride) {
        if (!reach[lin(i, j, k)] || closed(i, j, k) > 0) continue;
        ++inMask;
        if (raw(i, j, k) < minClearS) { ++tooThin; continue; }
        const Vec3f p = origin + Vec3f((i + 0.5f) * vox, (j + 0.5f) * vox, (k + 0.5f) * vox);
        if (p[0] >= cfg.sphereMaxX) continue;
        if (scene.sdf->GetCoarseDist(p) > 0.5f) { ++outContainer; continue; }
        if (excl && excl->GetCoarseDist(p) < 0.0f) { ++inExclusion; continue; }
        if (inner && inner->GetCoarseDist(p) <= 0.0f) { ++inInner; continue; }
        out.push_back(p);
      }
  LOGI("sphere cover: field targets from " << used << " large fruit, voxel " << vox
       << ", closing " << cfg.sphereFieldRadius << ": " << out.size() << " lattice points ("
       << inMask << " in crevice mask, " << tooThin << " too thin, " << outContainer
       << " outside container, " << inExclusion << " in exclusion, " << inInner
       << " inside inner container), "
       << clock.ElapsedMS() / 1000.0 << " s\n");
  return !out.empty();
}

// Minimal scene for the sphere cover: only the meshes it touches (the medium
// kinds for radius sizing and the pack's fruit near x < sphereMaxX), the
// container SDF, and the exclusion mesh. No background grid, triangle grids,
// or other whole-hand setup.
bool BuildSphereCoverScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg) {
  Utils::Stopwatch clock;
  clock.Start();
  namespace fs = std::filesystem;
  std::vector<std::string> names;
  auto addName = [&](const std::string &n) {
    if (std::find(names.begin(), names.end(), n) == names.end()) names.push_back(n);
  };
  for (const PackingStep &step : plan.steps)
    if (step.kind == StepKind::MediumPatch)
      for (const auto &n : step.names) addName(n);
  // pack fruit near the region. pack positions are in the input-mesh frame,
  // which differs from the inertia frame by a few cm, hence the margin.
  {
    std::ifstream in(cfg.ResumePackPath());
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream iss(line);
      std::string name, label;
      float x, y, z;
      if (!(iss >> name >> label >> x >> y >> z)) continue;
      if (x < cfg.sphereMaxX + 40.0f) addName(name);
    }
  }
  const fs::path dir(cfg.MeshDir());
  for (const std::string &n : names) {
    fs::path p = dir / (n + ".obj");
    if (!fs::exists(p)) p = dir / (n + ".stl");
    MeshInfo info;
    if (LoadMeshInfo(info, p) != 0) continue;
    info.filePath = p.string();
    scene.nameToIndex[info.name] = unsigned(scene.items.size());
    scene.items.push_back(std::move(info));
  }
  if (scene.items.empty()) {
    LOGI("sphere cover: no items loaded from " << dir.string() << "\n");
    return false;
  }
  if (LoadMeshInfo(scene.container, fs::path(cfg.ContainerPath())) != 0) {
    LOGI("sphere cover: could not load container " << cfg.ContainerPath() << "\n");
    return false;
  }
  if (LoadMeshInfo(scene.containerInner, fs::path(cfg.InnerContainerPath())) != 0)
    LOGI("sphere cover: no inner container at " << cfg.InnerContainerPath() << "\n");
  if (!cfg.exclusionMeshFile.empty()) {
    MeshInfo excl;
    if (LoadMeshInfo(excl, fs::path(cfg.ExclusionMeshPath())) == 0) {
      excl.name = "_exclusion";
      scene.exclusionItemIndex = unsigned(scene.items.size());
      scene.items.push_back(std::move(excl));
    }
  }
  scene.dx = cfg.dx;
  scene.gridDx = cfg.gridDx;
  scene.containerSDFDx = cfg.containerSDFDx;
  scene.subgridCellSize = cfg.subgridCellSize;
  scene.outputFolder = cfg.OutputFolder();
  std::error_code ec;
  fs::create_directories(scene.outputFolder, ec);
  scene.ComputeContainerSDF();
  scene.InitRigidBodies();
  for (auto &item : scene.items) {
    item.mesh.ComputeTrigNormals();
    item.mesh.ComputeVertNormals();
  }
  scene.broadPhase.Init(scene.container.box, cfg.broadPhaseDx);
  LOGI("sphere cover: lite scene with " << scene.items.size() << " meshes, " << clock.ElapsedMS() / 1000.0
                                         << " s\n");
  return true;
}

// registers an instance for distance queries only: no occupancy-grid
// voxelization (scene.Put does that for every instance and the cover never
// reads the background grid).
static unsigned AddInstanceLite(PackingScene &scene, unsigned itemIdx, const RigidTransform &tran,
                                bool exclusion) {
  TrigMesh inst = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
  const Box3f box = ComputeBBox(inst.v);
  const unsigned id = unsigned(scene.instances.size());
  scene.instances.push_back(InstanceInfo(itemIdx, tran));
  scene.instances[id].isExclusion = exclusion;
  scene.broadPhase.Add(box, id);
  return id;
}

std::vector<float> BuildSphereRadii(PackingScene &scene, const PackingStep &step,
                                    const PackingConfig &cfg) {
  return BuildRadii(scene, step, cfg);
}

void RunSphereCoverFast(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg) {
  Utils::Stopwatch clock;
  clock.Start();
  const PackingStep *medium = nullptr;
  for (const PackingStep &step : plan.steps)
    if (step.kind == StepKind::MediumPatch) { medium = &step; break; }
  if (!medium) {
    LOGI("sphere cover: plan has no medium step\n");
    return;
  }
  if (!cfg.shellPasses) ExportContainerShell(scene, cfg);
  if (scene.exclusionItemIndex != ~0u) {
    MeshInfo &excl = scene.items[scene.exclusionItemIndex];
    if (!excl.sdf) excl.ComputeSDFCached();
    AddInstanceLite(scene, scene.exclusionItemIndex, RigidTransform(), true);
  }
  // Only the fixed large fruit whose world-space bounding-box center is left
  // of sphereMaxX. Pack positions can differ from that center for long fruit.
  std::ifstream in(cfg.ResumePackPath());
  std::string line;
  unsigned total = 0, kept = 0;
  std::vector<char> needed(scene.items.size(), 0);
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string name, posLabel, rotLabel, scaleLabel;
    RigidTransform tran;
    if (!(iss >> name >> posLabel)) continue;
    if (!(iss >> tran.position[0] >> tran.position[1] >> tran.position[2])) continue;
    if (!(iss >> rotLabel)) continue;
    Matrix3f &m = tran.rotation;
    if (!(iss >> m(0, 0) >> m(0, 1) >> m(0, 2) >> m(1, 0) >> m(1, 1) >> m(1, 2) >> m(2, 0) >>
          m(2, 1) >> m(2, 2)))
      continue;
    if (!(iss >> scaleLabel >> tran.scale)) continue;
    ++total;
    auto it = scene.nameToIndex.find(name);
    if (it == scene.nameToIndex.end()) continue;
    const unsigned itemIdx = it->second;
    const MeshInfo &item = scene.items[itemIdx];
    if (item.BoxDiagonal() <= cfg.sphereSupportMinDiag) continue;
    tran = item.rb.GetInertiaTran(tran);
    // the shell passes query rays that reach fruit just past the region edge.
    const float loadMaxX = cfg.sphereMaxX + (cfg.shellPasses ? cfg.shellFruitMargin : 0.0f);
    if (cfg.sphereMaxX != kInf &&
        WorldBoxCenterX(scene, InstanceInfo(itemIdx, tran)) >= loadMaxX) continue;
    AddInstanceLite(scene, itemIdx, tran, false);
    needed[itemIdx] = 1;
    ++kept;
  }
  for (unsigned i = 0; i < needed.size(); ++i)
    if (needed[i]) scene.EnsureItemSamples(i);
  LOGI("sphere cover: loaded " << kept << " of " << total << " pack instances, "
                               << clock.ElapsedMS() / 1000.0 << " s\n");
  if (cfg.shellPasses)
    RunShellPasses(scene, *medium, cfg);
  else
    PackSphereCover(scene, *medium, cfg);
  scene.SaveInstances(scene.outputFolder + "/sphere_cover_fruit.txt");
  LOGI("sphere cover: total " << clock.ElapsedMS() / 1000.0 << " s\n");
}

void PackSphereCover(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg) {
  std::vector<Vec3f> targets;
  if (cfg.sphereFieldVoxel > 0.0f) {
    if (!GenerateFieldTargets(scene, cfg, targets)) return;
  } else if (!LoadObjVertices(cfg.MediumTargetsPath(), targets) || targets.empty()) {
    LOGI("sphere cover: no targets at '" << cfg.MediumTargetsPath() << "'\n");
    return;
  }
  if (cfg.sphereMaxX < std::numeric_limits<float>::max()) {
    std::vector<Vec3f> kept;
    for (const Vec3f &t : targets)
      if (t[0] < cfg.sphereMaxX) kept.push_back(t);
    LOGI("sphere cover: region x < " << cfg.sphereMaxX << " keeps " << kept.size() << " of "
                                     << targets.size() << " targets\n");
    targets.swap(kept);
  }
  std::vector<float> radii = BuildRadii(scene, step, cfg);
  if (radii.empty()) {
    LOGI("sphere cover: no medium kinds in step\n");
    return;
  }
  std::ostringstream rs;
  for (float r : radii) rs << r << ' ';
  LOGI("sphere cover: " << targets.size() << " targets, radii " << rs.str() << "\n");
  Coverer cov(scene, cfg, radii);
  if (cfg.sphereFieldVoxel > 0.0f) {
    const size_t before = targets.size();
    targets = cov.KeepClear(targets, cfg.sphereFieldMinClear);
    LOGI("sphere cover: " << targets.size() << " of " << before << " field points are free of fruit (clearance >= " << cfg.sphereFieldMinClear << " cm)\n");
  }
  if (cfg.sphereFieldVoxel > 0.0f)
    SaveVec3fObj(scene.outputFolder + "/sphere_cover_targets.obj", targets);
  if (cfg.sphereHullFaces) ExportCreviceFaces(scene, cfg, targets);
  std::vector<Vec3f> mediumMouths;
  if (cfg.sphereMouths)
    mediumMouths = ExportCreviceMouths(scene, cfg, targets, radii.front());
  cov.SetTargets(std::move(targets), cfg.sphereFieldVoxel > 0.0f);
  cov.SetMouthGuidance(mediumMouths, cfg.mouthTargetWeight);
  cov.AuditOpenMouths("before");
  cov.Run();
  cov.RepairMouths();
  cov.AuditOpenMouths("after");
  if (!mediumMouths.empty()) {
    unsigned hit = 0;
    std::ofstream coveredMouthObj(scene.outputFolder + "/crevice_mouth_medium_covered.obj");
    std::ofstream uncoveredMouthObj(scene.outputFolder + "/crevice_mouth_medium_uncovered.obj");
    for (const Vec3f &m : mediumMouths) {
      bool covered = false;
      for (const Sphere &s : cov.spheres) {
        if ((m - s.c).norm2() <= (s.r + kCoverMargin) * (s.r + kCoverMargin)) {
          covered = true; break;
        }
      }
      if (covered) ++hit;
      std::ostream &out = covered ? coveredMouthObj : uncoveredMouthObj;
      out << "v " << m[0] << ' ' << m[1] << ' ' << m[2] << '\n';
    }
    LOGI("sphere cover: medium-width mouth centers covered " << hit << "/"
         << mediumMouths.size() << "\n");
  }
  for (size_t i = 0; i + 2 < cfg.sphereProbes.size(); i += 3)
    cov.Probe(Vec3f(cfg.sphereProbes[i], cfg.sphereProbes[i + 1], cfg.sphereProbes[i + 2]));
  cov.Save();
}
