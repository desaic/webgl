#include "CavityRefine.h"

#include "AdapSDF.h"
#include "CavityField.h"
#include "FloodOutside.h"
#include "cpu_voxelizer.h"
#include "HullNudge.h"
#include "Log.h"
#include "MediumGraph.h"
#include "MeshOps.h"
#include "PackingDriver.h"
#include "Stopwatch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <queue>
#include <random>

namespace {

struct Tile {
  Box3f core;
};

struct Ctx {
  PackingScene &scene;
  const PackingConfig &cfg;
  CavityParams cp;
  std::vector<unsigned> kinds;       // item indices of the medium group
  std::vector<char> isMedium;        // per item index
  std::unique_ptr<HullLibrary> hulls;
  std::mt19937 rng{12345};
  float maxX;
  unsigned nTrial = 0, nOverlap = 0, nPen = 0, nOk = 0;
  Ctx(PackingScene &s, const PackingConfig &c) : scene(s), cfg(c) {}
};

std::vector<Tile> MakeTiles(Ctx &c, float tileSize) {
  Box3f box = ComputeBBox(c.scene.container.mesh.v);
  box.vmax[0] = std::min(box.vmax[0], c.maxX);
  std::vector<Tile> tiles;
  for (float z = box.vmin[2]; z < box.vmax[2]; z += tileSize)
    for (float y = box.vmin[1]; y < box.vmax[1]; y += tileSize)
      for (float x = box.vmin[0]; x < box.vmax[0]; x += tileSize) {
        Tile t;
        t.core.vmin = Vec3f(x, y, z);
        t.core.vmax = Vec3f(std::min(x + tileSize, box.vmax[0]), y + tileSize, z + tileSize);
        // skip tiles with no container inside.
        bool any = false;
        for (int s = 0; s < 27 && !any; s++) {
          const Vec3f p(x + tileSize * 0.5f * (s % 3), y + tileSize * 0.5f * ((s / 3) % 3),
                        z + tileSize * 0.5f * (s / 9));
          any = c.scene.sdf->GetCoarseDist(p) < c.cp.band + tileSize * 0.9f;
        }
        if (any) tiles.push_back(t);
      }
  return tiles;
}

Box3f Grow(const Box3f &b, float m) {
  Box3f o = b;
  o.vmin -= Vec3f(m, m, m);
  o.vmax += Vec3f(m, m, m);
  return o;
}

// cavities of the whole container, tile by tile. totalScore: sum of weighted volumes.
std::vector<CavityComp> AllCavities(Ctx &c, float &totalScore, float &totalVolume, unsigned &deep) {
  const float margin = 12.0f;
  std::vector<CavityComp> all;
  totalScore = 0.0f;
  totalVolume = 0.0f;
  deep = 0;
  const std::vector<Tile> tiles = MakeTiles(c, 40.0f);
  for (const Tile &t : tiles) {
    CavityWindow w;
    BuildCavityWindow(c.scene, Grow(t.core, margin), c.cp, ~0u, w);
    float s = 0.0f;
    std::vector<CavityComp> comps = FindCavities(w, w.solid, c.cp, &t.core, &s, nullptr);
    totalScore += s;
    for (const CavityComp &cc : comps) {
      totalVolume += cc.volume;
      deep += cc.clearance >= 1.5f;
      all.push_back(cc);
    }
  }
  return all;
}

void SaveCavityObj(const std::vector<CavityComp> &cs, const std::string &file) {
  std::ofstream out(file);
  // one tetra-ish marker per cavity, sized by clearance, so Blender can show them.
  size_t nv = 0;
  for (const CavityComp &cc : cs) {
    const float r = std::max(0.3f, cc.clearance);
    const Vec3f p = cc.deepest;
    const Vec3f d[6] = {{r, 0, 0}, {-r, 0, 0}, {0, r, 0}, {0, -r, 0}, {0, 0, r}, {0, 0, -r}};
    for (const Vec3f &o : d) out << "v " << p[0] + o[0] << " " << p[1] + o[1] << " " << p[2] + o[2] << "\n";
    const int f[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
    for (const auto &t : f) out << "f " << nv + t[0] + 1 << " " << nv + t[1] + 1 << " " << nv + t[2] + 1 << "\n";
    nv += 6;
  }
}

Box3f WorldBox(const PackingScene &scene, const InstanceInfo &inst) {
  return ComputeBBox(MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran).v);
}

// deepest penetration (cm) of a fruit pose into the neighbors and the container wall.
// path length (cm) through free voxels from the open air outside the container. a pocket joined to
// the surface by a short opening has a small value even when it is deep; a void behind a long
// tunnel does not.
void GeoDepth(const CavityWindow &w, const Array3D8u &solid, Array3D<float> &G) {
  const Vec3u n = w.size;
  const float INF = 1e9f;
  G.Allocate(n, INF);
  typedef std::pair<float, size_t> Item;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
  const int N0 = int(n[0]), N1 = int(n[1]), N2 = int(n[2]);
  auto free = [&](int i, int j, int k) { return !solid(i, j, k) && !w.wall(i, j, k); };
  for (int k = 0; k < N2; k++)
    for (int j = 0; j < N1; j++)
      for (int i = 0; i < N0; i++) {
        if (!w.wall(i, j, k)) continue;
        G(i, j, k) = 0.0f;
        bool edge = false;
        const int d6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (int q = 0; q < 6 && !edge; q++) {
          const int a = i + d6[q][0], b = j + d6[q][1], c = k + d6[q][2];
          if (a < 0 || b < 0 || c < 0 || a >= N0 || b >= N1 || c >= N2) continue;
          edge = free(a, b, c);
        }
        if (edge) pq.push(Item(0.0f, size_t(i) + size_t(N0) * (size_t(j) + size_t(N1) * size_t(k))));
      }
  while (!pq.empty()) {
    const Item it = pq.top();
    pq.pop();
    const int i = int(it.second % size_t(N0));
    const int j = int((it.second / size_t(N0)) % size_t(N1));
    const int k = int(it.second / (size_t(N0) * size_t(N1)));
    if (it.first > G(i, j, k)) continue;
    for (int dk = -1; dk <= 1; dk++)
      for (int dj = -1; dj <= 1; dj++)
        for (int di = -1; di <= 1; di++) {
          if (!di && !dj && !dk) continue;
          const int a = i + di, b = j + dj, c = k + dk;
          if (a < 0 || b < 0 || c < 0 || a >= N0 || b >= N1 || c >= N2) continue;
          if (!free(a, b, c)) continue;
          const float nd = it.first + w.dx * std::sqrt(float(di * di + dj * dj + dk * dk));
          if (nd < G(a, b, c)) {
            G(a, b, c) = nd;
            pq.push(Item(nd, size_t(a) + size_t(N0) * (size_t(b) + size_t(N1) * size_t(c))));
          }
        }
  }
}

// smallest GeoDepth within r of p (cm); large when p is outside the window.
float GeoNear(const CavityWindow &w, const Array3D<float> &G, const Vec3f &p, float r) {
  const int ri = int(std::ceil(r / w.dx));
  const int ci = int(std::floor((p[0] - w.origin[0]) / w.dx)), cj = int(std::floor((p[1] - w.origin[1]) / w.dx)),
            ck = int(std::floor((p[2] - w.origin[2]) / w.dx));
  float best = 1e9f;
  for (int k = std::max(0, ck - ri); k <= std::min(int(w.size[2]) - 1, ck + ri); k++)
    for (int j = std::max(0, cj - ri); j <= std::min(int(w.size[1]) - 1, cj + ri); j++)
      for (int i = std::max(0, ci - ri); i <= std::min(int(w.size[0]) - 1, ci + ri); i++)
        if ((w.Center(i, j, k) - p).norm() <= r) best = std::min(best, G(i, j, k));
  return best;
}

// number of distinct placed fruit that the posed fruit touches (a sample point within tol of the neighbor).
unsigned CountContacts(PackingScene &scene, unsigned itemId, const RigidTransform &tran, float tol) {
  scene.EnsureItemSamples(itemId);
  const MeshInfo &mi = scene.items[itemId];
  Box3f box = ComputeBBox(MakeTransformedMesh(mi.mesh, tran).v);
  unsigned n = 0;
  for (unsigned j : scene.broadPhase.GetNearby(box, tol)) {
    const InstanceInfo &o = scene.instances[j];
    if (o.isExclusion) continue;
    scene.EnsureItemSamples(o.itemId);
    const MeshInfo &oi = scene.items[o.itemId];
    if (!oi.sdf) continue;
    const Matrix3f inv = o.tran.rotation.transposed();
    for (const SamplePoint &sp : mi.samples) {
      if (oi.sdf->GetCoarseDist(inv * (tran.rotation * sp.x + tran.position - o.tran.position)) < tol) {
        n++;
        break;
      }
    }
  }
  return n;
}

float Pen(PackingScene &scene, unsigned itemId, const RigidTransform &tran, unsigned skipId, float wallTol = 0.0f) {
  scene.EnsureItemSamples(itemId);
  const MeshInfo &mi = scene.items[itemId];
  Box3f box = ComputeBBox(MakeTransformedMesh(mi.mesh, tran).v);
  float pen = 0.0f;
  for (const SamplePoint &sp : mi.samples) {
    pen = std::max(pen, scene.sdf->GetCoarseDist(tran.rotation * sp.x + tran.position) - wallTol);
  }
  for (unsigned j : scene.broadPhase.GetNearby(box, 0.0f)) {
    if (j == skipId) continue;
    const InstanceInfo &o = scene.instances[j];
    scene.EnsureItemSamples(o.itemId);
    const MeshInfo &oi = scene.items[o.itemId];
    if (!oi.sdf) continue;
    const Matrix3f inv = o.tran.rotation.transposed();
    for (const SamplePoint &sp : mi.samples) {
      const float d = oi.sdf->GetCoarseDist(inv * (tran.rotation * sp.x + tran.position - o.tran.position));
      pen = std::max(pen, -d);
    }
  }
  return pen;
}

Matrix3f AlignAxis(const Vec3f &from, const Vec3f &to) {
  const Vec3f ax = from.cross(to);
  const float s = ax.norm(), cs = from.dot(to);
  if (s < 1e-5f) {
    if (cs > 0.0f) return Matrix3f::identity();
    Vec3f perp = std::fabs(from[0]) < 0.9f ? Vec3f(1, 0, 0) : Vec3f(0, 1, 0);
    perp = from.cross(perp);
    perp.normalize();
    return Matrix3f::rotation(perp, 3.14159265f);
  }
  return Matrix3f::rotation((1.0f / s) * ax, std::atan2(s, cs));
}

struct Pose {
  unsigned kind = 0;
  RigidTransform tran;
  std::vector<RigidTransform> traj;
  float obj = 1e30f;
  bool valid = false;
};

// settles a pose into the valley: inward along the wall gradient plus a pull to the anchor.
RigidTransform Settle(Ctx &c, unsigned kind, const RigidTransform &start, const Vec3f &anchor,
                      std::vector<RigidTransform> &traj, size_t steps0 = 40, size_t steps1 = 160) {
  PackingScene &scene = c.scene;
  const std::function<Vec3f(const Vec3f &)> field = [&](const Vec3f &p) {
    Vec3f g = scene.sdf->GetCoarseGrad(p);
    g.normalize();
    Vec3f to = anchor - p;
    const float l = to.norm();
    Vec3f f = -0.7f * g;
    if (l > 0.3f) f += (0.3f / l) * to;
    return f;
  };
  const std::function<Vec3f(const Vec3f &)> zero = [](const Vec3f &) { return Vec3f(0, 0, 0); };
  RigidTransform cur = start;
  const bool useHull = c.hulls && c.hulls->Get(kind, scene.items[kind]);
  for (int stage = 0; stage < 2; stage++) {
    const auto &f = stage == 0 ? zero : field;
    const size_t steps = stage == 0 ? steps0 : steps1;
    cur = useHull ? NudgeWithHulls(scene, *c.hulls, kind, cur, f, steps, traj)
                  : scene.Nudge(kind, cur, Vec3f(0, 0, 0), 0.0f, traj, &f, steps);
  }
  return cur;
}

// tries poses of the given kinds at one cavity anchor. w/baseSolid: window without the moving
// fruit (selfId is out of the broad phase). returns the best pose by objective.
Pose TryAnchor(Ctx &c, const CavityComp &a, const std::vector<unsigned> &kinds, unsigned trials,
               unsigned selfId, CavityWindow &w, const Array3D8u &baseSolid) {
  PackingScene &scene = c.scene;
  std::uniform_real_distribution<float> u01(0.0f, 1.0f);
  Pose best;
  std::vector<unsigned> fit;
  for (unsigned k : kinds) {
    scene.EnsureItemLoaded(k);
    const Vec3f e = scene.items[k].box.vmax - scene.items[k].box.vmin;
    if (0.5f * std::min({e[0], e[1], e[2]}) <= a.clearance + 0.4f) fit.push_back(k);
  }
  if (fit.empty()) return best;
  Array3D<float> geo;
  GeoDepth(w, baseSolid, geo);
  for (unsigned t = 0; t < trials; t++) {
    c.nTrial++;
    // bigger kinds first in expectation: fewer fruit per cavity.
    unsigned kind = fit[c.rng() % fit.size()];
    { const unsigned k2 = fit[c.rng() % fit.size()]; if (scene.items[k2].rb.vol > scene.items[kind].rb.vol) kind = k2; }
    const MeshInfo &mi = scene.items[kind];
    RigidTransform tr;
    if (t % 4 != 3) {
      const Vec3f e = mi.box.vmax - mi.box.vmin;
      const int la = (e[0] >= e[1] && e[0] >= e[2]) ? 0 : (e[1] >= e[2] ? 1 : 2);
      Vec3f from(0, 0, 0);
      from[la] = 1.0f;
      const Matrix3f roll = Matrix3f::rotation(a.axis, 6.2831853f * u01(c.rng));
      tr.rotation = roll * AlignAxis(from, a.axis);
    } else {
      tr.rotation = RotationMatrixRad(6.2831853f * u01(c.rng), 6.2831853f * u01(c.rng), 6.2831853f * u01(c.rng));
    }
    tr.position = a.deepest;
    Array3D8u solid = baseSolid;
    unsigned added = 0;
    const unsigned already = StampFruit(scene, kind, tr, w, solid, &added);
    if (already > 3u * (added + 1u)) {
      c.nOverlap++;
      continue;
    }
    std::vector<RigidTransform> traj;
    const RigidTransform fin = Settle(c, kind, tr, a.deepest, traj);
    if (GeoNear(w, geo, fin.position, 2.0f) > c.cfg.cavReach) continue;
    const float pen = Pen(scene, kind, fin, selfId);
    if (c.nPen < 40 && pen > 0.4f) LOGI("pen " << pen << " kind " << mi.name << " clear " << a.clearance << " moved " << (fin.position - tr.position).norm() << " steps " << traj.size() << " already " << already << "/" << added << char(10));
    if (pen > 0.2f) {
      c.nPen++;
      continue;
    }
    solid = baseSolid;
    const unsigned overl = StampFruit(scene, kind, fin, w, solid, &added);
    if (float(overl) * w.dx * w.dx * w.dx > 0.05f * mi.rb.vol) continue;
    c.nOk++;
    float obj = 0.0f;
    FindCavities(w, solid, c.cp, nullptr, &obj, nullptr);
    if (obj < best.obj) {
      best.obj = obj;
      best.kind = kind;
      best.tran = fin;
      best.traj = traj;
      best.valid = true;
    }
  }
  return best;
}

Box3f AnchorBox(const Vec3f &p, float h) {
  Box3f b;
  b.vmin = p - Vec3f(h, h, h);
  b.vmax = p + Vec3f(h, h, h);
  return b;
}

// spawn pass: one tile at a time, largest cavities first.
unsigned SpawnPass(Ctx &c, std::vector<unsigned> &spawned) {
  const float margin = 12.0f;
  unsigned placed = 0, tried = 0;
  Utils::Stopwatch clock;
  clock.Start();
  const std::vector<Tile> tiles = MakeTiles(c, 40.0f);
  size_t ti = 0;
  for (const Tile &t : tiles) {
    ti++;
    CavityWindow w0;
    BuildCavityWindow(c.scene, Grow(t.core, margin), c.cp, ~0u, w0);
    float s0 = 0;
    std::vector<CavityComp> comps = FindCavities(w0, w0.solid, c.cp, &t.core, &s0, nullptr);
    std::sort(comps.begin(), comps.end(), [](const CavityComp &a, const CavityComp &b) { return a.score > b.score; });
    if (c.cfg.cavMaxPerPass && comps.size() > c.cfg.cavMaxPerPass) comps.resize(c.cfg.cavMaxPerPass);
    for (const CavityComp &a : comps) {
      if (a.score < c.cfg.cavMinGain) break;
      CavityWindow w;
      BuildCavityWindow(c.scene, AnchorBox(a.deepest, 14.0f), c.cp, ~0u, w);
      float base = 0;
      FindCavities(w, w.solid, c.cp, nullptr, &base, nullptr);
      tried++;
      Pose p = TryAnchor(c, a, c.kinds, c.cfg.cavTrials, ~0u, w, w.solid);
      if (!p.valid || base - p.obj < c.cfg.cavMinGain) continue;
      const unsigned id = c.scene.AddInstance(p.kind, p.tran);
      c.scene.instances[id].trajectory = p.traj;
      spawned.push_back(id);
      placed++;
    }
    LOGI("spawn tile " << ti << "/" << tiles.size() << ": " << comps.size() << " anchors, placed so far " << placed
                       << ", " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
  }
  LOGI("trials " << c.nTrial << " overlap-skip " << c.nOverlap << " penetrating " << c.nPen << " valid " << c.nOk << char(10));
  LOGI("spawn pass: tried " << tried << ", placed " << placed << char(10));
  return placed;
}

// relocate pass: lift each medium fruit near a cavity and try it in the cavities around it.
unsigned RelocatePass(Ctx &c, std::vector<unsigned> &moved) {
  PackingScene &scene = c.scene;
  unsigned tried = 0, accepted = 0;
  Utils::Stopwatch clock;
  clock.Start();
  std::vector<unsigned> ids;
  for (unsigned i = 0; i < scene.instances.size(); i++) {
    const InstanceInfo &inst = scene.instances[i];
    if (inst.isExclusion || !c.isMedium[inst.itemId] || inst.tran.position[0] >= c.maxX) continue;
    if (-scene.sdf->GetCoarseDist(inst.tran.position) > c.cp.band + 4.0f) continue;
    ids.push_back(i);
  }
  LOGI("relocate pass: " << ids.size() << " medium fruit near the wall" << char(10));
  size_t done = 0;
  for (unsigned id : ids) {
    done++;
    InstanceInfo &inst = scene.instances[id];
    const RigidTransform oldTran = inst.tran;
    const unsigned kind = inst.itemId;
    CavityWindow w;
    BuildCavityWindow(scene, AnchorBox(oldTran.position, 15.0f), c.cp, id, w);
    Array3D8u withOld = w.solid;
    StampFruit(scene, kind, oldTran, w, withOld);
    float objOld = 0;
    std::vector<CavityComp> anchors = FindCavities(w, withOld, c.cp, nullptr, &objOld, nullptr);
    std::vector<CavityComp> nearby;
    for (const CavityComp &a : anchors) {
      const float d = (a.deepest - oldTran.position).norm();
      if (d < 12.0f && a.score > c.cfg.cavMinGain) nearby.push_back(a);
    }
    std::sort(nearby.begin(), nearby.end(), [](const CavityComp &a, const CavityComp &b) { return a.score > b.score; });
    if (nearby.size() > 3) nearby.resize(3);
    if (nearby.empty()) continue;
    tried++;
    scene.broadPhase.Remove(WorldBox(scene, inst), id);
    Pose best;
    for (const CavityComp &a : nearby) {
      Pose p = TryAnchor(c, a, {kind}, std::max(4u, c.cfg.cavTrials / 3), id, w, w.solid);
      if (p.valid && p.obj < best.obj) best = p;
    }
    if (best.valid && objOld - best.obj > c.cfg.cavMinGain) {
      inst.tran = best.tran;
      if (inst.trajectory.empty()) inst.trajectory.push_back(oldTran);
      inst.trajectory.insert(inst.trajectory.end(), best.traj.begin(), best.traj.end());
      moved.push_back(id);
      accepted++;
    }
    scene.broadPhase.Add(WorldBox(scene, inst), id);
    if (done % 100 == 0) {
      LOGI("relocate " << done << "/" << ids.size() << ", tried " << tried << ", moved " << accepted << ", "
                       << clock.ElapsedMS() / 1000.0 << " s" << char(10));
    }
  }
  LOGI("relocate pass: tried " << tried << ", moved " << accepted << char(10));
  return accepted;
}

// ---- mouth capping -------------------------------------------------------------------------

struct MouthCluster {
  std::vector<std::array<int, 3>> vox;
  std::vector<Vec3f> pos, outDir;
  Vec3f center, t;
  Box3f box;
};

// clusters of mouth voxels: unreachable air-connected voxels next to voxels the tool reaches.
std::vector<MouthCluster> FindMouthClusters(const CavityWindow &w, const Array3D8u &solid, const CavityParams &cp) {
  Array3D<float> D;
  Array3D8u cav, reachFree;
  BuildCavMask(w, solid, cp, D, cav, reachFree);
  const Vec3u s = w.size;
  Array3D8u isMouth(s, 0), seen(s, 0);
  Array3D<Vec3f> outv(s, Vec3f(0, 0, 0));
  const int nb6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (unsigned k = 0; k < s[2]; k++)
    for (unsigned j = 0; j < s[1]; j++)
      for (unsigned i = 0; i < s[0]; i++) {
        if (!cav(i, j, k) || D(i, j, k) < cp.objClear) continue;
        Vec3f o(0, 0, 0);
        for (const auto &d : nb6) {
          const int x = int(i) + d[0], y = int(j) + d[1], z = int(k) + d[2];
          if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
          if (reachFree(x, y, z) && !cav(x, y, z)) o += Vec3f(float(d[0]), float(d[1]), float(d[2]));
        }
        if (o.norm() > 0.0f || false) {
          isMouth(i, j, k) = 1;
          outv(i, j, k) = o;
        }
      }
  std::vector<MouthCluster> out;
  std::vector<std::array<int, 3>> stack;
  for (unsigned k0 = 0; k0 < s[2]; k0++)
    for (unsigned j0 = 0; j0 < s[1]; j0++)
      for (unsigned i0 = 0; i0 < s[0]; i0++) {
        if (!isMouth(i0, j0, k0) || seen(i0, j0, k0)) continue;
        MouthCluster mc;
        stack.assign(1, {int(i0), int(j0), int(k0)});
        seen(i0, j0, k0) = 1;
        while (!stack.empty()) {
          const auto p = stack.back();
          stack.pop_back();
          mc.vox.push_back(p);
          for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
              for (int dxx = -1; dxx <= 1; dxx++) {
                const int x = p[0] + dxx, y = p[1] + dy, z = p[2] + dz;
                if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
                if (!isMouth(x, y, z) || seen(x, y, z)) continue;
                seen(x, y, z) = 1;
                stack.push_back({x, y, z});
              }
        }
        if (mc.vox.size() < 8) continue;
        Vec3f sum(0, 0, 0);
        for (const auto &v : mc.vox) {
          mc.pos.push_back(w.Center(v[0], v[1], v[2]));
          Vec3f o = outv(v[0], v[1], v[2]);
          o.normalize();
          mc.outDir.push_back(o);
          sum += mc.pos.back();
        }
        mc.center = (1.0f / float(mc.vox.size())) * sum;
        float cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        mc.box.vmin = Vec3f(1e30f, 1e30f, 1e30f);
        mc.box.vmax = Vec3f(-1e30f, -1e30f, -1e30f);
        for (const Vec3f &p : mc.pos) {
          const Vec3f d = p - mc.center;
          for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) cov[a][b] += d[a] * d[b];
            mc.box.vmin[a] = std::min(mc.box.vmin[a], p[a]);
            mc.box.vmax[a] = std::max(mc.box.vmax[a], p[a]);
          }
        }
        Vec3f ax(0.577f, 0.5f, 0.64f);
        for (int it = 0; it < 15; it++) {
          Vec3f nx(cov[0][0] * ax[0] + cov[0][1] * ax[1] + cov[0][2] * ax[2],
                   cov[1][0] * ax[0] + cov[1][1] * ax[1] + cov[1][2] * ax[2],
                   cov[2][0] * ax[0] + cov[2][1] * ax[1] + cov[2][2] * ax[2]);
          const float l = nx.norm();
          if (l < 1e-9f) break;
          ax = (1.0f / l) * nx;
        }
        mc.t = ax;
        out.push_back(mc);
      }
  return out;
}

