#include "MediumGraph.h"

#include "AdapSDF.h"
#include "Log.h"
#include "MeshOps.h"
#include "PackingDriver.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <unordered_map>

namespace {

// smallest signed distance from a's sample points to b's surface (b's sdf is
// negative inside). large when far.
float SampleGap(PackingScene &scene, const InstanceInfo &a, const InstanceInfo &b) {
  const MeshInfo &ia = scene.items[a.itemId];
  const MeshInfo &ib = scene.items[b.itemId];
  if (ia.samples.empty() || !ib.sdf) return 1e30f;
  const Matrix3f bInv = b.tran.rotation.transposed();
  float best = 1e30f;
  for (const SamplePoint &sp : ia.samples) {
    const Vec3f w = a.tran.rotation * sp.x + a.tran.position;
    best = std::min(best, ib.sdf->GetCoarseDist(bInv * (w - b.tran.position)));
  }
  return best;
}

Vec3f InstCenter(const PackingScene &scene, const InstanceInfo &inst, float &radius) {
  const MeshInfo &it = scene.items[inst.itemId];
  const Vec3f c = 0.5f * (it.box.vmin + it.box.vmax);
  // bounding sphere about the bbox center: the farthest mesh vertex.
  static std::unordered_map<unsigned, float> cache;
  auto found = cache.find(inst.itemId);
  if (found == cache.end()) {
    float r2 = 0.0f;
    for (size_t i = 0; i + 2 < it.mesh.v.size(); i += 3) {
      const Vec3f d = Vec3f(it.mesh.v[i], it.mesh.v[i + 1], it.mesh.v[i + 2]) - c;
      r2 = std::max(r2, d.dot(d));
    }
    found = cache.emplace(inst.itemId, std::sqrt(r2)).first;
  }
  radius = found->second;
  return inst.tran.rotation * c + inst.tran.position;
}

// fraction of a's surface samples that lie inside b's volume (b's sdf is negative inside).
// the samples sit 0.2 cm inside a's own surface, so two identical overlaid fruit read as
// only 0.2 cm of penetration by SampleGap; this fraction reads them as fully overlapping.
float InsideFraction(PackingScene &scene, const InstanceInfo &a, const InstanceInfo &b) {
  const MeshInfo &ia = scene.items[a.itemId];
  const MeshInfo &ib = scene.items[b.itemId];
  if (ia.samples.empty() || !ib.sdf) return 0.0f;
  const Matrix3f bInv = b.tran.rotation.transposed();
  unsigned inside = 0;
  for (const SamplePoint &sp : ia.samples) {
    const Vec3f w = a.tran.rotation * sp.x + a.tran.position;
    inside += ib.sdf->GetCoarseDist(bInv * (w - b.tran.position)) < 0.0f;
  }
  return float(inside) / float(ia.samples.size());
}

// true if a fruit of this kind at tran stays inside the container and does not penetrate
// any instance currently in the broad phase (more than 0.3 cm).
bool PoseIsFree(PackingScene &scene, unsigned itemId, const RigidTransform &tran,
                std::string *why = nullptr) {
  InstanceInfo cand(itemId, tran);
  const Box3f box = ComputeBBox(MakeTransformedMesh(scene.items[itemId].mesh, tran).v);
  for (unsigned j : scene.broadPhase.GetNearby(box, 0.0f)) {
    const float gap = SampleGap(scene, cand, scene.instances[j]);
    if (gap < -0.3f) {
      if (why) *why = "overlaps instance " + std::to_string(j) + " (" +
                      scene.items[scene.instances[j].itemId].name + ") by " + std::to_string(-gap);
      return false;
    }
    const InstanceInfo &other = scene.instances[j];
    const float frac = std::max(InsideFraction(scene, cand, other), InsideFraction(scene, other, cand));
    if (frac > 0.15f) {
      if (why) *why = "instance " + std::to_string(j) + " (" + scene.items[other.itemId].name +
                      ") shares volume, " + std::to_string(frac) + " of samples inside";
      return false;
    }
  }
  if (scene.sdf) {
    for (const SamplePoint &sp : scene.items[itemId].samples) {
      const float d = scene.sdf->GetCoarseDist(tran.rotation * sp.x + tran.position);
      if (d > 0.3f) {
        if (why) *why = "outside container by " + std::to_string(d);
        return false;
      }
    }
  }
  return true;
}



// prints "what done/total, elapsed, eta" at most every few seconds.
struct Progress {
  const char *what;
  size_t total;
  Utils::Stopwatch clock;
  double lastMs = 0.0;
  Progress(const char *w, size_t t) : what(w), total(t) { clock.Start(); }
  void Tick(size_t done, bool force = false) {
    const double ms = clock.ElapsedMS();
    if (!force && ms - lastMs < 5000.0) return;
    lastMs = ms;
    const double eta = done > 0 ? ms * (double(total) / double(done) - 1.0) : 0.0;
    LOGI("progress " << what << " " << done << "/" << total << ", elapsed " << ms / 1000.0
                     << " s, eta " << eta / 1000.0 << " s" << char(10));
  }
};

}  // namespace

