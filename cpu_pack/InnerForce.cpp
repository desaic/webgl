#include "InnerForce.h"

#include "AdapSDF.h"
#include "Log.h"
#include "MarchingCubes.h"
#include "HullNudge.h"
#include "MediumGraph.h"
#include "MeshOps.h"
#include "PackingDriver.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>

namespace {

const float kInf = 1e30f;

struct Offset {
  int d[3];
  float len;
};

const std::vector<Offset> &Neighbors26() {
  static std::vector<Offset> out;
  if (out.empty()) {
    for (int a = -1; a <= 1; a++)
      for (int b = -1; b <= 1; b++)
        for (int c = -1; c <= 1; c++) {
          if (!a && !b && !c) continue;
          out.push_back({{a, b, c}, std::sqrt(float(a * a + b * b + c * c))});
        }
  }
  return out;
}

}  // namespace

Vec3f InnerForceField::ForceAt(unsigned i, unsigned j, unsigned k) const {
  const float here = dist(i, j, k);
  if (here >= kInf || here <= 0.0f) return Vec3f(0, 0, 0);
  const Vec3u s = dist.GetSize();
  Vec3f sum(0, 0, 0);
  for (const Offset &o : Neighbors26()) {
    const int x = int(i) + o.d[0], y = int(j) + o.d[1], z = int(k) + o.d[2];
    if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
    const float n = dist(x, y, z);
    if (n >= kInf) continue;
    sum += ((here - n) / (o.len * o.len * dx)) * Vec3f(float(o.d[0]), float(o.d[1]), float(o.d[2]));
  }
  const float len = sum.norm();
  return len > 1e-6f ? sum / len : Vec3f(0, 0, 0);
}

Vec3f InnerForceField::Force(const Vec3f &p) const {
  const Vec3u s = dist.GetSize();
  const Vec3f l = (p - origin) / dx;
  if (l[0] < 0 || l[1] < 0 || l[2] < 0) return Vec3f(0, 0, 0);
  const unsigned i = unsigned(l[0]), j = unsigned(l[1]), k = unsigned(l[2]);
  if (i >= s[0] || j >= s[1] || k >= s[2]) return Vec3f(0, 0, 0);
  return ForceAt(i, j, k);
}