struct CapCand {
  unsigned kind = 0;
  RigidTransform tran;
  int a = 0, b = 0;
  float val = 0.0f, cost = 0.0f;
};

// caps one mouth cluster with a chain of fruit chosen by a 1D DP along the mouth centerline.
// returns the poses to commit (not yet added to the scene).
std::vector<CapCand> PlanCaps(Ctx &c, const MouthCluster &mc, CavityWindow &w, const Array3D8u &baseSolid) {
  PackingScene &scene = c.scene;
  std::vector<CapCand> chosen;
  const float ds = 0.8f;
  // bins along the slot direction.
  float smin = 1e30f, smax = -1e30f;
  for (const Vec3f &p : mc.pos) {
    const float sv = (p - mc.center).dot(mc.t);
    smin = std::min(smin, sv);
    smax = std::max(smax, sv);
  }
  const int B = std::max(1, int(std::ceil((smax - smin) / ds)));
  struct Bin {
    Vec3f sum = Vec3f(0, 0, 0), out = Vec3f(0, 0, 0);
    float lo = 1e30f, hi = -1e30f;
    int n = 0;
  };
  std::vector<Bin> bins(B);
  std::vector<int> binOf(mc.pos.size());
  const Vec3f perp0 = mc.t.cross(Vec3f(0.3f, 0.8f, 0.5f));
  for (size_t i = 0; i < mc.pos.size(); i++) {
    const int bi = std::min(B - 1, int((((mc.pos[i] - mc.center).dot(mc.t)) - smin) / ds));
    binOf[i] = bi;
    bins[bi].sum += mc.pos[i];
    bins[bi].out += mc.outDir[i];
    bins[bi].n++;
  }
  // width of each bin: extent across the slot, perpendicular to t and the bin's out direction.
  std::vector<Vec3f> bc(B), bn(B);
  std::vector<float> bw(B, 0.0f);
  for (int b = 0; b < B; b++) {
    if (!bins[b].n) continue;
    bc[b] = (1.0f / float(bins[b].n)) * bins[b].sum;
    Vec3f o = bins[b].out;
    if (o.norm() < 1e-6f) o = perp0;
    o.normalize();
    bn[b] = o;
    Vec3f u = mc.t.cross(o);
    if (u.norm() < 1e-6f) u = perp0;
    u.normalize();
    for (size_t i = 0; i < mc.pos.size(); i++) {
      if (binOf[i] != b) continue;
      const float x = (mc.pos[i] - bc[b]).dot(u);
      bins[b].lo = std::min(bins[b].lo, x);
      bins[b].hi = std::max(bins[b].hi, x);
    }
    bw[b] = bins[b].hi - bins[b].lo + w.dx;
  }
  std::uniform_real_distribution<float> u01(0.0f, 1.0f);
  std::vector<CapCand> cands;
  for (int b = 0; b < B; b++) {
    if (!bins[b].n) continue;
    for (unsigned k : c.kinds) {
      scene.EnsureItemLoaded(k);
      const MeshInfo &mi = scene.items[k];
      const Vec3f e = mi.box.vmax - mi.box.vmin;
      const float emin = std::min({e[0], e[1], e[2]});
      if (emin < 1.25f * bw[b]) continue;
      const int la = (e[0] >= e[1] && e[0] >= e[2]) ? 0 : (e[1] >= e[2] ? 1 : 2);
      Vec3f from(0, 0, 0);
      from[la] = 1.0f;
      const float hmax = 0.5f * e.norm() + 1.0f;
      for (int roll = 0; roll < 4; roll++) {
        RigidTransform tr;
        tr.rotation = Matrix3f::rotation(mc.t, 1.5707963f * roll + 0.3f * u01(c.rng)) * AlignAxis(from, mc.t);
        auto overlapAt = [&](float h, Array3D8u &tmp) {
          tr.position = bc[b] + h * bn[b];
          tmp = baseSolid;
          unsigned added = 0;
          return StampFruit(scene, k, tr, w, tmp, &added);
        };
        Array3D8u tmp;
        if (overlapAt(hmax, tmp) > 2) continue;
        float lo = 0.0f, hi = hmax;
        if (overlapAt(0.0f, tmp) <= 2) hi = 0.0f;
        for (int it = 0; it < 6 && hi > 0.0f; it++) {
          const float mid = 0.5f * (lo + hi);
          if (overlapAt(mid, tmp) <= 2) hi = mid;
          else lo = mid;
        }
        overlapAt(hi, tmp);
        // mouth voxels that the fruit now covers (it or a neighbor voxel is newly solid).
        int a = B, bb = -1;
        float covered = 0.0f;
        for (size_t i = 0; i < mc.vox.size(); i++) {
          const auto &v = mc.vox[i];
          bool hit = false;
          for (int dz = -1; dz <= 1 && !hit; dz++)
            for (int dy = -1; dy <= 1 && !hit; dy++)
              for (int dxx = -1; dxx <= 1 && !hit; dxx++) {
                const int x = v[0] + dxx, y = v[1] + dy, z = v[2] + dz;
                if (x < 0 || y < 0 || z < 0 || x >= int(w.size[0]) || y >= int(w.size[1]) || z >= int(w.size[2])) continue;
                hit = tmp(x, y, z) && !baseSolid(x, y, z);
              }
          if (hit) {
            covered += 1.0f;
            a = std::min(a, binOf[i]);
            bb = std::max(bb, binOf[i]);
          }
        }
        if (bb < 0 || covered < 4.0f) continue;
        CapCand cc;
        cc.kind = k;
        cc.tran = tr;
        cc.a = a;
        cc.b = bb;
        cc.val = covered;
        cc.cost = 8.0f + 0.002f * mi.rb.vol;
        cands.push_back(cc);
      }
    }
  }
  if (cands.empty()) return chosen;
  // weighted interval scheduling over the bins.
  std::vector<float> dp(B + 1, 0.0f);
  std::vector<int> from(B + 1, -1);
  for (int e = 1; e <= B; e++) {
    dp[e] = dp[e - 1];
    from[e] = -1;
    for (size_t i = 0; i < cands.size(); i++) {
      if (cands[i].b + 1 != e) continue;
      const float v = dp[cands[i].a] + cands[i].val - cands[i].cost;
      if (v > dp[e]) {
        dp[e] = v;
        from[e] = int(i);
      }
    }
  }
  for (int e = B; e > 0;) {
    if (from[e] < 0) {
      e--;
      continue;
    }
    chosen.push_back(cands[from[e]]);
    e = cands[from[e]].a;
  }
  return chosen;
}