MediumGraph BuildMediumGraph(PackingScene &scene, const std::vector<std::string> &mediumNames,
                             float maxX, float maxGap) {
  MediumGraph g;
  std::set<unsigned> mediumKinds;
  for (const std::string &n : mediumNames) {
    auto it = scene.nameToIndex.find(n);
    if (it != scene.nameToIndex.end()) mediumKinds.insert(it->second);
  }
  // samples and sdf only for the kinds in and around the chunk, built on the fly. the margin
  // covers neighbors reached by a nudge or a spawn. the exclusion mesh is always included.
  const float margin = 30.0f;
  std::set<unsigned> needed;
  if (scene.exclusionItemIndex != ~0u) needed.insert(scene.exclusionItemIndex);
  for (const InstanceInfo &inst : scene.instances) {
    if (inst.tran.position[0] < maxX + margin) needed.insert(inst.itemId);
  }
  for (unsigned kind : needed) {
    scene.EnsureItemSamples(kind);
    scene.items[kind].ComputeSDFCached();
  }

  std::vector<int> nodeOf(scene.instances.size(), -1);
  for (unsigned i = 0; i < scene.instances.size(); i++) {
    const InstanceInfo &inst = scene.instances[i];
    if (!mediumKinds.count(inst.itemId)) continue;
    MediumGraph::Node n;
    n.instance = i;
    n.center = InstCenter(scene, inst, n.radius);
    if (n.center[0] >= maxX) continue;
    nodeOf[i] = int(g.nodes.size());
    g.nodes.push_back(n);
  }

  for (unsigned ni = 0; ni < g.nodes.size(); ni++) {
    MediumGraph::Node &n = g.nodes[ni];
    const InstanceInfo &a = scene.instances[n.instance];
    Box3f box;
    box.vmin = n.center - Vec3f(n.radius);
    box.vmax = n.center + Vec3f(n.radius);
    for (unsigned j : scene.broadPhase.GetNearby(box, maxGap)) {
      if (j == n.instance) continue;
      const InstanceInfo &b = scene.instances[j];
      float ra;
      const float d = (InstCenter(scene, b, ra) - n.center).norm();
      float rb = 0.0f;
      InstCenter(scene, b, rb);
      if (d > n.radius + rb + maxGap) continue;
      const float gap = std::min(SampleGap(scene, a, b), SampleGap(scene, b, a));
      if (gap > maxGap) continue;
      if (nodeOf[j] >= 0) {
        if (unsigned(nodeOf[j]) > ni) {
          MediumGraph::Edge e;
          e.a = ni;
          e.b = unsigned(nodeOf[j]);
          e.gap = gap;
          g.edges.push_back(e);
        }
      } else if (!mediumKinds.count(b.itemId)) {
        n.fixedNbrs++;
      }
    }
    if (scene.sdf) {
      for (const SamplePoint &sp : scene.items[a.itemId].samples) {
        if (scene.sdf->GetCoarseDist(a.tran.rotation * sp.x + a.tran.position) > -maxGap) {
          n.containerNbr = true;
          break;
        }
      }
    }
  }
  return g;
}

