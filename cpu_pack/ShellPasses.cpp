#include "ShellPasses.h"

#include "AdapSDF.h"
#include "ContainerShell.h"
#include "Log.h"
#include "MeshOps.h"
#include "QuickHull.hpp"
#include "PointSample.h"
#include "SphereCover.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

const float kInf = std::numeric_limits<float>::max();

struct Ball {
  Vec3f c;
  float r = 0.0f;
  unsigned pass = 1;
};

const unsigned kBallBase = 1000000;

// signed distance to the nearest body (negative inside): fruit instances in the
// scene plus proxy balls. Body ids are instance ids, or kBallBase + ball index.
class FruitField {
public:
  explicit FruitField(const PackingScene &s) : scene(s) {}

  void SetBalls(const std::vector<Ball> *b) { balls = b; }

  float InstDist(unsigned id, const Vec3f &p) const {
    const auto &inst = scene.instances[id];
    const auto &item = scene.items[inst.itemId];
    const float scale = std::max(inst.tran.scale, 0.001f);
    const Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position) / scale;
    return scale * item.sdf->GetCoarseDist(local);
  }

  float BodyDist(unsigned id, const Vec3f &p) const {
    if (id >= kBallBase) {
      const Ball &b = (*balls)[id - kBallBase];
      return (p - b.c).norm() - b.r;
    }
    return InstDist(id, p);
  }

  float Dist(const Vec3f &p, float range, unsigned *bodyId = nullptr) const {
    Box3f q;
    q.vmin = q.vmax = p;
    float best = kInf;
    for (unsigned id : scene.broadPhase.GetNearby(q, range)) {
      const auto &inst = scene.instances[id];
      if (inst.isExclusion || !scene.items[inst.itemId].sdf) continue;
      const float d = InstDist(id, p);
      if (d < best) {
        best = d;
        if (bodyId) *bodyId = id;
      }
    }
    if (balls)
      for (unsigned i = 0; i < balls->size(); ++i) {
        const float d = (p - (*balls)[i].c).norm() - (*balls)[i].r;
        if (d < best) {
          best = d;
          if (bodyId) *bodyId = kBallBase + i;
        }
      }
    return best;
  }

  // distance along dir to the first body surface; returns cap when none within cap.
  float Ray(const Vec3f &p, const Vec3f &dir, float cap, unsigned *bodyId = nullptr) const {
    float s = 0.0f;
    while (s < cap) {
      unsigned id = 0;
      const float d = Dist(p + s * dir, 4.0f, &id);
      if (d <= 0.05f) {
        if (bodyId) *bodyId = id;
        return s;
      }
      s += std::max(0.1f, std::min(d, 4.0f));
    }
    return cap;
  }

private:
  const PackingScene &scene;
  const std::vector<Ball> *balls = nullptr;
};

// one ray sample on a shell triangle.
struct Sample {
  Vec3f p, n;
  unsigned tri = 0;
  float hit = 0.0f; // ray length to the first fruit, capped
  unsigned body = 0; // id of the body hit (see FruitField)
  bool open = false; // no fruit within the cap
};

// samples every shell triangle on a roughly uniform lattice of the given spacing.
std::vector<Sample> SampleShell(const TrigMesh &shell, float spacing) {
  std::vector<Sample> out;
  auto vert = [&](unsigned i) { return Vec3f(shell.v[3 * i], shell.v[3 * i + 1], shell.v[3 * i + 2]); };
  for (size_t t = 0; t < shell.t.size() / 3; ++t) {
    const Vec3f a = vert(shell.t[3 * t]), b = vert(shell.t[3 * t + 1]), c = vert(shell.t[3 * t + 2]);
    Vec3f n = (b - a).cross(c - a);
    if (n.norm2() < 1e-12f) continue;
    n.normalize();
    const float longest = std::sqrt(std::max({(b - a).norm2(), (c - b).norm2(), (a - c).norm2()}));
    const int div = std::clamp(int(std::ceil(longest / spacing)), 1, 64);
    for (int i = 0; i < div; ++i)
      for (int j = 0; i + j < div; ++j) {
        // centers of the sub-triangles that point the same way as the parent.
        const float u = (float(i) + 1.0f / 3.0f) / div, v = (float(j) + 1.0f / 3.0f) / div;
        Sample s;
        s.p = a + u * (b - a) + v * (c - a);
        s.n = n;
        s.tri = unsigned(t);
        out.push_back(s);
      }
  }
  return out;
}