unsigned CapPass(Ctx &c, std::vector<unsigned> &spawned) {
  PackingScene &scene = c.scene;
  const float margin = 12.0f;
  unsigned placed = 0, clusters = 0, accepted = 0, dNoPlan = 0, dKeepEmpty = 0, dLowGain = 0, dPen = 0, dOv = 0;
  Utils::Stopwatch clock;
  clock.Start();
  const std::vector<Tile> tiles = MakeTiles(c, 40.0f);
  size_t ti = 0;
  for (const Tile &t : tiles) {
    ti++;
    CavityWindow w0;
    BuildCavityWindow(scene, Grow(t.core, margin), c.cp, ~0u, w0);
    std::vector<MouthCluster> mcs = FindMouthClusters(w0, w0.solid, c.cp);
    std::vector<Vec3f> centers;
    for (const MouthCluster &mc : mcs) {
      const Vec3f p = mc.center;
      if (p[0] >= t.core.vmin[0] && p[0] < t.core.vmax[0] && p[1] >= t.core.vmin[1] && p[1] < t.core.vmax[1] &&
          p[2] >= t.core.vmin[2] && p[2] < t.core.vmax[2])
        centers.push_back(p);
    }
    for (const Vec3f &cen : centers) {
      // fresh window: earlier caps in this tile are already in the scene.
      CavityWindow w;
      BuildCavityWindow(scene, AnchorBox(cen, 20.0f), c.cp, ~0u, w);
      std::vector<MouthCluster> now = FindMouthClusters(w, w.solid, c.cp);
      const MouthCluster *pick = nullptr;
      float bestD = 3.0f;
      for (const MouthCluster &m : now) {
        const float d = (m.center - cen).norm();
        if (d < bestD) {
          bestD = d;
          pick = &m;
        }
      }
      if (!pick) continue;
      clusters++;
      std::vector<CapCand> plan = PlanCaps(c, *pick, w, w.solid);
      if (plan.empty()) {
        dNoPlan++;
        continue;
      }
      float base = 0;
      FindCavities(w, w.solid, c.cp, nullptr, &base, nullptr);
      Array3D8u solid = w.solid;
      std::vector<CapCand> keep;
      for (const CapCand &cc : plan) {
        const float pn = Pen(scene, cc.kind, cc.tran, ~0u, c.cfg.cavWallTol);
        if (pn > 0.4f) {
          dPen++;
          if (dPen < 15) {
            float wp = -1e9f;
            for (const SamplePoint &sp : scene.items[cc.kind].samples) wp = std::max(wp, scene.sdf->GetCoarseDist(cc.tran.rotation * sp.x + cc.tran.position));
            LOGI("cap pen " << pn << " wall protrusion " << wp << " " << scene.items[cc.kind].name << char(10));
          }
          continue;
        }
        Array3D8u tmp = solid;
        unsigned added = 0;
        if (StampFruit(scene, cc.kind, cc.tran, w, tmp, &added) > 6) {
          dOv++;
          continue;
        }
        solid = tmp;
        keep.push_back(cc);
      }
      if (keep.empty()) {
        dKeepEmpty++;
        continue;
      }
      float obj = 0;
      FindCavities(w, solid, c.cp, nullptr, &obj, nullptr);
      if (base - obj < c.cfg.cavMinGain) {
        dLowGain++;
        LOGI("cap low gain " << base - obj << " with " << keep.size() << " fruit, cluster size " << pick->vox.size() << char(10));
        continue;
      }
      accepted++;
      for (const CapCand &cc : keep) {
        const unsigned id = scene.AddInstance(cc.kind, cc.tran);
        spawned.push_back(id);
        placed++;
      }
    }
    LOGI("cap tile " << ti << "/" << tiles.size() << ": clusters " << clusters << ", plans accepted " << accepted
                     << ", fruit " << placed << ", " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
  }
  LOGI("cap fails: no plan " << dNoPlan << ", all rejected " << dKeepEmpty << ", low gain " << dLowGain << ", pen " << dPen << ", chain overlap " << dOv << char(10));
  LOGI("cap pass: " << clusters << " mouth clusters, " << accepted << " accepted, " << placed << " fruit" << char(10));
  return placed;
}

void ReportMetric(Ctx &c, const char *label, const std::string &objFile) {
  Utils::Stopwatch clock;
  clock.Start();
  float score = 0, vol = 0;
  unsigned deep = 0;
  std::vector<CavityComp> cs = AllCavities(c, score, vol, deep);
  LOGI("metric " << label << ": " << cs.size() << " cavities, volume " << vol << " cm3, weighted " << score
                 << ", with clearance >= 1.5 cm: " << deep << ", " << clock.ElapsedMS() / 1000.0 << " s"
                 << char(10));
  std::sort(cs.begin(), cs.end(), [](const CavityComp &a, const CavityComp &b) { return a.volume > b.volume; });
  for (size_t i = 0; i < cs.size() && i < 8; i++) {
    LOGI("  cavity " << i << " vol " << cs[i].volume << " clear " << cs[i].clearance << " at " << cs[i].deepest[0]
                     << " " << cs[i].deepest[1] << " " << cs[i].deepest[2] << char(10));
  }
  if (!objFile.empty()) SaveCavityObj(cs, objFile);
}

// ---- dimple fill ---------------------------------------------------------------------------
// free space inside the container is filled by the kind and pose with the best fill of its own
// volume. no tool model: a snug fruit that ends flush with the surface is what counts.

struct Dimple {
  Vec3f p;         // free ball center (largest clearance)
  Vec3f centroid;  // centroid of the free pocket voxels around it
  Vec3f axis;      // principal axis of the pocket, unit
  float clear = 0;
  float vol = 0;   // free volume inside the container within 4 cm (cm3)
};

// free inside-container voxels within r of p: volume, centroid and principal axis.
bool PocketStats(const CavityWindow &w, const Vec3f &p, float r, Dimple &d) {
  const Vec3u n = w.size;
  const float dx = w.dx;
  const int ri = int(std::ceil(r / dx));
  const int ci = int((p[0] - w.origin[0]) / dx), cj = int((p[1] - w.origin[1]) / dx), ck = int((p[2] - w.origin[2]) / dx);
  std::vector<Vec3f> pts;
  for (int k = std::max(0, ck - ri); k <= std::min(int(n[2]) - 1, ck + ri); k++)
    for (int j = std::max(0, cj - ri); j <= std::min(int(n[1]) - 1, cj + ri); j++)
      for (int i = std::max(0, ci - ri); i <= std::min(int(n[0]) - 1, ci + ri); i++) {
        if (w.solid(i, j, k) || w.wall(i, j, k)) continue;
        const Vec3f q = w.Center(i, j, k);
        if ((q - p).norm() > r) continue;
        pts.push_back(q);
      }
  d.p = p;
  d.vol = float(pts.size()) * dx * dx * dx;
  if (pts.size() < 8) return false;
  Vec3f m(0, 0, 0);
  for (const Vec3f &q : pts) m += q;
  m = (1.0f / float(pts.size())) * m;
  double C[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  for (const Vec3f &q : pts) {
    const Vec3f e = q - m;
    for (int a = 0; a < 3; a++)
      for (int b = 0; b < 3; b++) C[a][b] += double(e[a]) * double(e[b]);
  }
  double v[3] = {1, 0.3, 0.1};
  for (int it = 0; it < 30; it++) {
    double u[3];
    for (int a = 0; a < 3; a++) u[a] = C[a][0] * v[0] + C[a][1] * v[1] + C[a][2] * v[2];
    const double l = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]) + 1e-12;
    for (int a = 0; a < 3; a++) v[a] = u[a] / l;
  }
  d.centroid = m;
  d.axis = Vec3f(float(v[0]), float(v[1]), float(v[2]));
  return true;
}