InnerForceField BuildInnerForceField(PackingScene &scene, unsigned instId, const AdapSDF &innerSdf,
                                     float dx, float margin) {
  Utils::Stopwatch clock;
  clock.Start();
  auto lap = [&](const char *what) {
    LOGI("  force field " << what << ": " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
    clock.Start();
  };
  InnerForceField f;
  f.dx = dx;
  const InstanceInfo self = scene.instances[instId];
  const Box3f fruitBox = ComputeBBox(MakeTransformedMesh(scene.items[self.itemId].mesh, self.tran).v);
  const Vec3f m(margin, margin, margin);
  f.origin = fruitBox.vmin - m;
  const Vec3f ext = (fruitBox.vmax + m) - f.origin;
  const Vec3u size(unsigned(std::ceil(ext[0] / dx)), unsigned(std::ceil(ext[1] / dx)),
                   unsigned(std::ceil(ext[2] / dx)));
  Box3f region;
  region.vmin = f.origin;
  region.vmax = f.origin + dx * Vec3f(float(size[0]), float(size[1]), float(size[2]));
  LOGI("  force field grid " << size[0] << " x " << size[1] << " x " << size[2] << " at dx " << dx
                             << char(10));

  Array3D8u occ(size, 0);
  // the outer container is ignored: its wall and outside do not block the field.
  // other instances (the fruit itself is not an obstacle)
  unsigned nOthers = 0;
  for (unsigned j : scene.broadPhase.GetNearby(region, 0.0f)) {
    if (j == instId) continue;
    const InstanceInfo &other = scene.instances[j];
    scene.EnsureItemSamples(other.itemId);
    const MeshInfo &oi = scene.items[other.itemId];
    if (!oi.sdf) continue;
    nOthers++;
    // the exclusion mesh sits at its authored pose, and transforming it every time is slow.
    const Box3f ob = other.isExclusion ? oi.box : ComputeBBox(MakeTransformedMesh(oi.mesh, other.tran).v);
    int lo[3], hi[3];
    for (int a = 0; a < 3; a++) {
      lo[a] = std::max(0, int(std::floor((ob.vmin[a] - f.origin[a]) / dx)));
      hi[a] = std::min(int(size[a]) - 1, int(std::floor((ob.vmax[a] - f.origin[a]) / dx)));
    }
    const Matrix3f inv = other.tran.rotation.transposed();
    for (int k = lo[2]; k <= hi[2]; k++)
      for (int jj = lo[1]; jj <= hi[1]; jj++)
        for (int i = lo[0]; i <= hi[0]; i++) {
          if (occ(i, jj, k)) continue;
          const Vec3f p = f.VoxelCenter(i, jj, k);
          if (oi.sdf->GetCoarseDist(inv * (p - other.tran.position)) < 0.0f) occ(i, jj, k) = 1;
        }
  }
  lap("other instances");
  LOGI("  force field obstacles from " << nOthers << " instances" << char(10));

  const Vec3f com = self.tran.position;
  const Vec3f cl = (com - f.origin) / dx;
  f.start = Vec3u(unsigned(std::clamp(int(cl[0]), 0, int(size[0]) - 1)),
                  unsigned(std::clamp(int(cl[1]), 0, int(size[1]) - 1)),
                  unsigned(std::clamp(int(cl[2]), 0, int(size[2]) - 1)));
  // a voxel is free unless another fruit or the exclusion mesh
  // covers it. the grid only guides the force; the fruit itself is moved by the simulation.
  f.allowed.Allocate(size, 0);
  for (size_t i = 0; i < occ.GetData().size(); i++) f.allowed.GetData()[i] = !occ.GetData()[i];

  // multi-source Dijkstra from the free voxels inside the inner container, through free voxels.
  // blocked voxels have no path and so no force.
  f.dist.Allocate(size, kInf);
  using QItem = std::pair<float, size_t>;
  std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> q;
  const size_t sx = size[0], sxy = size_t(size[0]) * size[1];
  for (unsigned k = 0; k < size[2]; k++)
    for (unsigned j = 0; j < size[1]; j++)
      for (unsigned i = 0; i < size[0]; i++) {
        if (!f.allowed(i, j, k)) continue;
        if (innerSdf.GetCoarseDist(f.VoxelCenter(i, j, k)) < 0.0f) {
          f.dist(i, j, k) = 0.0f;
          q.push({0.0f, i + j * sx + k * sxy});
          f.targets++;
        }
      }
  f.startInside = innerSdf.GetCoarseDist(com) < 0.0f;
  const std::vector<Offset> &nb = Neighbors26();
  while (!q.empty()) {
    const auto [d, idx] = q.top();
    q.pop();
    const unsigned k = unsigned(idx / sxy), j = unsigned((idx % sxy) / sx), i = unsigned(idx % sx);
    if (d > f.dist(i, j, k)) continue;
    for (const Offset &o : nb) {
      const int x = int(i) + o.d[0], y = int(j) + o.d[1], z = int(k) + o.d[2];
      if (x < 0 || y < 0 || z < 0 || x >= int(size[0]) || y >= int(size[1]) || z >= int(size[2])) continue;
      if (!f.allowed(x, y, z)) continue;
      const float nd = d + o.len * dx;
      if (nd < f.dist(x, y, z)) {
        f.dist(x, y, z) = nd;
        q.push({nd, x + y * sx + z * sxy});
      }
    }
  }
  lap("dijkstra");
  size_t reachable = 0;
  for (unsigned i = 0; i < size[0]; i++)
    for (unsigned j = 0; j < size[1]; j++)
      for (unsigned k = 0; k < size[2]; k++) reachable += f.dist(i, j, k) < kInf;
  LOGI("  free voxels with a route to the inner container: " << reachable << " of "
                                                              << size_t(size[0]) * size[1] * size[2] << char(10));
  const float startDist = f.dist(f.start[0], f.start[1], f.start[2]);
  LOGI("  force field: " << f.targets << " target voxels, start path length "
                         << (startDist >= kInf ? -1.0f : startDist)
                         << " cm, center of mass inside inner: " << int(f.startInside) << char(10));
  return f;
}

void SaveInnerForceObj(const InnerForceField &f, const std::string &filename, unsigned stride,
                       float lineLen) {
  std::ofstream out(filename);
  const Vec3u s = f.dist.GetSize();
  size_t nv = 0, nl = 0;
  auto line = [&](const Vec3f &a, const Vec3f &b) {
    out << "v " << a[0] << " " << a[1] << " " << a[2] << "\nv " << b[0] << " " << b[1] << " " << b[2]
        << "\nl " << nv + 1 << " " << nv + 2 << "\n";
    nv += 2;
    nl++;
  };
  stride = std::max(1u, stride);
  for (unsigned k = 0; k < s[2]; k += stride)
    for (unsigned j = 0; j < s[1]; j += stride)
      for (unsigned i = 0; i < s[0]; i += stride) {
        const Vec3f dir = f.ForceAt(i, j, k);
        if (dir.norm() < 0.5f) continue;
        const Vec3f p = f.VoxelCenter(i, j, k);
        line(p, p + lineLen * dir);
      }
  // shortest path from the start by steepest descent, as a polyline.
  std::vector<Vec3f> path;
  Vec3u c = f.start;
  const std::vector<Offset> &nb = Neighbors26();
  for (unsigned step = 0; step < 100000; step++) {
    path.push_back(f.VoxelCenter(c[0], c[1], c[2]));
    float best = f.dist(c[0], c[1], c[2]);
    Vec3u next = c;
    for (const Offset &o : nb) {
      const int x = int(c[0]) + o.d[0], y = int(c[1]) + o.d[1], z = int(c[2]) + o.d[2];
      if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
      if (f.dist(x, y, z) < best) {
        best = f.dist(x, y, z);
        next = Vec3u(x, y, z);
      }
    }
    if (next[0] == c[0] && next[1] == c[1] && next[2] == c[2]) break;
    c = next;
  }
  if (path.size() > 1) {
    const size_t first = nv + 1;
    for (const Vec3f &p : path) {
      out << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
      nv++;
    }
    out << "l";
    for (size_t i = 0; i < path.size(); i++) out << " " << first + i;
    out << "\n";
  }
  LOGI("saved " << filename << ": " << nl << " force lines, path of " << path.size() << " voxels"
                << char(10));
}


namespace {

Box3f WorldBox(const PackingScene &scene, const InstanceInfo &inst) {
  return ComputeBBox(MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran).v);
}

// builds the field for one instance and nudges it along it. returns the distance moved.
// hulls: if set, the nudge uses the convex hulls (NudgeWithHulls) instead of the triangle grids.
// fieldMs and nudgeMs accumulate the time spent building the field and nudging.
struct NudgeTimers {
  double fieldMs = 0.0, nudgeMs = 0.0;
  HullNudgeStats hull;
};

float NudgeInstanceInward(PackingScene &scene, unsigned id, const AdapSDF &innerSdf,
                          const PackingConfig &cfg, bool saveField, HullLibrary *hulls,
                          NudgeTimers &timers) {
  Utils::Stopwatch clock;
  clock.Start();
  const InnerForceField f = BuildInnerForceField(scene, id, innerSdf, cfg.forceGridDx,
                                                 cfg.forceGridMargin);
  timers.fieldMs += clock.ElapsedMS();
  clock.Start();
  if (saveField) {
    SaveInnerForceObj(f, scene.outputFolder + "/inner_force_" + scene.items[scene.instances[id].itemId].name + ".obj", 3, 1.2f);
  }
  InstanceInfo &inst = scene.instances[id];
  const unsigned kind = inst.itemId;
  const RigidTransform start = inst.tran;
  if (saveField) {
    // what touches the fruit: surface gap per neighbor and how much it lies along the force.
    const Vec3f fdir = f.Force(start.position);
    const MeshInfo &mi = scene.items[kind];
    float wall = 1e30f;
    for (const SamplePoint &sp : mi.samples) {
      wall = std::min(wall, -scene.sdf->GetCoarseDist(start.rotation * sp.x + start.position));
    }
    LOGI("  container wall gap " << wall << " cm" << char(10));
    size_t withForce = 0;
    for (const SamplePoint &sp : mi.samples) {
      withForce += f.Force(start.rotation * sp.x + start.position).norm() > 0.5f;
    }
    LOGI("  samples with a force: " << withForce << " of " << mi.samples.size() << char(10));
    for (unsigned j : scene.broadPhase.GetNearby(WorldBox(scene, inst), 1.0f)) {
      if (j == id) continue;
      const InstanceInfo &o = scene.instances[j];
      const MeshInfo &oi = scene.items[o.itemId];
      if (!oi.sdf) continue;
      const Matrix3f inv = o.tran.rotation.transposed();
      float gap = 1e30f;
      for (const SamplePoint &sp : mi.samples) {
        gap = std::min(gap, oi.sdf->GetCoarseDist(inv * (start.rotation * sp.x + start.position - o.tran.position)));
      }
      Vec3f toward = o.tran.position - start.position;
      toward.normalize();
      LOGI("  neighbor " << j << " " << oi.name << " gap " << gap << " cm, cos to force " << toward.dot(fdir)
                         << (o.isExclusion ? " (exclusion)" : "") << char(10));
    }
  }
  // out of the broad phase so the fruit does not collide with itself.
  scene.broadPhase.Remove(WorldBox(scene, inst), id);
  const std::function<Vec3f(const Vec3f &)> field = [&](const Vec3f &p) { return f.Force(p); };
  std::vector<RigidTransform> traj;
  clock.Start();
  const RigidTransform end =
      hulls ? NudgeWithHulls(scene, *hulls, kind, start, field, 400, traj, &timers.hull)
            : scene.Nudge(kind, start, Vec3f(0, 0, 0), 0.0f, traj, &field, 400);
  timers.nudgeMs += clock.ElapsedMS();
  inst.tran = end;
  if (inst.trajectory.empty()) inst.trajectory.push_back(start);
  inst.trajectory.insert(inst.trajectory.end(), traj.begin(), traj.end());
  scene.broadPhase.Add(WorldBox(scene, inst), id);
  return (end.position - start.position).norm();
}

// penetration of the moved fruit into their neighbors, measured with the mesh sdfs (independent
// of the hulls): the deepest sample of a fruit inside a neighbor, per moved fruit.
void ReportOverlaps(PackingScene &scene, const PackingConfig &cfg, const std::vector<unsigned> &ids) {
  std::vector<float> worst;
  for (unsigned id : ids) {
    int deepestNb = -1;
    const InstanceInfo &a = scene.instances[id];
    scene.EnsureItemSamples(a.itemId);
    float deepest = 0.0f;
    for (unsigned j : scene.broadPhase.GetNearby(WorldBox(scene, a), 0.0f)) {
      if (j == id) continue;
      const InstanceInfo &b = scene.instances[j];
      scene.EnsureItemSamples(b.itemId);
      const MeshInfo &ib = scene.items[b.itemId];
      if (!ib.sdf) continue;
      const Matrix3f bInv = b.tran.rotation.transposed();
      for (const SamplePoint &sp : scene.items[a.itemId].samples) {
        const float d = ib.sdf->GetCoarseDist(bInv * (a.tran.rotation * sp.x + a.tran.position - b.tran.position));
        if (d < deepest) {
          deepest = d;
          deepestNb = int(j);
        }
      }
    }
    worst.push_back(-deepest);
    if (-deepest > 1.0f) {
      // save the deep fruit: mesh and hulls at the final pose.
      const MeshInfo &mi = scene.items[a.itemId];
      const std::string base = scene.outputFolder + "/deep_" + mi.name + "_" + std::to_string(id);
      TrigMesh m = mi.mesh;
      for (size_t i = 0; i + 2 < m.v.size(); i += 3) {
        const Vec3f w = a.tran.rotation * Vec3f(m.v[i], m.v[i + 1], m.v[i + 2]) + a.tran.position;
        m.v[i] = w[0];
        m.v[i + 1] = w[1];
        m.v[i + 2] = w[2];
      }
      m.SaveObj(base + ".obj");
      SaveHullsObj(cfg.dataDir + cfg.hullDir + "/" + mi.name + ".hulls.txt", mi.rb, a.tran, base + "_hulls.obj");
      LOGI("deep fruit " << id << " " << mi.name << " at " << a.tran.position[0] << " " << a.tran.position[1] << " "
                        << a.tran.position[2] << ": " << -deepest << " cm into instance " << deepestNb << " "
                        << (deepestNb >= 0 ? scene.items[scene.instances[deepestNb].itemId].name : std::string())
                        << ", saved " << base << "(.obj, _hulls.obj)" << char(10));
    }
  }
  std::sort(worst.begin(), worst.end());
  unsigned over03 = 0, over05 = 0, over10 = 0;
  for (float w : worst) {
    over03 += w > 0.3f;
    over05 += w > 0.5f;
    over10 += w > 1.0f;
  }
  LOGI("overlap of " << ids.size() << " moved fruit: deepest " << (worst.empty() ? 0.0f : worst.back())
                     << " cm, median " << (worst.empty() ? 0.0f : worst[worst.size() / 2]) << " cm, "
                     << over03 << " deeper than 0.3 cm, " << over05 << " deeper than 0.5 cm, " << over10
                     << " deeper than 1 cm" << char(10));
}

}  // namespace