// A blob is a connected group of flagged samples. Two samples link when they
// share a patch and lie within linkDist, so blobs grow across triangle borders
// inside a patch but not across creases (patches bound the normal change).
struct Blob {
  std::vector<unsigned> samples;
  Vec3f center;
  Vec3f normal;
  float area = 0.0f;
};

std::vector<Blob> FindBlobs(const std::vector<Sample> &samples, const std::vector<char> &flag,
                            const std::vector<ShellPatch> &patches, float spacing) {
  std::vector<unsigned> parent(samples.size());
  for (unsigned i = 0; i < parent.size(); ++i) parent[i] = i;
  auto find = [&](unsigned x) {
    while (parent[x] != x) x = parent[x] = parent[parent[x]];
    return x;
  };
  std::unordered_map<unsigned, std::vector<unsigned>> byTri;
  for (unsigned i = 0; i < samples.size(); ++i)
    if (flag[i]) byTri[samples[i].tri].push_back(i);
  const float link2 = (1.6f * spacing) * (1.6f * spacing);
  for (const ShellPatch &p : patches) {
    std::vector<unsigned> ids;
    for (unsigned t : p.trigs) {
      auto it = byTri.find(t);
      if (it != byTri.end()) ids.insert(ids.end(), it->second.begin(), it->second.end());
    }
    for (size_t a = 0; a < ids.size(); ++a)
      for (size_t b = a + 1; b < ids.size(); ++b)
        if ((samples[ids[a]].p - samples[ids[b]].p).norm2() <= link2) parent[find(ids[a])] = find(ids[b]);
  }
  std::unordered_map<unsigned, Blob> comps;
  for (unsigned i = 0; i < samples.size(); ++i)
    if (flag[i]) comps[find(i)].samples.push_back(i);
  std::vector<Blob> out;
  for (auto &kv : comps) {
    Blob &b = kv.second;
    Vec3f c(0, 0, 0), n(0, 0, 0);
    for (unsigned i : b.samples) {
      c = c + samples[i].p;
      n = n + samples[i].n;
    }
    b.center = c * (1.0f / float(b.samples.size()));
    if (n.norm2() > 1e-12f) n.normalize();
    b.normal = n;
    b.area = float(b.samples.size()) * spacing * spacing;
    out.push_back(std::move(b));
  }
  std::sort(out.begin(), out.end(), [](const Blob &a, const Blob &b) { return a.area > b.area; });
  return out;
}

void SavePoints(const std::string &path, const std::vector<Vec3f> &pts) { SaveVec3fObj(path, pts); }

// proxy-sphere legality against the container, exclusion mesh, fruit and the
// spheres placed so far.
class BallWorld {
public:
  BallWorld(const PackingScene &s, const FruitField &f, const PackingConfig &c, const AdapSDF *e)
      : scene(s), fruit(f), cfg(c), excl(e) {}

  // contact: touches fruit, another sphere, or (allowWall) the container wall.
  bool Legal(const Vec3f &c, float r, bool allowWall, bool &contact) const {
    const float tol = cfg.sphereContactTol;
    const float wall = scene.sdf->GetCoarseDist(c);
    if (wall > -(1.0f - cfg.sphereHardFrac) * r) { ++rej[0]; return false; }
    if (excl && excl->GetCoarseDist(c) < (1.0f - cfg.sphereHardFrac) * r) { ++rej[1]; return false; }
    contact = allowWall && wall >= -r - tol;
    const float d = fruit.Dist(c, r + tol + 1.0f);
    if (d < r - cfg.sphereOverlapCm) { ++rej[2]; return false; }
    if (d - r <= tol) contact = true;
    for (const Ball &o : balls) {
      const float gap = (o.c - c).norm() - r - o.r;
      if (-gap > cfg.sphereOverlapCm) { ++rej[3]; return false; }
      if (gap <= tol) contact = true;
    }
    return true;
  }

  std::vector<Ball> balls;
  // rejection tally: wall, exclusion, fruit overlap, ball overlap, no contact, low plug.
  mutable unsigned rej[6] = {0, 0, 0, 0, 0, 0};

private:
  const PackingScene &scene;
  const FruitField &fruit;
  const PackingConfig &cfg;
  const AdapSDF *excl;
};

// near-uniform directions on the unit sphere.
std::vector<Vec3f> FibonacciDirs(unsigned n) {
  std::vector<Vec3f> d;
  const float golden = 3.14159265f * (3.0f - std::sqrt(5.0f));
  for (unsigned i = 0; i < n; ++i) {
    const float z = 1.0f - 2.0f * (float(i) + 0.5f) / float(n);
    const float rr = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float a = golden * float(i);
    d.push_back(Vec3f(rr * std::cos(a), rr * std::sin(a), z));
  }
  return d;
}

