#include "CavityRefine.h"

#include "AdapSDF.h"
#include "CavityField.h"
#include "HullNudge.h"
#include "Log.h"
#include "MediumGraph.h"
#include "MeshOps.h"
#include "PackingDriver.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
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
float Pen(PackingScene &scene, unsigned itemId, const RigidTransform &tran, unsigned skipId) {
  scene.EnsureItemSamples(itemId);
  const MeshInfo &mi = scene.items[itemId];
  Box3f box = ComputeBBox(MakeTransformedMesh(mi.mesh, tran).v);
  float pen = 0.0f;
  for (const SamplePoint &sp : mi.samples) {
    pen = std::max(pen, scene.sdf->GetCoarseDist(tran.rotation * sp.x + tran.position));
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
                      std::vector<RigidTransform> &traj) {
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
    const size_t steps = stage == 0 ? 40 : 160;
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
    const float pen = Pen(scene, kind, fin, selfId);
    if (c.nPen < 40 && pen > 0.4f) LOGI("pen " << pen << " kind " << mi.name << " clear " << a.clearance << " moved " << (fin.position - tr.position).norm() << " steps " << traj.size() << " already " << already << "/" << added << char(10));
    if (pen > 0.4f) {
      c.nPen++;
      continue;
    }
    c.nOk++;
    solid = baseSolid;
    StampFruit(scene, kind, fin, w, solid, &added);
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
    RelocatePass(c, changed);
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