bool RunInnerForce(const PackingConfig &cfg, bool batch) {
  PackingScene scene;
  Utils::Stopwatch clock;
  clock.Start();
  auto lap = [&](const char *what) {
    LOGI("time " << what << ": " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
    clock.Start();
  };
  if (!BuildScene(scene, cfg, true)) return false;
  lap("build scene");
  if (scene.containerInner.mesh.v.empty()) {
    LOGI("no inner container loaded" << char(10));
    return false;
  }
  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    scene.instances[scene.AddInstance(scene.exclusionItemIndex, identity)].isExclusion = true;
  }
  LoadPack(scene, cfg.ResumePackPath(), false);
  lap("load pack");
  std::shared_ptr<AdapSDF> innerSdf = ComputeSDF(0.002f, 2.0f, scene.containerInner.mesh);
  lap("inner sdf");
  // convex hulls instead of the triangle grids for the nudge collision.
  std::unique_ptr<HullLibrary> hullLib;
  if (!cfg.hullDir.empty()) {
    hullLib.reset(new HullLibrary(cfg.dataDir + cfg.hullDir, cfg.hullInset, cfg.hullSampleSpacing));
    LOGI("nudging with convex hulls from " << cfg.dataDir + cfg.hullDir << ", inset " << cfg.hullInset
                                           << " cm" << char(10));
  }
  NudgeTimers timers;

  if (!batch) {
    const unsigned kind = scene.GetItemIndex(cfg.forceKind);
    if (scene.items[kind].name != cfg.forceKind) {
      LOGI("unknown kind " << cfg.forceKind << char(10));
      return false;
    }
    unsigned pick = ~0u;
    for (unsigned i = 0; i < scene.instances.size(); i++) {
      const InstanceInfo &inst = scene.instances[i];
      if (inst.itemId != kind) continue;
      if (pick == ~0u || inst.tran.position[0] < scene.instances[pick].tran.position[0]) pick = i;
    }
    if (pick == ~0u) {
      LOGI("no instance of " << cfg.forceKind << char(10));
      return false;
    }
    scene.EnsureItemSamples(kind);
    NudgeTimers timers;
    const float moved = NudgeInstanceInward(scene, pick, *innerSdf, cfg, true, hullLib.get(), timers);
    LOGI("instance " << pick << " moved " << moved << " cm" << char(10));
    SaveNudgedTrajectories(scene, {pick}, scene.outputFolder + "/traj_inner_force.txt");
    lap("nudge");
    return true;
  }

  // every fruit with its center of mass at x < sphereMaxX that is not already inside the inner
  // container. nearest to the inner container first, so those further out can follow.
  std::vector<unsigned> ids;
  for (unsigned i = 0; i < scene.instances.size(); i++) {
    const InstanceInfo &inst = scene.instances[i];
    if (inst.isExclusion || inst.tran.position[0] >= cfg.sphereMaxX) continue;
    ids.push_back(i);
  }
  for (unsigned id : ids) scene.EnsureItemSamples(scene.instances[id].itemId);
  // fruit whose surface comes within forceExclusionSkip of the exclusion mesh stay put.
  std::vector<char> nearExclusion(scene.instances.size(), 0);
  if (scene.exclusionItemIndex != ~0u) {
    scene.EnsureItemSamples(scene.exclusionItemIndex);
    const MeshInfo &ex = scene.items[scene.exclusionItemIndex];
    if (ex.sdf) {
      unsigned nearCount = 0;
      std::vector<unsigned> keep;
      for (unsigned id : ids) {
        const InstanceInfo &inst = scene.instances[id];
        float gap = 1e30f;
        for (const SamplePoint &sp : scene.items[inst.itemId].samples) {
          gap = std::min(gap, ex.sdf->GetCoarseDist(inst.tran.rotation * sp.x + inst.tran.position));
        }
        if (gap <= cfg.forceExclusionSkip) {
          nearExclusion[id] = 1;
          nearCount++;
        } else {
          keep.push_back(id);
        }
      }
      LOGI("inner nudge: skipping " << nearCount << " fruit within " << cfg.forceExclusionSkip
                                    << " cm of the exclusion mesh" << char(10));
      ids = keep;
    } else {
      LOGI("inner nudge: exclusion mesh has no sdf, nothing skipped" << char(10));
    }
  }
  LOGI("inner nudge: " << ids.size() << " fruit with center of mass x < " << cfg.sphereMaxX << char(10));
  std::vector<unsigned> everMoved;
  std::vector<char> movedFlag(scene.instances.size(), 0);
  for (unsigned pass = 0; pass < cfg.forcePasses; pass++) {
    std::vector<std::pair<float, unsigned>> order;
    unsigned fixed = 0;
    for (unsigned id : ids) {
      const float d = innerSdf->GetCoarseDist(scene.instances[id].tran.position);
      if (d < 0.0f) {
        fixed++;
        continue;
      }
      order.push_back({d, id});
    }
    std::sort(order.begin(), order.end());
    Utils::Stopwatch passClock;
    passClock.Start();
    float total = 0.0f;
    unsigned moved = 0;
    double nextReport = 5000.0;
    for (size_t k = 0; k < order.size(); k++) {
      const unsigned id = order[k].second;
      const float d = NudgeInstanceInward(scene, id, *innerSdf, cfg, false, hullLib.get(), timers);
      total += d;
      if (d > 0.3f) {
        moved++;
        if (!movedFlag[id]) {
          movedFlag[id] = 1;
          everMoved.push_back(id);
        }
      }
      const double ms = passClock.ElapsedMS();
      if (ms > nextReport) {
        nextReport = ms + 5000.0;
        LOGI("progress pass " << pass << " " << k + 1 << "/" << order.size() << ", elapsed " << ms / 1000.0
                              << " s, eta " << ms * (double(order.size()) / double(k + 1) - 1.0) / 1000.0
                              << " s" << char(10));
      }
    }
    LOGI("pass " << pass << ": " << fixed << " already inside, " << order.size() << " nudged, " << moved
                 << " moved > 0.3 cm, total " << total << " cm, " << passClock.ElapsedMS() / 1000.0 << " s"
                 << char(10));
    LOGI("  field build " << timers.fieldMs / 1000.0 << " s, nudge " << timers.nudgeMs / 1000.0 << " s");
    if (hullLib) {
      LOGI(", " << timers.hull.steps << " steps, " << timers.hull.pointTests << " point tests, contact gathering "
                << timers.hull.contactMs / 1000.0 << " s");
    }
    LOGI(char(10));
    timers = NudgeTimers();
    if (moved == 0) break;
  }
  ReportOverlaps(scene, cfg, everMoved);
  scene.SaveInstances(scene.outputFolder + "/pack_inner_nudged.txt");
  SaveNudgedTrajectories(scene, everMoved, scene.outputFolder + "/traj_inner_nudged.txt");
  lap("save");
  LOGI("inner nudge: " << everMoved.size() << " fruit moved" << char(10));
  return true;
}