std::vector<std::vector<unsigned>> BuildMediumPaths(const MediumGraph &g) {
  std::vector<std::vector<unsigned>> nbrs(g.nodes.size());
  for (const MediumGraph::Edge &e : g.edges) {
    nbrs[e.a].push_back(e.b);
    nbrs[e.b].push_back(e.a);
  }
  for (auto &v : nbrs) std::sort(v.begin(), v.end());
  std::vector<char> used(g.nodes.size(), 0);
  auto freeDegree = [&](unsigned i) {
    unsigned d = 0;
    for (unsigned j : nbrs[i]) d += !used[j];
    return d;
  };
  std::vector<std::vector<unsigned>> paths;
  for (;;) {
    // start from the unused node with the fewest unused neighbors, so paths begin at leaves.
    int start = -1;
    unsigned best = ~0u;
    for (unsigned i = 0; i < g.nodes.size(); i++) {
      if (used[i]) continue;
      const unsigned d = freeDegree(i);
      if (d < best) {
        best = d;
        start = int(i);
      }
    }
    if (start < 0) break;
    std::vector<unsigned> path;
    for (int cur = start; cur >= 0;) {
      used[cur] = 1;
      path.push_back(unsigned(cur));
      int next = -1;
      for (unsigned j : nbrs[cur]) {
        if (!used[j]) {
          next = int(j);
          break;
        }
      }
      cur = next;
    }
    // the end with the lower x comes last: it is the end the fruit get pushed toward.
    if (g.nodes[path.front()].center[0] < g.nodes[path.back()].center[0]) {
      std::reverse(path.begin(), path.end());
    }
    paths.push_back(path);
  }
  return paths;
}

void SaveMediumPaths(const MediumGraph &g, const std::vector<std::vector<unsigned>> &paths,
                     const std::string &filename) {
  std::ofstream obj(filename);
  const char nl = 10;
  for (const MediumGraph::Node &n : g.nodes) {
    obj << "v " << n.center[0] << " " << n.center[1] << " " << n.center[2] << nl;
  }
  for (size_t p = 0; p < paths.size(); p++) {
    if (paths[p].size() < 2) continue;
    obj << "o path" << p << nl << "l";
    for (unsigned i : paths[p]) obj << " " << i + 1;
    obj << nl;
  }
}