// seeds: local maxima of the clearance (distance to placed fruit) among free voxels inside the
// container, near the surface, with the center in core. sorted by pocket volume, largest first.
std::vector<Dimple> FindDimples(const Ctx &c, const CavityWindow &w, const Box3f &core, bool snug = false) {
  const Vec3u n = w.size;
  const float dx = w.dx;
  Array3D<float> D;
  DistanceToSeeds(w.solid, dx, D);
  struct Cand {
    float d;
    int i, j, k;
  };
  std::vector<Cand> cand;
  Array3D<float> geo;
  GeoDepth(w, w.solid, geo);
  for (unsigned k = 0; k < n[2]; k++)
    for (unsigned j = 0; j < n[1]; j++)
      for (unsigned i = 0; i < n[0]; i++) {
        if (w.solid(i, j, k) || w.wall(i, j, k) || geo(i, j, k) > c.cfg.cavReach) continue;
        const float d = D(i, j, k);
        if (d < c.cfg.cavDimpleClear) continue;
        const Vec3f p = w.Center(i, j, k);
        if (p[0] < core.vmin[0] || p[0] >= core.vmax[0] || p[1] < core.vmin[1] || p[1] >= core.vmax[1] ||
            p[2] < core.vmin[2] || p[2] >= core.vmax[2]) continue;
        cand.push_back({d, int(i), int(j), int(k)});
      }
  std::sort(cand.begin(), cand.end(), [](const Cand &a, const Cand &b) { return a.d > b.d; });
  // only spots near space the tool cannot reach are worth a fruit.
  Array3D<float> Dc;
  Array3D8u cav, reachFree;
  BuildCavMask(w, w.solid, c.cp, Dc, cav, reachFree);
  Array3D<float> distCav;
  DistanceToSeeds(cav, dx, distCav);
  Array3D8u taken(n, 0);
  const int sr = int(std::ceil(2.0f / dx));
  std::vector<Dimple> out;
  for (const Cand &cd : cand) {
    if (taken(cd.i, cd.j, cd.k)) continue;
    if (!snug && distCav(cd.i, cd.j, cd.k) > 3.0f) continue;
    for (int k = std::max(0, cd.k - sr); k <= std::min(int(n[2]) - 1, cd.k + sr); k++)
      for (int j = std::max(0, cd.j - sr); j <= std::min(int(n[1]) - 1, cd.j + sr); j++)
        for (int i = std::max(0, cd.i - sr); i <= std::min(int(n[0]) - 1, cd.i + sr); i++)
          if ((i - cd.i) * (i - cd.i) + (j - cd.j) * (j - cd.j) + (k - cd.k) * (k - cd.k) <= sr * sr)
            taken(i, j, k) = 1;
    Dimple d;
    if (!PocketStats(w, w.Center(cd.i, cd.j, cd.k), 4.0f, d)) continue;
    d.clear = cd.d;
    if (d.vol < c.cfg.cavFillVol) continue;
    out.push_back(d);
  }
  std::sort(out.begin(), out.end(), [](const Dimple &a, const Dimple &b) { return a.vol > b.vol; });
  return out;
}