bool RunInnerFree(const PackingConfig &cfg) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  if (scene.containerInner.mesh.v.empty()) {
    LOGI("no inner container loaded" << char(10));
    return false;
  }
  LOGI("inner container " << cfg.InnerContainerPath() << char(10));
  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    scene.instances[scene.AddInstance(scene.exclusionItemIndex, identity)].isExclusion = true;
  }
  LoadPack(scene, cfg.ResumePackPath(), false);
  std::shared_ptr<AdapSDF> innerSdf = ComputeSDF(0.002f, 2.0f, scene.containerInner.mesh);

  // grid over the inner container bounding box, cropped to x < sphereMaxX, padded by one voxel.
  const float dx = cfg.forceGridDx;
  Box3f box = ComputeBBox(scene.containerInner.mesh.v);
  box.vmax[0] = std::min(box.vmax[0], cfg.sphereMaxX);
  const Vec3f pad(dx, dx, dx);
  const Vec3f origin = box.vmin - pad;
  const Vec3f ext = (box.vmax + pad) - origin;
  const Vec3u size(unsigned(std::ceil(ext[0] / dx)), unsigned(std::ceil(ext[1] / dx)),
                   unsigned(std::ceil(ext[2] / dx)));
  Box3f region;
  region.vmin = origin;
  region.vmax = origin + dx * Vec3f(float(size[0]), float(size[1]), float(size[2]));
  LOGI("free voxel grid " << size[0] << " x " << size[1] << " x " << size[2] << " at dx " << dx << char(10));

  // free: inside the inner container and the outer container, not inside any fruit or the exclusion mesh.
  Array3D8u freeVox(size, 0);
  for (unsigned k = 0; k < size[2]; k++)
    for (unsigned j = 0; j < size[1]; j++)
      for (unsigned i = 0; i < size[0]; i++) {
        const Vec3f p = origin + dx * Vec3f(i + 0.5f, j + 0.5f, k + 0.5f);
        if (p[0] >= cfg.sphereMaxX) continue;
        if (innerSdf->GetCoarseDist(p) < 0.0f && (!scene.sdf || scene.sdf->GetCoarseDist(p) <= 0.0f)) freeVox(i, j, k) = 1;
      }
  size_t insideInner = 0;
  for (unsigned char v : freeVox.GetData()) insideInner += v;
  for (unsigned j : scene.broadPhase.GetNearby(region, 0.0f)) {
    const InstanceInfo &other = scene.instances[j];
    scene.EnsureItemSamples(other.itemId);
    const MeshInfo &oi = scene.items[other.itemId];
    if (!oi.sdf) continue;
    const Box3f ob = other.isExclusion ? oi.box : ComputeBBox(MakeTransformedMesh(oi.mesh, other.tran).v);
    int lo[3], hi[3];
    for (int a = 0; a < 3; a++) {
      lo[a] = std::max(0, int(std::floor((ob.vmin[a] - origin[a]) / dx)));
      hi[a] = std::min(int(size[a]) - 1, int(std::floor((ob.vmax[a] - origin[a]) / dx)));
    }
    const Matrix3f inv = other.tran.rotation.transposed();
    for (int k = lo[2]; k <= hi[2]; k++)
      for (int jj = lo[1]; jj <= hi[1]; jj++)
        for (int i = lo[0]; i <= hi[0]; i++) {
          if (!freeVox(i, jj, k)) continue;
          const Vec3f p = origin + dx * Vec3f(i + 0.5f, jj + 0.5f, k + 0.5f);
          if (oi.sdf->GetCoarseDist(inv * (p - other.tran.position)) < 0.0f) freeVox(i, jj, k) = 0;
        }
  }
  size_t freeCount = 0;
  for (unsigned char v : freeVox.GetData()) freeCount += v;
  LOGI("inner container voxels x < " << cfg.sphereMaxX << ": " << insideInner << " (" << insideInner * dx * dx * dx
                                     << " cm^3), free: " << freeCount << " (" << freeCount * dx * dx * dx
                                     << " cm^3, " << 100.0 * double(freeCount) / double(std::max<size_t>(1, insideInner))
                                     << " %)" << char(10));

  // blocky mesh of the free voxels, like the other debug mask meshes.
  Array3D<short> field;
  field.Allocate(size, 0);
  for (size_t i = 0; i < freeVox.GetData().size(); i++) field.GetData()[i] = freeVox.GetData()[i] ? short(100) : short(0);
  TrigMesh surf;
  MarchingCubes(field, 50.0f, 1.0f, dx, origin, &surf);
  const std::string out = scene.outputFolder + "/inner_free_voxels.obj";
  surf.SaveObj(out);
  LOGI("saved " << out << ": " << surf.GetNumTrigs() << " triangles" << char(10));
  return true;
}