unsigned NudgeAlongPaths(PackingScene &scene, MediumGraph &g,
                         const std::vector<std::vector<unsigned>> &paths, unsigned passes) {
  auto worldBox = [&](const InstanceInfo &inst) {
    return ComputeBBox(MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran).v);
  };
  // nudges node ni along dir, keeps the result unless it penetrates. returns distance moved.
  auto pushNode = [&](unsigned ni, Vec3f dir) -> float {
    MediumGraph::Node &n = g.nodes[ni];
    const unsigned id = n.instance;
    dir.normalize();
    const RigidTransform start = scene.instances[id].tran;
    scene.broadPhase.Remove(worldBox(scene.instances[id]), id);
    std::vector<RigidTransform> traj;
    RigidTransform end = scene.Nudge(scene.instances[id].itemId, start, dir, 1.0f, traj);

    // reject a settle that ends inside a neighbor or outside the container.
    std::string why;
    const bool ok = PoseIsFree(scene, scene.instances[id].itemId, end, &why);
    float dist = 0.0f;
    if (ok) {
      scene.instances[id].tran = end;
      std::vector<RigidTransform> &all = scene.instances[id].trajectory;
      if (all.empty()) all.push_back(start);
      all.insert(all.end(), traj.begin(), traj.end());
      dist = (end.position - start.position).norm();
    }
    scene.broadPhase.Add(worldBox(scene.instances[id]), id);
    float r;
    n.center = InstCenter(scene, scene.instances[id], r);
    if (!ok) LOGI("  node " << ni << " rejected: " << why << char(10));
    return dist;
  };

  unsigned moved = 0;
  size_t pushable = 0;
  for (const auto &path : paths) pushable += path.size() >= 2 ? path.size() : 0;
  Progress progress("nudge", pushable * passes);
  size_t pushed = 0;
  for (unsigned pass = 0; pass < passes; pass++) {
    float passDist = 0.0f;
    for (size_t p = 0; p < paths.size(); p++) {
      const std::vector<unsigned> &path = paths[p];
      if (path.size() < 2) continue;
      // last fruit first, along the direction the path was heading.
      const size_t last = path.size() - 1;
      float d = pushNode(path[last], g.nodes[path[last]].center - g.nodes[path[last - 1]].center);
      // then each earlier fruit toward its successor's new position.
      for (size_t k = last; k-- > 0;) {
        d += pushNode(path[k], g.nodes[path[k + 1]].center - g.nodes[path[k]].center);
      }
      passDist += d;
      pushed += path.size();
      progress.Tick(pushed);
      LOGI("  pass " << pass << " path " << p << " (" << path.size() << " fruit) moved " << d << " cm\n");
    }
    moved += 1;
    if (passDist < 0.2f) break;
  }
  return moved;
}


unsigned SpawnAlongPaths(PackingScene &scene, const MediumGraph &g,
                         const std::vector<std::vector<unsigned>> &paths,
                         const std::vector<RigidTransform> &origTran,
                         std::vector<unsigned> &spawned) {
  const unsigned maxPerVertex = 30;
  size_t vertices = 0;
  for (const auto &path : paths) vertices += path.size() >= 2 ? path.size() : 0;
  Progress progress("spawn", vertices);
  size_t visited = 0;
  for (size_t p = 0; p < paths.size(); p++) {
    const std::vector<unsigned> &path = paths[p];
    if (path.size() < 2) continue;
    const Vec3f packedEnd = g.nodes[path.back()].center;
    const Vec3f tangent = packedEnd - g.nodes[path[path.size() - 2]].center;
    unsigned pathCount = 0;
    // from the vertex next to the packed end back toward the path start.
    for (size_t k = path.size(); k-- > 0;) {
      const unsigned node = path[k];
      const unsigned itemId = scene.instances[g.nodes[node].instance].itemId;
      const RigidTransform pose = origTran[node];
      unsigned count = 0;
      for (; count < maxPerVertex; count++) {
        std::string why;
        const RigidTransform &start = pose;
        if (!PoseIsFree(scene, itemId, start, &why)) {
          if (k == 0 || count > 0) LOGI("    vertex " << k << " try " << count << ": " << why << char(10));
          break;
        }
        float r;
        Vec3f dir = k + 1 < path.size()
                        ? g.nodes[path[k + 1]].center - InstCenter(scene, InstanceInfo(itemId, start), r)
                        : tangent;
        dir.normalize();
        std::vector<RigidTransform> traj;
        const RigidTransform end = scene.Nudge(itemId, start, dir, 1.0f, traj);
        if (!PoseIsFree(scene, itemId, end, &why)) {
          LOGI("    vertex " << k << " settle rejected: " << why << char(10));
          break;
        }
        // a spawn that cannot travel far does not open any space: leave it at its start pose.
        const float travel = (end.position - start.position).norm();
        const bool farEnough = travel >= 0.9f * 2.0f * g.nodes[node].radius;
        const unsigned id = scene.Put(itemId, farEnough ? end : start);
        traj.insert(traj.begin(), start);
        // a snapped-back spawn still records the nudge, ending at the start pose.
        if (!farEnough) traj.push_back(start);
        scene.instances[id].trajectory = traj;
        if (!farEnough) {
          LOGI("    vertex " << k << " spawn moved only " << travel << " cm, kept at its start pose" << char(10));
        }
        spawned.push_back(id);
      }
      pathCount += count;
      progress.Tick(++visited);
      LOGI("  path " << p << " vertex " << k << " (" << scene.items[itemId].name << "): spawned "
                     << count << "\n");
    }
    LOGI("  path " << p << ": " << pathCount << " new fruit\n");
  }
  return unsigned(spawned.size());
}