struct StampCount {
  unsigned inside = 0, outside = 0, overlap = 0;
};

// stamps a pose into a copy of base: new voxels inside the container, outside it, and overlapped ones.
StampCount CountStamp(Ctx &c, unsigned kind, const RigidTransform &tr, CavityWindow &w, const Array3D8u &base,
                      Array3D8u &tmp) {
  tmp = base;
  StampCount sc;
  sc.overlap = StampFruit(c.scene, kind, tr, w, tmp);
  const std::vector<unsigned char> &a = base.GetData();
  const std::vector<unsigned char> &b = tmp.GetData();
  const std::vector<unsigned char> &wl = w.wall.GetData();
  for (size_t i = 0; i < a.size(); i++) {
    if (b[i] && !a[i]) {
      if (wl[i]) sc.outside++;
      else sc.inside++;
    }
  }
  return sc;
}

struct DimpleHit {
  bool valid = false;
  unsigned kind = 0;
  RigidTransform tran;
  std::vector<RigidTransform> traj;
  float fill = 0;      // fraction of the fruit volume that fills free space
  float fillVol = 0;   // cm3
};

// base: occupancy to fill into (w.solid plus any fruit already planned). useGate: also require the
// objective to drop by cavMinGain with this single fruit.
DimpleHit TryDimple(Ctx &c, const Dimple &d, CavityWindow &w, const Array3D8u &base, float baseObj, bool useGate,
                    unsigned minContacts = 0, float fillMinOverride = 0.0f) {
  PackingScene &scene = c.scene;
  std::uniform_real_distribution<float> u01(0.0f, 1.0f);
  const float v3 = w.dx * w.dx * w.dx;
  struct Cand {
    unsigned kind;
    RigidTransform tr;
    float score;
  };
  std::vector<Cand> cands;
  Array3D8u tmp;
  Array3D<float> geo;
  GeoDepth(w, base, geo);
  bool dbg = false;
  if (const char *e = std::getenv("DBG_PT")) {
    float x, y, z;
    if (std::sscanf(e, "%f %f %f", &x, &y, &z) == 3) dbg = (d.p - Vec3f(x, y, z)).norm() < 6.0f;
  }
  if (dbg) LOGI("DBG seed " << d.p[0] << " " << d.p[1] << " " << d.p[2] << " clear " << d.clear << " vol " << d.vol << " geo " << GeoNear(w, geo, d.p, 0.5f) << " base " << baseObj << char(10));
  for (unsigned k : c.kinds) {
    scene.EnsureItemLoaded(k);
    const MeshInfo &mi = scene.items[k];
    const Vec3f e = mi.box.vmax - mi.box.vmin;
    if (0.5f * std::min({e[0], e[1], e[2]}) > d.clear + 1.2f) continue;
    const int la = (e[0] >= e[1] && e[0] >= e[2]) ? 0 : (e[1] >= e[2] ? 1 : 2);
    Vec3f from(0, 0, 0);
    from[la] = 1.0f;
    for (int s = 0; s < 14; s++) {
      RigidTransform tr;
      if (s < 12) {
        const Vec3f ax = (s % 2 == 0) ? d.axis : -1.0f * d.axis;
        const Matrix3f roll = Matrix3f::rotation(ax, 6.2831853f * float(s / 2) / 6.0f);
        tr.rotation = roll * AlignAxis(from, ax);
      } else {
        tr.rotation = RotationMatrixRad(6.2831853f * u01(c.rng), 6.2831853f * u01(c.rng), 6.2831853f * u01(c.rng));
      }
      tr.position = (s % 3 == 2) ? d.centroid : d.p;
      const StampCount sc = CountStamp(c, k, tr, w, base, tmp);
      if (float(sc.overlap) * v3 > 0.25f * mi.rb.vol) continue;
      const float score = float(sc.inside) - 3.0f * float(sc.overlap) - 3.0f * float(sc.outside);
      cands.push_back({k, tr, score});
    }
  }
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) { return a.score > b.score; });
  if (dbg) LOGI("DBG cands " << cands.size() << char(10));
  if (cands.size() > c.cfg.cavTrials / 2u) cands.resize(c.cfg.cavTrials / 2u);
  DimpleHit best;
  const float wallTol = std::max(0.0f, c.cfg.cavProtrude - 0.4f);
  for (const Cand &cd : cands) {
    c.nTrial++;
    std::vector<RigidTransform> traj;
    const RigidTransform fin = Settle(c, cd.kind, cd.tr, d.p, traj, 15, 60);
    const std::string nm = scene.items[cd.kind].name;
    if (GeoNear(w, geo, fin.position, 2.0f) > c.cfg.cavReach) {
      if (dbg) LOGI("DBG " << nm << " geo reject " << GeoNear(w, geo, fin.position, 2.0f) << char(10));
      continue;
    }
    const float pen = Pen(scene, cd.kind, fin, ~0u, wallTol);
    if (pen > 0.4f) {
      c.nPen++;
      if (dbg) LOGI("DBG " << nm << " pen " << pen << " moved " << (fin.position - cd.tr.position).norm() << char(10));
      continue;
    }
    const StampCount sc = CountStamp(c, cd.kind, fin, w, base, tmp);
    const float vol = scene.items[cd.kind].rb.vol;
    if (float(sc.overlap) * v3 > 0.05f * vol) {
      if (dbg) LOGI("DBG " << nm << " overlap " << float(sc.overlap) * v3 << " of " << vol << char(10));
      continue;
    }
    const float fillVol = float(sc.inside) * v3;
    const float fill = fillVol / std::max(1.0f, vol);
    const float fillMin = fillMinOverride > 0.0f ? fillMinOverride : c.cfg.cavFillMin;
    if (fill < fillMin || fillVol < c.cfg.cavFillVol) {
      if (dbg) LOGI("DBG " << nm << " fill " << fill << " vol " << fillVol << char(10));
      continue;
    }
    if (float(sc.outside) * v3 > 0.2f * vol) {
      if (dbg) LOGI("DBG " << nm << " outside " << float(sc.outside) * v3 << char(10));
      continue;
    }
    if (minContacts && CountContacts(scene, cd.kind, fin, 0.3f) < minContacts) {
      if (dbg) LOGI("DBG " << nm << " contacts " << CountContacts(scene, cd.kind, fin, 0.3f) << char(10));
      continue;
    }
    float obj = 0.0f;
    if (useGate) FindCavities(w, tmp, c.cp, nullptr, &obj, nullptr);
    if (dbg) LOGI("DBG " << nm << " gain " << baseObj - obj << " fill " << fill << char(10));
    if (useGate && baseObj - obj < c.cfg.cavMinGain) continue;
    c.nOk++;
    if (!best.valid || fillVol > best.fillVol) {
      best.valid = true;
      best.kind = cd.kind;
      best.tran = fin;
      best.traj = traj;
      best.fill = fill;
      best.fillVol = fillVol;
    }
  }
  return best;
}