// Depth of a point on a body surface inside the convex hull of the touching
// group (the body, plus its nearest neighbors within hullGap). A convex fruit
// alone gives about 0; a seam between two or three fruit gives the depth of the
// pocket under the hull that would bridge them.
class HullDepth {
public:
  HullDepth(const PackingScene &scene, const FruitField &fruit, const std::vector<Ball> &balls,
            const PackingConfig &c)
      : field(fruit), cfg(c) {
    for (unsigned id = 0; id < scene.instances.size(); ++id) {
      const auto &inst = scene.instances[id];
      if (inst.isExclusion || !scene.items[inst.itemId].sdf) continue;
      const TrigMesh &m = scene.items[inst.itemId].mesh;
      const size_t nv = m.GetNumVerts();
      const size_t stride = std::max<size_t>(1, nv / std::max(1u, cfg.hullSamples));
      std::vector<Vec3f> pts;
      for (size_t i = 0; i < nv; i += stride)
        pts.push_back(inst.tran.rotation * (inst.tran.scale * m.GetVertex(unsigned(i))) + inst.tran.position);
      Add(id, std::move(pts));
    }
    for (unsigned i = 0; i < balls.size(); ++i) {
      std::vector<Vec3f> pts;
      for (const Vec3f &d : FibonacciDirs(120)) pts.push_back(balls[i].c + balls[i].r * d);
      Add(kBallBase + i, std::move(pts));
    }
    nbr.resize(ids.size());
    for (size_t a = 0; a < ids.size(); ++a)
      for (size_t b = a + 1; b < ids.size(); ++b) {
        float gap = kInf;
        for (const Vec3f &p : samples[a]) gap = std::min(gap, field.BodyDist(ids[b], p));
        for (const Vec3f &p : samples[b]) gap = std::min(gap, field.BodyDist(ids[a], p));
        if (gap <= cfg.hullGap) {
          nbr[a].push_back(unsigned(b));
          nbr[b].push_back(unsigned(a));
        }
      }
  }

  float Depth(const Vec3f &q, unsigned bodyId) {
    auto it = index.find(bodyId);
    if (it == index.end()) return 0.0f;
    const unsigned a = it->second;
    std::vector<std::pair<float, unsigned>> cand;
    for (unsigned b : nbr[a]) {
      const float d = field.BodyDist(ids[b], q);
      if (d <= cfg.hullSupportDist) cand.push_back({d, b});
    }
    std::sort(cand.begin(), cand.end());
    std::vector<unsigned> group = {a};
    for (size_t i = 0; i < cand.size() && group.size() < cfg.crevGroupMax; ++i) group.push_back(cand[i].second);
    std::sort(group.begin(), group.end());
    const std::vector<Plane> &planes = Planes(group);
    float depth = kInf;
    for (const Plane &pl : planes) depth = std::min(depth, pl.d - pl.n.dot(q));
    return planes.empty() ? 0.0f : std::max(0.0f, depth);
  }

  unsigned numBodies() const { return unsigned(ids.size()); }
  unsigned numEdges() const {
    unsigned e = 0;
    for (const auto &n : nbr) e += unsigned(n.size());
    return e / 2;
  }

private:
  struct Plane {
    Vec3f n;
    float d;
  };

  void Add(unsigned id, std::vector<Vec3f> pts) {
    index[id] = unsigned(ids.size());
    ids.push_back(id);
    samples.push_back(std::move(pts));
  }

  const std::vector<Plane> &Planes(const std::vector<unsigned> &group) {
    auto it = cache.find(group);
    if (it != cache.end()) return it->second;
    std::vector<quickhull::Vector3<float>> cloud;
    Vec3f centroid(0, 0, 0);
    for (unsigned g : group)
      for (const Vec3f &p : samples[g]) {
        cloud.emplace_back(p[0], p[1], p[2]);
        centroid = centroid + p;
      }
    centroid = centroid * (1.0f / float(cloud.size()));
    std::vector<Plane> planes;
    if (cloud.size() >= 4) {
      quickhull::QuickHull<float> qh;
      auto hull = qh.getConvexHull(cloud, true, true);
      const auto &idx = hull.getIndexBuffer();
      for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const auto &c0 = cloud[idx[t]], &c1 = cloud[idx[t + 1]], &c2 = cloud[idx[t + 2]];
        const Vec3f v0(c0.x, c0.y, c0.z), v1(c1.x, c1.y, c1.z), v2(c2.x, c2.y, c2.z);
        Vec3f n = (v1 - v0).cross(v2 - v0);
        if (n.norm2() < 1e-12f) continue;
        n.normalize();
        if (n.dot(v0 - centroid) < 0.0f) n = -1.0f * n;
        planes.push_back({n, n.dot(v0)});
      }
    }
    return cache.emplace(group, std::move(planes)).first->second;
  }

  const FruitField &field;
  const PackingConfig &cfg;
  std::vector<unsigned> ids;
  std::vector<std::vector<Vec3f>> samples;
  std::vector<std::vector<unsigned>> nbr;
  std::map<unsigned, unsigned> index;
  std::map<std::vector<unsigned>, std::vector<Plane>> cache;
};