unsigned RevertShortMoves(PackingScene &scene, MediumGraph &g,
                          const std::vector<std::vector<unsigned>> &paths,
                          const std::vector<RigidTransform> &origTran, float diameterFraction) {
  unsigned reverted = 0;
  for (size_t p = 0; p < paths.size(); p++) {
    if (paths[p].size() < 2) continue;
    const unsigned ni = paths[p].front();
    MediumGraph::Node &n = g.nodes[ni];
    const unsigned id = n.instance;
    InstanceInfo &inst = scene.instances[id];
    const float travel = (inst.tran.position - origTran[ni].position).norm();
    const float limit = diameterFraction * 2.0f * n.radius;
    if (travel >= limit) continue;
    auto worldBox = [&]() {
      return ComputeBBox(MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran).v);
    };
    scene.broadPhase.Remove(worldBox(), id);
    inst.tran = origTran[ni];
    // keep the recorded motion and end it at the original pose.
    if (!inst.trajectory.empty()) inst.trajectory.push_back(origTran[ni]);
    scene.broadPhase.Add(worldBox(), id);
    float r;
    n.center = InstCenter(scene, inst, r);
    reverted++;
    LOGI("  path " << p << " end node " << ni << " moved " << travel << " cm < " << limit
                   << ", put back at its original pose" << char(10));
  }
  return reverted;
}

void SaveNudgedTrajectories(const PackingScene &scene, const std::vector<unsigned> &ids,
                            const std::string &filename) {
  std::ofstream out(filename);
  const char nl = 10;
  for (unsigned id : ids) {
    const InstanceInfo &inst = scene.instances[id];
    if (inst.trajectory.empty()) continue;
    const MeshInfo &item = scene.items[inst.itemId];
    out << "Instance " << id << " Mesh " << item.filePath << nl;
    for (size_t j = 0; j < inst.trajectory.size(); j++) {
      RigidTransform t = inst.trajectory[j];
      out << "Step " << j << " " << item.rb.GetInputTran(t).toString() << nl;
    }
    RigidTransform t = inst.tran;
    out << "Final " << item.rb.GetInputTran(t).toString() << nl << nl;
  }
}

void SaveMediumGraph(const PackingScene &scene, const MediumGraph &g, const std::string &prefix) {
  std::ofstream txt(prefix + ".txt");
  txt << "nodes " << g.nodes.size() << " edges " << g.edges.size() << "\n";
  for (unsigned i = 0; i < g.nodes.size(); i++) {
    const MediumGraph::Node &n = g.nodes[i];
    txt << "node " << i << " " << scene.items[scene.instances[n.instance].itemId].name
        << " inst " << n.instance << " center " << n.center[0] << " " << n.center[1] << " "
        << n.center[2] << " radius " << n.radius << " fixed " << n.fixedNbrs << " container "
        << int(n.containerNbr) << "\n";
  }
  for (const MediumGraph::Edge &e : g.edges) {
    txt << "edge " << e.a << " " << e.b << " gap " << e.gap << "\n";
  }
  std::ofstream obj(prefix + ".obj");
  for (const MediumGraph::Node &n : g.nodes) {
    obj << "v " << n.center[0] << " " << n.center[1] << " " << n.center[2] << "\n";
  }
  for (const MediumGraph::Edge &e : g.edges) {
    obj << "l " << e.a + 1 << " " << e.b + 1 << "\n";
  }
}