// next fill seed inside a window whose occupancy is cur: the largest free ball within r of p whose
// path from the outside is short enough.
bool NextSeed(const Ctx &c, const CavityWindow &w, const Array3D8u &cur, const Vec3f &p, float r, Dimple &out) {
  Array3D<float> D, geo;
  DistanceToSeeds(cur, w.dx, D);
  GeoDepth(w, cur, geo);
  float bestD = 0.0f;
  Vec3f bp(0, 0, 0);
  for (unsigned k = 0; k < w.size[2]; k++)
    for (unsigned j = 0; j < w.size[1]; j++)
      for (unsigned i = 0; i < w.size[0]; i++) {
        if (cur(i, j, k) || w.wall(i, j, k) || geo(i, j, k) > c.cfg.cavReach) continue;
        const Vec3f q = w.Center(i, j, k);
        if ((q - p).norm() > r || D(i, j, k) < c.cfg.cavDimpleClear || D(i, j, k) <= bestD) continue;
        bestD = D(i, j, k);
        bp = q;
      }
  if (bestD <= 0.0f) return false;
  CavityWindow wc = w;
  wc.solid = cur;
  if (!PocketStats(wc, bp, 4.0f, out) || out.vol < c.cfg.cavFillVol) return false;
  out.clear = bestD;
  return true;
}