// true when the ray from s along -n, of length len, passes through the sphere.
bool RayHitsBall(const Sample &s, float len, const Vec3f &c, float r) {
  const Vec3f dir = -1.0f * s.n;
  float t = (c - s.p).dot(dir);
  t = std::max(0.0f, std::min(len, t));
  return (s.p + t * dir - c).norm2() <= r * r;
}

// Pass 1: plug each open-hole blob with spheres grown like a vine. The first
// sphere of a blob sits flush under the shell on the wall; later ones touch
// an earlier sphere (or the wall, fruit). Returns the number of spheres.
unsigned PlugBlobs(unsigned pass, const std::vector<Blob> &blobs, std::vector<Sample> &samples,
                   std::vector<char> &open, const std::vector<float> &radii, BallWorld &world,
                   const FruitField &fruit, const PackingScene &scene, const PackingConfig &cfg) {
  const std::vector<Vec3f> dirs = FibonacciDirs(48);
  const float spacing2 = cfg.shellSampleSpacing * cfg.shellSampleSpacing;
  unsigned placed = 0;
  // pass 1 plugs the whole open ray; pass 2 plugs the ray down to the fruit.
  auto RayLen = [&](const Sample &s) { return pass == 1 ? cfg.fillMaxDiag : s.hit; };
  for (const Blob &blob : blobs) {
    if (blob.area < (pass == 1 ? cfg.holeMinArea : cfg.crevMinArea)) continue;
    std::vector<unsigned> ids = blob.samples;
    // sweep order: min x, then y, then z.
    std::sort(ids.begin(), ids.end(), [&](unsigned a, unsigned b) {
      const Vec3f &pa = samples[a].p, &pb = samples[b].p;
      if (pa[0] != pb[0]) return pa[0] < pb[0];
      if (pa[1] != pb[1]) return pa[1] < pb[1];
      return pa[2] < pb[2];
    });
    const size_t firstBall = world.balls.size();
    for (unsigned iter = 0; iter < cfg.sphereMaxCount; ++iter) {
      unsigned bestPlugged = 0;
      Vec3f bestC;
      float bestR = 0.0f;
      auto consider = [&](const Vec3f &c, float r, bool allowWall) {
        bool contact = false;
        if (!world.Legal(c, r, allowWall, contact)) return;
        if (!contact) { ++world.rej[4]; return; }
        unsigned plugged = 0;
        for (unsigned i : ids)
          if (open[i] && RayHitsBall(samples[i], RayLen(samples[i]), c, r)) ++plugged;
        // a sphere must plug at least a quarter of its own footprint.
        if (float(plugged) * spacing2 < 0.25f * 3.14159265f * r * r) { ++world.rej[5]; return; }
        if (plugged > bestPlugged || (plugged == bestPlugged && r > bestR)) {
          bestPlugged = plugged;
          bestC = c;
          bestR = r;
        }
      };
      // seeds: flush under the shell, sliding inward until tangent to the wall.
      const size_t stride = std::max<size_t>(1, ids.size() / 60);
      for (size_t k = 0; k < ids.size(); k += stride) {
        const unsigned i = ids[k];
        if (!open[i]) continue;
        // lateral offsets in the shell plane: the hole is rarely centered on the ray.
        const Vec3f n = samples[i].n;
        Vec3f u = n.cross(std::fabs(n[0]) < 0.9f ? Vec3f(1, 0, 0) : Vec3f(0, 1, 0));
        u.normalize();
        const Vec3f v = n.cross(u);
        for (size_t ri = radii.size(); ri-- > 0;) {
          const float r = radii[ri];
          for (int a = -2; a <= 2; ++a)
            for (int b = -2; b <= 2; ++b) {
              const Vec3f base = samples[i].p + (0.5f * r * float(a)) * u + (0.5f * r * float(b)) * v;
              float s = 0.0f;
              if (pass == 1) {
                // tangent to the wall.
                while (s < 40.0f && scene.sdf->GetCoarseDist(base - s * n) > -r) s += 0.25f;
              } else {
                // tangent to the first fruit, which the ray hits at samples[i].hit.
                const float stop = std::max(0.0f, samples[i].hit - 0.5f * r);
                while (s < stop && fruit.Dist(base - s * n, r + 2.0f) > r + 0.05f) s += 0.25f;
              }
              if (s >= 40.0f) continue;
              consider(base - s * n, r, true);
            }
        }
      }
      // vine: tangent to any sphere already placed for this blob.
      for (size_t bi = firstBall; bi < world.balls.size(); ++bi) {
        const Ball S = world.balls[bi];
        for (float r : radii)
          for (const Vec3f &d : dirs) consider(S.c + (S.r + r) * d, r, true);
      }
      if (bestPlugged == 0) break;
      world.balls.push_back({bestC, bestR, pass});
      LOGI("shell passes: pass " << pass << " sphere r " << bestR << " at " << bestC[0] << " "
                                 << bestC[1] << " " << bestC[2] << " plugs " << bestPlugged
                                 << " samples\n");
      for (unsigned i : ids)
        if (open[i] && RayHitsBall(samples[i], RayLen(samples[i]), bestC, bestR)) open[i] = 0;
      ++placed;
    }
  }
  return placed;
}