bool RunHullCheck(const PackingConfig &cfg) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  HullLibrary lib(cfg.dataDir + cfg.hullDir, 0.0f, cfg.hullSampleSpacing);
  float worst = 0.0f;
  std::string worstName;
  unsigned n = 0;
  for (unsigned i = 0; i < scene.items.size(); i++) {
    if (i == scene.exclusionItemIndex) continue;
    scene.EnsureItemLoaded(i);
    const MeshInfo &item = scene.items[i];
    const HullSet *hs = lib.Get(i, item);
    if (!hs || !item.IsLoaded()) continue;
    const Box3f mb = ComputeBBox(item.mesh.v);
    float dev = 0.0f;
    for (int a = 0; a < 3; a++) {
      dev = std::max(dev, std::fabs(mb.vmin[a] - hs->box.vmin[a]));
      dev = std::max(dev, std::fabs(mb.vmax[a] - hs->box.vmax[a]));
    }
    // center of the hull box against the center of the mesh box, in the inertia frame.
    n++;
    if (dev > worst) {
      worst = dev;
      worstName = item.name;
    }
  }
  LOGI("hull check (inset 0): " << n << " kinds, hull box vs inertia frame mesh box, largest deviation "
                                << worst << " cm (" << worstName << ")" << char(10));
  return true;
}