unsigned DimplePass(Ctx &c, std::vector<unsigned> &spawned) {
  const float margin = 12.0f;
  unsigned placed = 0, tried = 0;
  Utils::Stopwatch clock;
  clock.Start();
  const std::vector<Tile> tiles = MakeTiles(c, 40.0f);
  size_t ti = 0;
  std::map<std::string, unsigned> perKind;
  for (const Tile &t : tiles) {
    ti++;
    CavityWindow w0;
    BuildCavityWindow(c.scene, Grow(t.core, margin), c.cp, ~0u, w0);
    std::vector<Dimple> seeds = FindDimples(c, w0, t.core, c.cfg.cavSnug != 0);
    if (c.cfg.cavMaxPerPass && seeds.size() > c.cfg.cavMaxPerPass) seeds.resize(c.cfg.cavMaxPerPass);
    for (const Dimple &s : seeds) {
      CavityWindow w;
      BuildCavityWindow(c.scene, AnchorBox(s.p, 13.0f), c.cp, ~0u, w);
      Dimple d;
      if (!PocketStats(w, s.p, 4.0f, d) || d.vol < c.cfg.cavFillVol) continue;
      d.clear = s.clear;
      tried++;
      float baseObj = 0.0f;
      FindCavities(w, w.solid, c.cp, nullptr, &baseObj, nullptr);
      std::vector<DimpleHit> group;
      if (c.cfg.cavSnug) {
        // snug fit between at least cavContacts placed fruit; no tool gate.
        DimpleHit hs = TryDimple(c, d, w, w.solid, baseObj, false, c.cfg.cavContacts, c.cfg.cavSnugFill);
        if (hs.valid) group.push_back(hs);
      }
      DimpleHit h;
      if (!c.cfg.cavSnug) h = TryDimple(c, d, w, w.solid, baseObj, true);
      if (h.valid) group.push_back(h);
      else if (!c.cfg.cavSnug && d.vol >= 120.0f) {
        // a big pocket needs several fruit: plan up to 4 together, keep them only if the whole
        // group lowers the objective.
        Array3D8u cur = w.solid;
        Dimple dd = d;
        for (int g = 0; g < 4; g++) {
          DimpleHit hg = TryDimple(c, dd, w, cur, baseObj, false);
          if (!hg.valid) break;
          group.push_back(hg);
          StampFruit(c.scene, hg.kind, hg.tran, w, cur);
          if (!NextSeed(c, w, cur, s.p, 6.0f, dd)) break;
        }
        float obj = 0.0f;
        FindCavities(w, cur, c.cp, nullptr, &obj, nullptr);
        if (const char *e = std::getenv("DBG_PT")) {
          float x, y, z;
          if (std::sscanf(e, "%f %f %f", &x, &y, &z) == 3 && (s.p - Vec3f(x, y, z)).norm() < 6.0f) {
            LOGI("DBG group seed " << s.p[0] << " " << s.p[1] << " " << s.p[2] << " vol " << d.vol << " n " << group.size() << " base " << baseObj << " after " << obj << char(10));
          }
        }
        if (group.empty() || baseObj - obj < c.cfg.cavMinGain) group.clear();
        else LOGI("dimple group of " << group.size() << " gain " << baseObj - obj << char(10));
      }
      for (const DimpleHit &hh : group) {
        const unsigned id = c.scene.AddInstance(hh.kind, hh.tran);
        c.scene.instances[id].trajectory = hh.traj;
        spawned.push_back(id);
        perKind[c.scene.items[hh.kind].name]++;
        placed++;
        LOGI("dimple " << c.scene.items[hh.kind].name << " at " << hh.tran.position[0] << " " << hh.tran.position[1] << " "
                       << hh.tran.position[2] << " fill " << hh.fill << " (" << hh.fillVol << " cm3)" << char(10));
      }
    }
    LOGI("dimple tile " << ti << "/" << tiles.size() << ": " << seeds.size() << " seeds, placed so far " << placed << ", "
                        << clock.ElapsedMS() / 1000.0 << " s" << char(10));
  }
  LOGI("dimple trials " << c.nTrial << " penetrating " << c.nPen << " valid " << c.nOk << char(10));
  for (const auto &kv : perKind) LOGI("  " << kv.first << " " << kv.second << char(10));
  LOGI("dimple pass: tried " << tried << ", placed " << placed << char(10));
  return placed;
}

}  // namespace