// Swap proxy balls for real fruit. Each ball takes the medium kind whose
// bounding sphere radius (bbox center to farthest vertex) is closest to the
// ball radius, aligned bbox center to ball center, tries several orientations,
// then nudges the best ones toward the container interior with the same rigid
// body Nudge the FFT packing uses until one settles without overlap.
struct KindInfo {
  unsigned item = 0;
  Vec3f boxCenter;
  float outer = 0.0f;
};

std::vector<KindInfo> BuildKinds(PackingScene &scene, const PackingStep &step) {
  std::vector<KindInfo> kinds;
  for (const auto &name : step.names) {
    auto it = scene.nameToIndex.find(name);
    if (it == scene.nameToIndex.end()) continue;
    KindInfo k;
    k.item = it->second;
    const TrigMesh &m = scene.items[k.item].mesh;
    const Box3f box = ComputeBBox(m.v);
    k.boxCenter = (box.vmin + box.vmax) * 0.5f;
    for (size_t i = 0; i < m.GetNumVerts(); ++i)
      k.outer = std::max(k.outer, (m.GetVertex(unsigned(i)) - k.boxCenter).norm());
    kinds.push_back(k);
  }
  return kinds;
}

// penetration cost of a fruit pose against fruit, container wall and exclusion
// mesh. maxPen is the deepest penetration (cm) over the fruit's surface samples.
float PoseCost(const PackingScene &scene, const FruitField &fruit, const AdapSDF *excl,
               const MeshInfo &item, const RigidTransform &tran, float &maxPen,
               float *minGap = nullptr) {
  float cost = 0.0f;
  maxPen = 0.0f;
  if (minGap) *minGap = kInf;
  for (const SamplePoint &sp : item.samples) {
    const Vec3f w = tran.rotation * sp.x + tran.position;
    float pen = 0.0f;
    const float d = fruit.Dist(w, 3.0f);
    if (d < 0.0f) pen = std::max(pen, -d);
    if (minGap) *minGap = std::min(*minGap, d);
    const float wall = scene.sdf->GetCoarseDist(w);
    if (wall > 0.0f) pen = std::max(pen, wall);
    if (excl) {
      const float e = excl->GetCoarseDist(w);
      if (e < 0.0f) pen = std::max(pen, -e);
    }
    cost += pen;
    maxPen = std::max(maxPen, pen);
  }
  return cost;
}