bool RunHullPair(const PackingConfig &cfg, const std::string &packFile, const std::string &nameA,
                 const Vec3f &posA, const std::string &nameB, const Vec3f &posB) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  LoadPack(scene, packFile, false);
  auto find = [&](const std::string &name, const Vec3f &p) {
    int best = -1;
    float bd = 1e30f;
    for (unsigned i = 0; i < scene.instances.size(); i++) {
      const InstanceInfo &inst = scene.instances[i];
      if (inst.isExclusion || scene.items[inst.itemId].name != name) continue;
      const float d = (inst.tran.position - p).norm();
      if (d < bd) {
        bd = d;
        best = int(i);
      }
    }
    return best;
  };
  const int ia = find(nameA, posA), ib = find(nameB, posB);
  if (ia < 0 || ib < 0) {
    LOGI("instance not found" << char(10));
    return false;
  }
  for (int id : {ia, ib}) {
    const MeshInfo &mi = scene.items[scene.instances[id].itemId];
    const std::string out = scene.outputFolder + "/" + mi.name + "_hulls_" + std::to_string(id) + ".obj";
    LOGI((SaveHullsObj(cfg.dataDir + cfg.hullDir + "/" + mi.name + ".hulls.txt", mi.rb, scene.instances[id].tran, out)
              ? "saved " : "failed to save ")
         << out << char(10));
  }
  const float insets[2] = {0.0f, cfg.hullInset};
  for (float inset : insets) {
    HullLibrary lib(cfg.dataDir + cfg.hullDir, inset, cfg.hullSampleSpacing);
    const InstanceInfo &a = scene.instances[ia], &b = scene.instances[ib];
    const HullSet *ha = lib.Get(a.itemId, scene.items[a.itemId]);
    const HullSet *hb = lib.Get(b.itemId, scene.items[b.itemId]);
    if (!ha || !hb) return false;
    float deepest = 0.0f;
    unsigned inside = 0;
    for (int dir = 0; dir < 2; dir++) {
      const InstanceInfo &s = dir ? b : a, &t = dir ? a : b;
      const HullSet *hs = dir ? hb : ha, *ht = dir ? ha : hb;
      const Matrix3f tInv = t.tran.rotation.transposed();
      for (const SamplePoint &sp : hs->samples) {
        const Vec3f p = s.tran.rotation * sp.x + s.tran.position;
        const float d = ht->Distance(tInv * (p - t.tran.position), 1e3f);
        if (d < 0.0f) inside++;
        deepest = std::max(deepest, -d);
      }
    }
    LOGI("inset " << inset << ": " << scene.items[a.itemId].name << " (" << ia << ") vs " << scene.items[b.itemId].name
                  << " (" << ib << "): samples inside the other hull set " << inside << ", deepest " << deepest
                  << " cm, centers " << (a.tran.position - b.tran.position).norm() << " cm apart" << char(10));
  }
  return true;
}

namespace {

// closest point on triangle abc to p (Ericson).
Vec3f Scl(const Vec3f &v, float s) { return Vec3f(v[0] * s, v[1] * s, v[2] * s); }

Vec3f ClosestOnTrig(const Vec3f &p, const Vec3f &a, const Vec3f &b, const Vec3f &c) {
  const Vec3f ab = b - a, ac = c - a, ap = p - a;
  const float d1 = ab.dot(ap), d2 = ac.dot(ap);
  if (d1 <= 0 && d2 <= 0) return a;
  const Vec3f bp = p - b;
  const float d3 = ab.dot(bp), d4 = ac.dot(bp);
  if (d3 >= 0 && d4 <= d3) return b;
  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0) return a + Scl(ab, d1 / (d1 - d3));
  const Vec3f cp = p - c;
  const float d5 = ab.dot(cp), d6 = ac.dot(cp);
  if (d6 >= 0 && d5 <= d6) return c;
  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0) return a + Scl(ac, d2 / (d2 - d6));
  const float va = d3 * d6 - d5 * d4;
  if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return b + Scl(c - b, (d4 - d3) / ((d4 - d3) + (d5 - d6)));
  const float denom = 1.0f / (va + vb + vc);
  return a + Scl(ab, vb * denom) + Scl(ac, vc * denom);
}