bool RunMediumGraph(const PackingConfig &cfg, const std::vector<std::string> &mediumNames, bool refine) {
  PackingScene scene;
  Utils::Stopwatch clock;
  clock.Start();
  auto lap = [&](const char *what) {
    LOGI("time " << what << ": " << clock.ElapsedMS() / 1000.0 << " s" << char(10));
    clock.Start();
  };
  if (!BuildScene(scene, cfg)) return false;
  lap("build scene");
  // collision runs on the trig grids and per-kind sdfs only: no voxel grid is allocated.
  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    scene.instances[scene.AddInstance(scene.exclusionItemIndex, identity)].isExclusion = true;
  }
  LoadPack(scene, cfg.ResumePackPath(), false);
  lap("load pack");
  if (scene.instances.empty()) {
    LOGI("no instances loaded from " << cfg.ResumePackPath() << "\n");
    return false;
  }
  MediumGraph g = BuildMediumGraph(scene, mediumNames, cfg.sphereMaxX, cfg.graphGap);
  lap("build graph");
  const std::string prefix = scene.outputFolder + "/medium_graph";
  SaveMediumGraph(scene, g, prefix);
  unsigned isolated = 0, deg1 = 0;
  std::vector<unsigned> deg(g.nodes.size(), 0);
  for (const MediumGraph::Edge &e : g.edges) {
    deg[e.a]++;
    deg[e.b]++;
  }
  for (unsigned d : deg) {
    isolated += d == 0;
    deg1 += d == 1;
  }
  LOGI("medium graph: " << g.nodes.size() << " nodes, " << g.edges.size() << " edges, "
                        << isolated << " isolated, " << deg1 << " leaves, saved " << prefix
                        << ".txt/.obj\n");
  if (refine) {
    const auto paths = BuildMediumPaths(g);
    {
      size_t longest = 0, multi = 0, single = 0, over20 = 0;
      for (const auto &p : paths) {
        longest = std::max(longest, p.size());
        multi += p.size() >= 2;
        single += p.size() < 2;
        over20 += p.size() > 20;
      }
      LOGI("paths: " << paths.size() << " (" << multi << " with 2+ fruit, " << single
                     << " singletons), longest " << longest << ", " << over20 << " longer than 20"
                     << char(10));
    }
    SaveMediumPaths(g, paths, scene.outputFolder + "/medium_paths.obj");
    std::vector<RigidTransform> origTran;
    std::vector<unsigned> ids;
    for (const MediumGraph::Node &n : g.nodes) {
      origTran.push_back(scene.instances[n.instance].tran);
      ids.push_back(n.instance);
    }
    lap("paths and save");
    unsigned moved = NudgeAlongPaths(scene, g, paths, 3);
    lap("nudge along paths");
    RevertShortMoves(scene, g, paths, origTran, 0.9f);
    lap("revert short moves");
    const unsigned spawnedCount = SpawnAlongPaths(scene, g, paths, origTran, ids);
    LOGI("spawned " << spawnedCount - g.nodes.size() << " new fruit" << char(10));
    lap("spawn");
    MediumGraph g2 = BuildMediumGraph(scene, mediumNames, cfg.sphereMaxX, cfg.graphGap);
    lap("rebuild graph");
    SaveMediumGraph(scene, g2, scene.outputFolder + "/medium_graph_nudged");
    scene.SaveInstances(scene.outputFolder + "/pack_nudged.txt");
    SaveNudgedTrajectories(scene, ids, scene.outputFolder + "/traj_nudged.txt");
    lap("save outputs");
    LOGI("nudged " << moved << " passes, " << g2.edges.size() << " edges after\n");
  }
  return true;
}