unsigned SwapBallsForFruit(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg,
                           FruitField &fruit, const AdapSDF *excl, const std::vector<Ball> &balls) {
  std::vector<KindInfo> kinds = BuildKinds(scene, step);
  if (kinds.empty()) {
    LOGI("shell swap: no medium kinds\n");
    return 0;
  }
  scene.InitContainerGrids();
  fruit.SetBalls(nullptr); // only real fruit count from here on.
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> ang(0.0f, 6.2831853f);
  unsigned placed = 0, rejected = 0;
  for (size_t bi = 0; bi < balls.size(); ++bi) {
    const Ball &b = balls[bi];
    const KindInfo *kind = &kinds[0];
    for (const KindInfo &k : kinds)
      if (std::fabs(k.outer - b.r) < std::fabs(kind->outer - b.r)) kind = &k;
    const unsigned itemIdx = kind->item;
    scene.EnsureItemSamples(itemIdx);
    const MeshInfo &item = scene.items[itemIdx];

    struct Pose { float cost; RigidTransform tran; };
    std::vector<Pose> poses;
    for (unsigned t = 0; t < cfg.swapTrials; ++t) {
      RigidTransform tran;
      tran.rotation = t == 0 ? Matrix3f::identity()
                             : RotationMatrixRad(ang(rng), ang(rng), ang(rng));
      tran.position = b.c - tran.rotation * kind->boxCenter;
      float maxPen;
      poses.push_back({PoseCost(scene, fruit, excl, item, tran, maxPen), tran});
    }
    std::sort(poses.begin(), poses.end(), [](const Pose &a, const Pose &c) { return a.cost < c.cost; });
    bool done = false;
    for (size_t pi = 0; pi < std::min<size_t>(poses.size(), 3) && !done; ++pi) {
      const RigidTransform &start = poses[pi].tran;
      // inward: against the container SDF gradient, no gravity bias.
      const Vec3f pushDir = scene.ForceDirection(itemIdx, Vec3f(0, 0, 0), 0.0f, -1.0f, start);
      std::vector<RigidTransform> trajectory;
      const RigidTransform settled = scene.Nudge(itemIdx, start, pushDir, cfg.swapForceW, trajectory);
      float maxPen, gap;
      PoseCost(scene, fruit, excl, item, settled, maxPen, &gap);
      if (maxPen > cfg.swapMaxOverlap) continue;
      const TrigMesh inst = MakeTransformedMesh(item.mesh, settled);
      const unsigned id = unsigned(scene.instances.size());
      scene.instances.push_back(InstanceInfo(itemIdx, settled));
      scene.instances[id].trajectory = trajectory;
      scene.broadPhase.Add(ComputeBBox(inst.v), id);
      ++placed;
      done = true;
      LOGI("shell swap: ball " << bi << " r " << b.r << " -> " << item.name << " (bound r " << kind->outer
                               << "), moved " << (settled.position - start.position).norm()
                               << " cm, max penetration " << maxPen << " cm, gap to nearest fruit " << gap
                               << " cm\n");
    }
    if (!done) {
      ++rejected;
      LOGI("shell swap: ball " << bi << " r " << b.r << " at " << b.c[0] << " " << b.c[1] << " " << b.c[2]
                               << " no pose within " << cfg.swapMaxOverlap << " cm overlap (" << item.name << ")\n");
    }
  }
  LOGI("shell swap: " << placed << " fruit placed, " << rejected << " balls rejected\n");
  return placed;
}

// Same ray metrics for any fruit set: how many shell rays still see open holes
// and deep hull-relative crevices. Re-casts every sample against the fruit
// currently in the scene (no balls).
void EvalRays(const char *label, const PackingScene &scene, FruitField &fruit,
              std::vector<Sample> samples, const PackingConfig &cfg) {
  fruit.SetBalls(nullptr);
  unsigned numFruit = 0;
  for (const auto &inst : scene.instances)
    if (!inst.isExclusion && scene.items[inst.itemId].sdf) ++numFruit;
  const std::vector<Ball> noBalls;
  HullDepth hull(scene, fruit, noBalls, cfg);
  unsigned open = 0, hits = 0, crev = 0, deep6 = 0;
  for (Sample &s : samples) {
    s.hit = fruit.Ray(s.p, -1.0f * s.n, cfg.fillMaxDiag, &s.body);
    if (s.hit >= cfg.fillMaxDiag) {
      ++open;
      continue;
    }
    ++hits;
    const float d = hull.Depth(s.p - s.hit * s.n, s.body);
    if (d > cfg.crevDepth) ++crev;
    if (d > 6.0f) ++deep6;
  }
  LOGI("shell eval [" << label << "]: " << numFruit << " fruit loaded, " << samples.size()
                      << " rays, open " << open << ", crevice (depth > " << cfg.crevDepth << ") " << crev
                      << ", depth > 6: " << deep6 << "\n");
}

} // namespace