// exact signed distance of p to a closed mesh by brute force. sign by ray parity along a fixed
// skew direction, so it does not depend on triangle normals.
float ExactSignedDist(const TrigMesh &m, const Vec3f &p) {
  float best2 = 1e30f;
  const Vec3f dir(0.5773f, 0.5774f, 0.5775f);
  int hits = 0;
  for (size_t i = 0; i < m.GetNumTrigs(); i++) {
    const Vec3f a = m.GetTriangleVertex(unsigned(i), 0), b = m.GetTriangleVertex(unsigned(i), 1),
                c = m.GetTriangleVertex(unsigned(i), 2);
    Vec3f q = ClosestOnTrig(p, a, b, c);
    Vec3f dq = q - p;
    best2 = std::min(best2, dq.dot(dq));
    // Moller-Trumbore
    const Vec3f e1 = b - a, e2 = c - a;
    const Vec3f pv = dir.cross(e2);
    const float det = e1.dot(pv);
    if (std::fabs(det) < 1e-12f) continue;
    const float inv = 1.0f / det;
    const Vec3f tv = p - a;
    const float u = tv.dot(pv) * inv;
    if (u < 0 || u > 1) continue;
    const Vec3f qv = tv.cross(e1);
    const float v = dir.dot(qv) * inv;
    if (v < 0 || u + v > 1) continue;
    if (e2.dot(qv) * inv > 0) hits++;
  }
  const float d = std::sqrt(best2);
  return (hits & 1) ? -d : d;
}

}  // namespace

bool RunSdfCheck(const PackingConfig &cfg, const std::string &packFile, const std::string &nameA,
                 const Vec3f &posA, const std::string &nameB, const Vec3f &posB) {
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  LoadPack(scene, packFile, false);
  auto find = [&](const std::string &name, const Vec3f &p) {
    if (name.compare(0, 3, "id:") == 0) return atoi(name.c_str() + 3);  // instance index of this scene
    int best = -1;
    float bd = 1e30f;
    for (unsigned i = 0; i < scene.instances.size(); i++) {
      const InstanceInfo &inst = scene.instances[i];
      if (inst.isExclusion || scene.items[inst.itemId].name != name) continue;
      const float d = (inst.tran.position - p).norm();
      if (d < bd) {
        bd = d;
        best = int(i);
      }
    }
    return best;
  };
  const int ia = find(nameA, posA), ib = find(nameB, posB);
  if (ia < 0 || ib < 0) return false;

  // 1. sdf against exact distance at random points near the surface of each kind.
  const int ids[2] = {ia, ib};
  for (int k = 0; k < 2; k++) {
    const unsigned item = scene.instances[ids[k]].itemId;
    scene.EnsureItemSamples(item);
    const MeshInfo &mi = scene.items[item];
    const Box3f box = ComputeBBox(mi.mesh.v);
    LOGI("sdf check kind " << mi.name << " trigs " << mi.mesh.GetNumTrigs() << " sdf " << (mi.sdf ? 1 : 0) << char(10));
    unsigned seed = 12345;
    auto rnd = [&]() {
      seed = seed * 1664525u + 1013904223u;
      return float(seed >> 8) / float(1 << 24);
    };
    // bands of exact signed distance: [-1,-0.5) [-0.5,0) [0,0.5) [0.5,2)
    const float lo[4] = {-1.0f, -0.5f, 0.0f, 0.5f}, hi[4] = {-0.5f, 0.0f, 0.5f, 2.0f};
    unsigned n[4] = {0, 0, 0, 0}, bad[4] = {0, 0, 0, 0};
    double sum[4] = {0, 0, 0, 0}, sumFine[4] = {0, 0, 0, 0};
    float mx[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4000; i++) {
      Vec3f p;
      for (int a = 0; a < 3; a++) p[a] = box.vmin[a] - 2.0f + rnd() * (box.vmax[a] - box.vmin[a] + 4.0f);
      const float ex = ExactSignedDist(mi.mesh, p);
      for (int b = 0; b < 4; b++) {
        if (ex < lo[b] || ex >= hi[b]) continue;
        const float c = mi.sdf->GetCoarseDist(p), f = c;
        n[b]++;
        sum[b] += std::fabs(c - ex);
        sumFine[b] += std::fabs(f - ex);
        mx[b] = std::max(mx[b], std::fabs(c - ex));
        bad[b] += (c < 0) != (ex < 0);
      }
    }
    for (int b = 0; b < 4; b++) {
      LOGI("sdf check " << mi.name << " exact in [" << lo[b] << "," << hi[b] << "): " << n[b] << " pts, coarse mean abs err "
                        << (n[b] ? sum[b] / n[b] : 0.0) << " max " << mx[b] << ", fine mean abs err "
                        << (n[b] ? sumFine[b] / n[b] : 0.0) << ", sign mismatches " << bad[b] << char(10));
    }
  }

  // 2. the pair: deepest mesh vertex of one inside the other, exact and by sdf.
  float exactDeep = 0.0f, sdfDeep = 0.0f;
  for (int dir = 0; dir < 2; dir++) {
    const InstanceInfo &s = scene.instances[ids[dir]], &t = scene.instances[ids[1 - dir]];
    const MeshInfo &ms = scene.items[s.itemId], &mt = scene.items[t.itemId];
    const Matrix3f tInv = t.tran.rotation.transposed();
    for (size_t i = 0; i + 2 < ms.mesh.v.size(); i += 3) {
      const Vec3f x(ms.mesh.v[i], ms.mesh.v[i + 1], ms.mesh.v[i + 2]);
      const Vec3f p = tInv * (s.tran.rotation * x + s.tran.position - t.tran.position);
      if (p[0] < mt.box.vmin[0] - 1 || p[0] > mt.box.vmax[0] + 1 || p[1] < mt.box.vmin[1] - 1 || p[1] > mt.box.vmax[1] + 1 ||
          p[2] < mt.box.vmin[2] - 1 || p[2] > mt.box.vmax[2] + 1)
        continue;
      sdfDeep = std::min(sdfDeep, mt.sdf->GetCoarseDist(p));
      const float c = mt.sdf->GetCoarseDist(p);
      if (c < 0.5f) exactDeep = std::min(exactDeep, ExactSignedDist(mt.mesh, p));
    }
  }
  LOGI("pair mesh vertices: deepest by sdf " << -sdfDeep << " cm, deepest exact " << -exactDeep << " cm" << char(10));
  return true;
}