bool RunCavityRefine(const PackingConfig &cfg, const std::vector<std::string> &mediumNames, bool metricOnly) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    scene.instances[scene.AddInstance(scene.exclusionItemIndex, identity)].isExclusion = true;
  }
  LoadPack(scene, cfg.ResumePackPath(), false);
  Ctx c(scene, cfg);
  c.cp.dx = cfg.cavDx;
  c.cp.band = cfg.cavBand;
  c.cp.R = cfg.cavR;
  c.cp.minClear = cfg.cavMinClear;
  c.cp.coneDeg = cfg.cavCone;
  c.cp.objClear = cfg.cavObjClear;
  c.maxX = cfg.sphereMaxX;
  c.isMedium.assign(scene.items.size(), 0);
  for (const std::string &n : mediumNames) {
    const unsigned k = scene.GetItemIndex(n);
    if (scene.items[k].name != n) continue;
    c.kinds.push_back(k);
    c.isMedium[k] = 1;
  }
  LOGI("cavity refine: " << c.kinds.size() << " medium kinds, " << scene.instances.size() << " instances" << char(10));
  if (!cfg.hullDir.empty()) {
    c.hulls.reset(new HullLibrary(cfg.dataDir + cfg.hullDir, cfg.hullInset, cfg.hullSampleSpacing));
  }
  ReportMetric(c, "before", scene.outputFolder + "/cavities_before.obj");
  if (metricOnly) return true;
  std::vector<unsigned> changed;
  for (unsigned pass = 0; pass < cfg.cavPasses; pass++) {
    LOGI("=== cavity pass " << pass << char(10));
    Utils::Stopwatch clock;
    clock.Start();
    if (cfg.cavCap || cfg.cavCapOnly) CapPass(c, changed);
    if (cfg.cavCapOnly) continue;
    if (cfg.cavDimple || cfg.cavDimpleOnly) {
      DimplePass(c, changed);
      scene.SaveInstances(scene.outputFolder + "/pack_cavity_dimple" + std::to_string(pass) + ".txt");
      if (cfg.cavDimpleOnly) {
        ReportMetric(c, ("after dimple " + std::to_string(pass)).c_str(), "");
        continue;
      }
    }
    scene.SaveInstances(scene.outputFolder + "/pack_cavity_cap" + std::to_string(pass) + ".txt");
    if (cfg.cavRelocate) RelocatePass(c, changed);
    LOGI("relocate " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
    scene.SaveInstances(scene.outputFolder + "/pack_cavity_reloc" + std::to_string(pass) + ".txt");
    clock.Start();
    SpawnPass(c, changed);
    LOGI("spawn " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
    scene.SaveInstances(scene.outputFolder + "/pack_cavity_" + std::to_string(pass) + ".txt");
    ReportMetric(c, ("after pass " + std::to_string(pass)).c_str(),
                 scene.outputFolder + "/cavities_after" + std::to_string(pass) + ".obj");
  }
  scene.SaveInstances(scene.outputFolder + "/pack_cavity.txt");
  SaveNudgedTrajectories(scene, changed, scene.outputFolder + "/traj_cavity.txt");
  return true;
}

bool RunStampCheck(const PackingConfig &cfg, const std::vector<std::string> &names) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  const float dx = cfg.cavDx;
  LOGI("stamp check at dx " << dx << ": sdf sign vs surface voxelization + flood fill" << char(10));
  for (const std::string &n : names) {
    const unsigned k = scene.GetItemIndex(n);
    if (scene.items[k].name != n) continue;
    scene.EnsureItemLoaded(k);
    scene.EnsureItemSamples(k);
    const MeshInfo &mi = scene.items[k];
    if (!mi.sdf) continue;
    VoxConf conf;
    conf.origin = mi.box.vmin - Vec3f(2 * dx, 2 * dx, 2 * dx);
    conf.unit = Vec3f(dx, dx, dx);
    const Vec3f ext = mi.box.vmax - mi.box.vmin + Vec3f(4 * dx, 4 * dx, 4 * dx);
    conf.gridSize = Vec3u(unsigned(std::ceil(ext[0] / dx)), unsigned(std::ceil(ext[1] / dx)), unsigned(std::ceil(ext[2] / dx)));
    Array3D8u vox(conf.gridSize, 0);
    cpu_voxelize_grid(conf, &mi.mesh, vox);
    size_t surf = 0;
    for (unsigned char v : vox.GetData()) surf += v != 0;
    FloodOutside8u(vox, 1, 2);
    size_t flood = 0, sdfN = 0, both = 0;
    for (unsigned z = 0; z < conf.gridSize[2]; z++)
      for (unsigned y = 0; y < conf.gridSize[1]; y++)
        for (unsigned x = 0; x < conf.gridSize[0]; x++) {
          const Vec3f p = conf.origin + dx * Vec3f(x + 0.5f, y + 0.5f, z + 0.5f);
          const bool a = mi.sdf->GetCoarseDist(p) < 0.0f;
          const bool b = vox(x, y, z) != 0;
          sdfN += a;
          flood += b;
          both += a && b;
        }
    const float v3 = dx * dx * dx;
    LOGI(n << ": mesh vol " << mi.rb.vol << " cm3, sdf " << sdfN * v3 << ", flood " << flood * v3 << ", surface shell "
           << surf * v3 << ", differ " << (sdfN + flood - 2 * both) * v3 << " cm3 ("
           << 100.0f * float(sdfN + flood - 2 * both) / float(std::max<size_t>(1, flood)) << " pct of flood)" << char(10));
  }
  return true;
}

bool RunCavityProbe(const PackingConfig &cfg, const std::string &kindName, const Vec3f &pos, const float q[4]) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    scene.instances[scene.AddInstance(scene.exclusionItemIndex, identity)].isExclusion = true;
  }
  LoadPack(scene, cfg.ResumePackPath(), false);
  const unsigned kind = scene.GetItemIndex(kindName);
  if (scene.items[kind].name != kindName) return false;
  scene.EnsureItemLoaded(kind);
  float w = q[0], x = q[1], y = q[2], z = q[3];
  const float n = std::sqrt(w * w + x * x + y * y + z * z);
  w /= n; x /= n; y /= n; z /= n;
  RigidTransform tr;
  tr.rotation = Matrix3f(1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
                         2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                         2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y));
  tr.position = pos;
  tr = scene.items[kind].rb.GetInertiaTran(tr);  // pack files hold the mesh-frame pose
  CavityParams cp;
  cp.dx = cfg.cavDx;
  cp.band = cfg.cavBand;
  cp.R = cfg.cavR;
  cp.minClear = cfg.cavMinClear;
  cp.coneDeg = cfg.cavCone;
  cp.objClear = cfg.cavObjClear;
  CavityWindow win;
  BuildCavityWindow(scene, AnchorBox(pos, 15.0f), cp, ~0u, win);
  float before = 0.0f;
  std::vector<CavityComp> cb = FindCavities(win, win.solid, cp, nullptr, &before, nullptr);
  Array3D8u solid = win.solid;
  unsigned added = 0;
  const unsigned already = StampFruit(scene, kind, tr, win, solid, &added);
  float after = 0.0f;
  std::vector<CavityComp> ca = FindCavities(win, solid, cp, nullptr, &after, nullptr);
  const float v3 = cp.dx * cp.dx * cp.dx;
  const float pen = Pen(scene, kind, tr, ~0u);
  LOGI("probe " << kindName << " vol " << scene.items[kind].rb.vol << " cm3: overlap " << already * v3 << " cm3, new "
                << added * v3 << " cm3, pen " << pen << " cm, sd at center " << scene.sdf->GetCoarseDist(pos) << char(10));
  LOGI("window cavity weighted " << before << " -> " << after << " (" << cb.size() << " -> " << ca.size()
                                 << " anchors)" << char(10));
  for (const CavityComp &cc : cb)
    LOGI("  anchor before at " << cc.deepest[0] << " " << cc.deepest[1] << " " << cc.deepest[2] << " clear "
                               << cc.clearance << " vol " << cc.volume << char(10));
  return true;
}