void RunShellPasses(PackingScene &scene, const PackingStep &medium, const PackingConfig &cfg) {
  Utils::Stopwatch clock;
  clock.Start();
  std::vector<ShellPatch> patches;
  TrigMesh shell = ComputeContainerPatches(scene, cfg, patches);
  if (shell.t.empty()) {
    LOGI("shell passes: no shell\n");
    return;
  }
  shell.SaveObj(scene.outputFolder + "/container_shell.obj");
  SavePatchesObj(scene.outputFolder + "/container_patches.obj", shell, patches);

  FruitField fruit(scene);
  const AdapSDF *excl = nullptr;
  if (scene.exclusionItemIndex != ~0u && scene.items[scene.exclusionItemIndex].sdf)
    excl = scene.items[scene.exclusionItemIndex].sdf.get();

  // Rays: from each shell sample inward, only inside the debug region and away
  // from the exclusion mesh.
  std::vector<Sample> all = SampleShell(shell, cfg.shellSampleSpacing);
  std::vector<Sample> samples;
  for (Sample &s : all) {
    if (s.p[0] >= cfg.sphereMaxX) continue;
    if (excl && excl->GetCoarseDist(s.p) < cfg.hullExclMargin) continue;
    s.hit = fruit.Ray(s.p, -1.0f * s.n, cfg.fillMaxDiag, &s.body);
    s.open = s.hit >= cfg.fillMaxDiag;
    samples.push_back(s);
  }
  if (cfg.shellEvalOnly) {
    EvalRays("loaded pack", scene, fruit, samples, cfg);
    return;
  }
  unsigned numOpen = 0;
  std::vector<Vec3f> openPts, hitPts;
  for (const Sample &s : samples) {
    if (s.open) {
      ++numOpen;
      openPts.push_back(s.p);
    } else {
      hitPts.push_back(s.p - s.hit * s.n);
    }
  }
  LOGI("shell passes: " << all.size() << " shell samples, " << samples.size() << " in region, "
                        << numOpen << " open (no fruit within " << cfg.fillMaxDiag << " cm), "
                        << clock.ElapsedMS() / 1000.0 << " s\n");
  std::vector<char> flag(samples.size(), 0);
  for (unsigned i = 0; i < samples.size(); ++i) flag[i] = samples[i].open;
  std::vector<Blob> blobs = FindBlobs(samples, flag, patches, cfg.shellSampleSpacing);
  std::vector<Vec3f> blobPts, smallPts;
  unsigned keptBlobs = 0;
  for (const Blob &b : blobs) {
    const bool keep = b.area >= cfg.holeMinArea;
    if (keep) ++keptBlobs;
    for (unsigned i : b.samples) (keep ? blobPts : smallPts).push_back(samples[i].p);
  }
  LOGI("shell passes: pass 1 holes: " << blobs.size() << " blobs, " << keptBlobs << " with area >= "
                                      << cfg.holeMinArea << " cm2 (largest "
                                      << (blobs.empty() ? 0.0f : blobs.front().area) << ")\n");
  std::vector<float> radii = BuildSphereRadii(scene, medium, cfg);
  // medium spheres only: the 3 cm class is left for a later pass.
  radii.erase(std::remove_if(radii.begin(), radii.end(),
                             [&](float r) { return 2.0f * r < cfg.fillMinDiag; }),
              radii.end());
  if (radii.empty()) {
    LOGI("shell passes: no medium radii\n");
    return;
  }
  {
    std::ostringstream rs;
    for (float r : radii) rs << r << ' ';
    LOGI("shell passes: sphere radii (diameter >= " << cfg.fillMinDiag << "): " << rs.str() << "\n");
  }
  BallWorld world(scene, fruit, cfg, excl);
  const unsigned numPass1 = PlugBlobs(1, blobs, samples, flag, radii, world, fruit, scene, cfg);
  unsigned stillOpen = 0;
  for (char f : flag) stillOpen += f ? 1 : 0;
  LOGI("shell passes: pass 1 rejections wall " << world.rej[0] << " exclusion " << world.rej[1]
                                               << " fruit " << world.rej[2] << " ball " << world.rej[3]
                                               << " no contact " << world.rej[4] << " low plug "
                                               << world.rej[5] << "\n");
  LOGI("shell passes: pass 1 placed " << numPass1 << " spheres, open samples " << numOpen << " -> "
                                      << stillOpen << ", " << clock.ElapsedMS() / 1000.0 << " s\n");
  {
    std::ofstream out(scene.outputFolder + "/shell_pass1_spheres.txt");
    out << world.balls.size() << "\n";
    for (const Ball &b : world.balls) out << b.c[0] << ' ' << b.c[1] << ' ' << b.c[2] << ' ' << b.r << "\n";
  }
  // Pass 2: pass 1 spheres now count as existing pack. Re-cast every ray
  // against fruit plus balls, then flag hits that lie deep inside the hull of
  // their touching group.
  const size_t numBallsPass1 = world.balls.size();
  fruit.SetBalls(&world.balls);
  for (Sample &s : samples) {
    s.hit = fruit.Ray(s.p, -1.0f * s.n, cfg.fillMaxDiag, &s.body);
    s.open = s.hit >= cfg.fillMaxDiag;
  }
  HullDepth hull(scene, fruit, world.balls, cfg);
  LOGI("shell passes: hull groups from " << hull.numBodies() << " bodies, " << hull.numEdges()
                                         << " neighbor edges\n");
  std::vector<char> crev(samples.size(), 0);
  std::vector<float> depths(samples.size(), 0.0f);
  unsigned numHit = 0, numCrev = 0, deep3 = 0, deep6 = 0;
  for (unsigned i = 0; i < samples.size(); ++i) {
    const Sample &s = samples[i];
    if (s.open) continue;
    ++numHit;
    depths[i] = hull.Depth(s.p - s.hit * s.n, s.body);
    if (depths[i] > cfg.crevDepth) {
      crev[i] = 1;
      ++numCrev;
    }
    if (depths[i] > 3.0f) ++deep3;
    if (depths[i] > 6.0f) ++deep6;
  }
  LOGI("shell passes: " << numHit << " ray hits, hull depth > " << cfg.crevDepth << ": " << numCrev
                        << ", > 3: " << deep3 << ", > 6: " << deep6 << ", "
                        << clock.ElapsedMS() / 1000.0 << " s\n");
  std::vector<Blob> crevBlobs = FindBlobs(samples, crev, patches, cfg.shellSampleSpacing);
  unsigned keptCrev = 0;
  std::vector<Vec3f> crevPts, crevSmall;
  for (const Blob &b : crevBlobs) {
    const bool keep = b.area >= cfg.crevMinArea;
    if (keep) ++keptCrev;
    for (unsigned i : b.samples) (keep ? crevPts : crevSmall).push_back(samples[i].p - samples[i].hit * samples[i].n);
  }
  LOGI("shell passes: pass 2 crevices: " << crevBlobs.size() << " blobs, " << keptCrev
                                         << " with area >= " << cfg.crevMinArea << " cm2 (largest "
                                         << (crevBlobs.empty() ? 0.0f : crevBlobs.front().area) << ")\n");
  SavePoints(scene.outputFolder + "/shell_crevice_blobs.obj", crevPts);
  SavePoints(scene.outputFolder + "/shell_crevice_small.obj", crevSmall);
  const unsigned numPass2 = PlugBlobs(2, crevBlobs, samples, crev, radii, world, fruit, scene, cfg);
  unsigned stillCrev = 0;
  for (char f : crev) stillCrev += f ? 1 : 0;
  LOGI("shell passes: pass 2 placed " << numPass2 << " spheres, crevice samples " << numCrev << " -> "
                                      << stillCrev << ", " << clock.ElapsedMS() / 1000.0 << " s\n");
  {
    std::ofstream out(scene.outputFolder + "/shell_spheres_all.txt");
    out << world.balls.size() << "\n";
    for (const Ball &b : world.balls) out << b.c[0] << ' ' << b.c[1] << ' ' << b.c[2] << ' ' << b.r << "\n";
    std::ofstream out2(scene.outputFolder + "/shell_pass2_spheres.txt");
    out2 << world.balls.size() - numBallsPass1 << "\n";
    for (size_t i = numBallsPass1; i < world.balls.size(); ++i)
      out2 << world.balls[i].c[0] << ' ' << world.balls[i].c[1] << ' ' << world.balls[i].c[2] << ' '
           << world.balls[i].r << "\n";
  }
  std::vector<Vec3f> crevLeft;
  for (unsigned i = 0; i < samples.size(); ++i)
    if (crev[i]) crevLeft.push_back(samples[i].p - samples[i].hit * samples[i].n);
  SavePoints(scene.outputFolder + "/shell_crevice_remaining.obj", crevLeft);
  SavePoints(scene.outputFolder + "/shell_hole_blobs.obj", blobPts);
  if (cfg.shellSwap) {
    EvalRays("large fruit only", scene, fruit, samples, cfg);
    unsigned n = SwapBallsForFruit(scene, medium, cfg, fruit, excl, world.balls);
    EvalRays("after shell passes + swap + nudge", scene, fruit, samples, cfg);
    LOGI("shell passes: swapped " << n << " of " << world.balls.size() << " balls, "
                                  << clock.ElapsedMS() / 1000.0 << " s\n");
  }
  SavePoints(scene.outputFolder + "/shell_hole_small.obj", smallPts);
  SavePoints(scene.outputFolder + "/shell_open_samples.obj", openPts);
  SavePoints(scene.outputFolder + "/shell_ray_hits.obj", hitPts);
}