bool RunDeepCompare(const PackingConfig &cfg, const std::vector<std::string> &packs,
                    const std::vector<unsigned> &ids, unsigned watermelon) {
  for (const std::string &packFile : packs) {
    PackingScene scene;
    if (!BuildScene(scene, cfg, true)) return false;
    if (scene.exclusionItemIndex != ~0u) {
      RigidTransform identity;
      scene.instances[scene.AddInstance(scene.exclusionItemIndex, identity)].isExclusion = true;
    }
    LoadPack(scene, packFile, false);
    HullLibrary lib(cfg.dataDir + cfg.hullDir, 0.0f, cfg.hullSampleSpacing);
    LOGI("pack " << packFile << char(10));
    for (unsigned id : ids) {
      const InstanceInfo &a = scene.instances[id];
      scene.EnsureItemSamples(a.itemId);
      const HullSet *ha = lib.Get(a.itemId, scene.items[a.itemId]);
      float meshDeep = 0, meshDeepMelon = 0, hullDeep = 0, hullDeepMelon = 0;
      int nbMesh = -1;
      std::vector<unsigned> near = scene.broadPhase.GetNearby(WorldBox(scene, a), 0.0f);
      if (std::find(near.begin(), near.end(), watermelon) == near.end()) near.push_back(watermelon);
      for (unsigned j : near) {
        if (j == id) continue;
        const InstanceInfo &b = scene.instances[j];
        if (b.isExclusion) continue;
        scene.EnsureItemSamples(b.itemId);
        const MeshInfo &ib = scene.items[b.itemId];
        const HullSet *hb = lib.Get(b.itemId, ib);
        const Matrix3f bInv = b.tran.rotation.transposed();
        float md = 0, hd = 0;
        for (const SamplePoint &sp : scene.items[a.itemId].samples) {
          const Vec3f p = bInv * (a.tran.rotation * sp.x + a.tran.position - b.tran.position);
          md = std::min(md, ib.sdf->GetCoarseDist(p));
        }
        if (ha && hb) {
          for (const SamplePoint &sp : ha->samples) {
            const Vec3f p = bInv * (a.tran.rotation * sp.x + a.tran.position - b.tran.position);
            hd = std::min(hd, hb->Distance(p, 1e3f));
          }
        }
        if (md < meshDeep) {
          meshDeep = md;
          nbMesh = int(j);
        }
        hullDeep = std::min(hullDeep, hd);
        if (j == watermelon) {
          meshDeepMelon = md;
          hullDeepMelon = hd;
        }
      }
      LOGI("  " << id << " " << scene.items[a.itemId].name << ": deepest mesh " << -meshDeep << " cm (into " << nbMesh
                << "), hull " << -hullDeep << " cm; into instance " << watermelon << ": mesh " << -meshDeepMelon
                << " cm, hull " << -hullDeepMelon << " cm" << char(10));
    }
  }
  return true;
}

bool RunFilterTouch(const PackingConfig &cfg, const std::string &packIn, const std::string &packOut, float tol,
                    unsigned minTouch) {
  // instances of the resume pack are the larger fruit, everything after them in packIn is new.
  unsigned firstNew = 0;
  {
    std::ifstream in(cfg.ResumePackPath());
    if (!(in >> firstNew)) return false;
  }
  PackingScene scene;
  if (!BuildScene(scene, cfg, true)) return false;
  LoadPack(scene, packIn, false);
  LOGI("filter: " << scene.instances.size() << " instances, first " << firstNew << " are the larger fruit" << char(10));
  if (scene.instances.size() < firstNew) return false;
  std::vector<char> keep(scene.instances.size(), 1);
  unsigned removed = 0;
  std::vector<unsigned> histogram(8, 0);
  for (unsigned id = firstNew; id < scene.instances.size(); id++) {
    const InstanceInfo &a = scene.instances[id];
    scene.EnsureItemSamples(a.itemId);
    unsigned touching = 0;
    for (unsigned j : scene.broadPhase.GetNearby(WorldBox(scene, a), tol)) {
      if (j >= firstNew) continue;
      const InstanceInfo &b = scene.instances[j];
      if (b.isExclusion) continue;
      scene.EnsureItemSamples(b.itemId);
      const MeshInfo &ib = scene.items[b.itemId];
      const Matrix3f bInv = b.tran.rotation.transposed();
      float gap = 1e30f;
      for (const SamplePoint &sp : scene.items[a.itemId].samples) {
        gap = std::min(gap, ib.sdf->GetCoarseDist(bInv * (a.tran.rotation * sp.x + a.tran.position - b.tran.position)));
        if (gap <= tol) break;
      }
      touching += gap <= tol;
    }
    histogram[std::min<unsigned>(touching, 7)]++;
    if (touching < minTouch) {
      keep[id] = 0;
      removed++;
    }
  }
  std::ifstream in(packIn);
  std::ofstream out(packOut);
  std::string line;
  std::getline(in, line);
  out << scene.instances.size() - removed << char(10);
  for (unsigned i = 0; std::getline(in, line); i++) {
    if (i < keep.size() && keep[i]) out << line << char(10);
  }
  LOGI("filter: touching larger fruit within " << tol << " cm, histogram 0.." << 7 << "+:");
  for (unsigned h : histogram) LOGI(" " << h);
  LOGI(char(10) << "filter: removed " << removed << " of " << scene.instances.size() - firstNew << " new fruit, wrote "
                << packOut << char(10));
  return true;
}
