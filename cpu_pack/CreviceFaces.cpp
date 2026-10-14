#include "CreviceFaces.h"

#include "AdapSDF.h"
#include "Log.h"
#include "QuickHull.hpp"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace {

const float kInf = std::numeric_limits<float>::max();

// signed distance from world point p to instance id (negative inside).
float InstanceDist(const PackingScene &scene, unsigned id, const Vec3f &p) {
  const auto &inst = scene.instances[id];
  const auto &item = scene.items[inst.itemId];
  const float scale = std::max(0.001f, inst.tran.scale);
  const Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position) / scale;
  return scale * item.sdf->GetCoarseDist(local);
}

// smallest signed distance from p to any fruit within range.
float MinFruitDist(const PackingScene &scene, const Vec3f &p, float range) {
  Box3f q;
  q.vmin = q.vmax = p;
  float best = kInf;
  for (unsigned id : scene.broadPhase.GetNearby(q, range)) {
    const auto &inst = scene.instances[id];
    if (inst.isExclusion || !scene.items[inst.itemId].sdf) continue;
    best = std::min(best, InstanceDist(scene, id, p));
  }
  return best;
}

// distance along dir from p to the first fruit surface, sphere traced; cap if none.
float RayToFruit(const PackingScene &scene, const Vec3f &p, const Vec3f &dir, float cap) {
  float s = 0.0f;
  while (s < cap) {
    const float d = MinFruitDist(scene, p + s * dir, 4.0f);
    if (d <= 0.05f) return s;
    s += std::max(0.1f, std::min(d, 4.0f));
  }
  return cap;
}

struct Face {
  Vec3f v[3];
  Vec3f n;
  float depth = 0.0f;
  float surfaceDist = 0.0f;
  unsigned group = 0;
  std::vector<unsigned> targetIds;
};

} // namespace

void ExportCreviceFaces(PackingScene &scene, const PackingConfig &cfg,
                        const std::vector<Vec3f> &targets) {
  Utils::Stopwatch clock;
  clock.Start();
  // fruit and their world vertex samples.
  std::vector<unsigned> fruit;
  for (unsigned id = 0; id < scene.instances.size(); ++id) {
    const auto &inst = scene.instances[id];
    if (!inst.isExclusion && scene.items[inst.itemId].sdf) fruit.push_back(id);
  }
  const unsigned nf = unsigned(fruit.size());
  std::vector<std::vector<Vec3f>> samples(nf);
  std::vector<Box3f> boxes(nf);
  for (unsigned f = 0; f < nf; ++f) {
    const auto &inst = scene.instances[fruit[f]];
    const TrigMesh &m = scene.items[inst.itemId].mesh;
    const size_t nv = m.GetNumVerts();
    const size_t stride = std::max<size_t>(1, nv / std::max(1u, cfg.hullSamples));
    Box3f b;
    b.vmin = Vec3f(kInf, kInf, kInf);
    b.vmax = Vec3f(-kInf, -kInf, -kInf);
    for (size_t i = 0; i < nv; i += stride) {
      const Vec3f w = inst.tran.rotation * (inst.tran.scale * m.GetVertex(unsigned(i))) + inst.tran.position;
      samples[f].push_back(w);
      for (int c = 0; c < 3; ++c) {
        b.vmin[c] = std::min(b.vmin[c], w[c]);
        b.vmax[c] = std::max(b.vmax[c], w[c]);
      }
    }
    boxes[f] = b;
  }
  std::vector<int> fruitOf(scene.instances.size(), -1);
  for (unsigned f = 0; f < nf; ++f) fruitOf[fruit[f]] = int(f);

  // 1. A tight graph supplies triangle/four-cycle groups. Sparse wider edges
  // supply two-fruit mouths that the tight graph cannot represent.
  std::vector<std::vector<unsigned>> adj(nf);
  struct Pair { unsigned f, g; float gap; bool selected = false; };
  std::vector<Pair> pairs;
  std::vector<std::vector<std::pair<float, size_t>>> ranked(nf);
  unsigned edges = 0;
  for (unsigned f = 0; f < nf; ++f) {
    for (unsigned id : scene.broadPhase.GetNearby(boxes[f], cfg.hullBridgeGap)) {
      const int g = fruitOf[id];
      if (g < 0 || unsigned(g) <= f) continue;
      float gap = kInf;
      for (const Vec3f &s : samples[f]) gap = std::min(gap, InstanceDist(scene, fruit[g], s));
      for (const Vec3f &s : samples[g]) gap = std::min(gap, InstanceDist(scene, fruit[f], s));
      if (gap > cfg.hullBridgeGap) continue;
      const size_t pi = pairs.size();
      pairs.push_back({f, unsigned(g), gap});
      ranked[f].push_back({gap, pi});
      ranked[g].push_back({gap, pi});
      if (gap <= cfg.hullGap) {
        adj[f].push_back(unsigned(g));
        adj[g].push_back(f);
        ++edges;
      }
    }
  }
  for (auto &r : ranked) {
    std::sort(r.begin(), r.end());
    for (size_t i = 0; i < std::min<size_t>(r.size(), cfg.hullMaxNeighbors); ++i)
      pairs[r[i].second].selected = true;
  }
  for (auto &a : adj) std::sort(a.begin(), a.end());
  {
    // debug: one vertex per fruit at its center of mass, one line per selected edge.
    std::ofstream gobj(scene.outputFolder + "/fruit_graph.obj");
    for (unsigned f = 0; f < nf; ++f) {
      const Vec3f &c = scene.instances[fruit[f]].tran.position;
      gobj << "v " << c[0] << ' ' << c[1] << ' ' << c[2] << "\n";
    }
    for (const Pair &p : pairs)
      if (p.selected || p.gap <= cfg.hullGap)
        gobj << "l " << p.f + 1 << ' ' << p.g + 1 << "\n";
  }
  auto linked = [&](unsigned a, unsigned b) {
    return std::binary_search(adj[a].begin(), adj[a].end(), b);
  };

  // 2. Pair groups span a two-fruit groove; only tight edges form higher-order
  // groups, preventing combinatorial growth and broad arbitrary arches.
  std::vector<std::vector<unsigned>> groups;
  unsigned pairGroups = 0;
  for (const Pair &p : pairs) if (p.selected || p.gap <= cfg.hullGap) {
    groups.push_back({p.f, p.g});
    ++pairGroups;
  }
  unsigned tris = 0, quads = 0;
  for (unsigned a = 0; a < nf; ++a)
    for (unsigned b : adj[a]) {
      if (b <= a) continue;
      for (unsigned c : adj[b])
        if (c > b && linked(a, c)) {
          groups.push_back({a, b, c});
          ++tris;
        }
    }
  {
    std::set<std::vector<unsigned>> seen;
    for (unsigned a = 0; a < nf; ++a)
      for (unsigned c = a + 1; c < nf; ++c) {
        if (linked(a, c)) continue;
        std::vector<unsigned> common;
        std::set_intersection(adj[a].begin(), adj[a].end(), adj[c].begin(), adj[c].end(),
                              std::back_inserter(common));
        for (size_t i = 0; i < common.size(); ++i)
          for (size_t j = i + 1; j < common.size(); ++j) {
            const unsigned b = common[i], d = common[j];
            if (linked(b, d)) continue;
            std::vector<unsigned> key = {a, b, c, d};
            std::sort(key.begin(), key.end());
            if (!seen.insert(key).second) continue;
            groups.push_back(key);
            ++quads;
          }
      }
  }

  std::shared_ptr<AdapSDF> innerSdf;
  if (!scene.containerInner.mesh.v.empty()) innerSdf = ComputeSDF(0.002f, 2.0f, scene.containerInner.mesh);
  const AdapSDF *excl = nullptr;
  if (scene.exclusionItemIndex != ~0u && scene.items[scene.exclusionItemIndex].sdf)
    excl = scene.items[scene.exclusionItemIndex].sdf.get();

  // Target buckets make the mouth relevance test local rather than scanning
  // every lattice point for every hull patch.
  const float targetCell = 3.0f;
  auto targetKey = [&](const Vec3f &p) {
    return std::make_tuple(int(std::floor(p[0] / targetCell)),
        int(std::floor(p[1] / targetCell)), int(std::floor(p[2] / targetCell)));
  };
  std::map<std::tuple<int, int, int>, std::vector<unsigned>> targetBins;
  for (unsigned i = 0; i < targets.size(); ++i)
    targetBins[targetKey(targets[i])].push_back(i);
  std::vector<unsigned> targetSeen(targets.size(), 0);
  unsigned targetStamp = 0;

  // 3-5. Subdivide hull triangles and validate each small mouth patch. A
  // single deep ray must not keep an otherwise internal 200 cm2 triangle.
  std::vector<Face> faces;
  std::ofstream rawObj(scene.outputFolder + "/crevice_faces_raw_bridges.obj");
  size_t rawVerts = 0;
  unsigned nBridging = 0, nPatches = 0, nInner = 0, nExcl = 0;
  unsigned nHidden = 0, nShallow = 0, nDup = 0, nFruit = 0, nSupport = 0, nNoTargets = 0;
  const float patchSize = std::max(0.5f, cfg.hullPatchSize);
  for (unsigned gi = 0; gi < groups.size(); ++gi) {
    const auto &grp = groups[gi];
    std::vector<quickhull::Vector3<float>> cloud;
    std::vector<unsigned> owner;
    Vec3f centroid(0, 0, 0);
    for (unsigned f : grp)
      for (const Vec3f &s : samples[f]) {
        cloud.emplace_back(s[0], s[1], s[2]);
        owner.push_back(f);
        centroid = centroid + s;
      }
    centroid = centroid * (1.0f / float(cloud.size()));
    quickhull::QuickHull<float> qh;
    auto hull = qh.getConvexHull(cloud, true, true);
    const auto &idx = hull.getIndexBuffer();
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
      const unsigned i0 = unsigned(idx[t]), i1 = unsigned(idx[t + 1]), i2 = unsigned(idx[t + 2]);
      if (owner[i0] == owner[i1] && owner[i1] == owner[i2]) continue;
      ++nBridging;
      Face fc;
      fc.v[0] = Vec3f(cloud[i0].x, cloud[i0].y, cloud[i0].z);
      fc.v[1] = Vec3f(cloud[i1].x, cloud[i1].y, cloud[i1].z);
      fc.v[2] = Vec3f(cloud[i2].x, cloud[i2].y, cloud[i2].z);
      const Vec3f fcen = (fc.v[0] + fc.v[1] + fc.v[2]) * (1.0f / 3.0f);
      Vec3f n = (fc.v[1] - fc.v[0]).cross(fc.v[2] - fc.v[0]);
      if (n.norm2() < 1e-10f) continue;
      n.normalize();
      if (n.dot(fcen - centroid) < 0.0f) {
        n = -1.0f * n;
        std::swap(fc.v[1], fc.v[2]);
      }
      for (const Vec3f &v : fc.v)
        rawObj << "v " << v[0] << ' ' << v[1] << ' ' << v[2] << "\n";
      rawObj << "f " << rawVerts + 1 << ' ' << rawVerts + 2 << ' ' << rawVerts + 3 << "\n";
      rawVerts += 3;
      const float longest = std::sqrt(std::max({(fc.v[1] - fc.v[0]).norm2(),
          (fc.v[2] - fc.v[1]).norm2(), (fc.v[0] - fc.v[2]).norm2()}));
      const int divisions = std::clamp(int(std::ceil(longest / patchSize)), 1, 32);
      auto gridPoint = [&](int a, int b) {
        return fc.v[0] + (float(a) / divisions) * (fc.v[1] - fc.v[0]) +
                         (float(b) / divisions) * (fc.v[2] - fc.v[0]);
      };
      auto checkPatch = [&](const Vec3f &a, const Vec3f &b, const Vec3f &c) {
        ++nPatches;
        const Vec3f p = (a + b + c) * (1.0f / 3.0f);
        const float containerDist = scene.sdf->GetCoarseDist(p);
        if (containerDist > 0.2f || containerDist < -cfg.hullSurfaceBand) {
          ++nHidden; return;
        }
        if (innerSdf && innerSdf->GetCoarseDist(p) <= 0.0f) {
          ++nInner; return;
        }
        if (excl && excl->GetCoarseDist(p) < cfg.hullExclMargin) {
          ++nExcl; return;
        }
        if (MinFruitDist(scene, p, 2.0f) <= 0.35f) {
          ++nFruit; return;
        }
        // The outward side must be open over a useful fruit-scale distance;
        // reaching the container wall is not required (it rejected visible
        // front-side mouths while admitting broad internal sheets).
        for (float s = 0.75f; s <= 5.25f; s += 0.75f) {
          if (MinFruitDist(scene, p + s * n, 2.0f) <= 0.1f) {
            ++nHidden; return;
          }
        }
        // A mouth matters only if it caps actual free crevice lattice points.
        // Count points in a short cylinder beneath this patch. The target
        // lattice is volume based, so this score is not triangle-density bias.
        std::vector<unsigned> under;
        ++targetStamp;
        const float lateral2 = std::pow(std::max(2.0f, patchSize + 0.5f), 2.0f);
        const Vec3f ab = b - a, ac = c - a;
        const float d00 = ab.dot(ab), d01 = ab.dot(ac), d11 = ac.dot(ac);
        const float baryDenom = d00 * d11 - d01 * d01;
        if (baryDenom < 1e-8f) return;
        for (float s = 1.5f; s < 16.0f; s += targetCell) {
          const auto [cx, cy, cz] = targetKey(p - s * n);
          for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
              for (int dx = -1; dx <= 1; ++dx) {
                auto it = targetBins.find({cx + dx, cy + dy, cz + dz});
                if (it == targetBins.end()) continue;
                for (unsigned ti : it->second) {
                  if (targetSeen[ti] == targetStamp) continue;
                  targetSeen[ti] = targetStamp;
                  const Vec3f delta = targets[ti] - p;
                  const float along = -delta.dot(n);
                  if (along <= 0.25f || along > 16.0f ||
                      delta.norm2() - along * along > lateral2) continue;
                  const Vec3f ap = targets[ti] + along * n - a;
                  const float d20 = ap.dot(ab), d21 = ap.dot(ac);
                  const float u = (d11 * d20 - d01 * d21) / baryDenom;
                  const float v = (d00 * d21 - d01 * d20) / baryDenom;
                  // A small seam tolerance prevents lattice points directly
                  // over a subdivision edge from being lost to both patches.
                  if (u < -0.12f || v < -0.12f || u + v > 1.12f) continue;
                  under.push_back(ti);
                }
              }
        }
        if (under.empty()) { ++nNoTargets; return; }
        // Test multi-fruit support where the gap actually is. A hull mouth can
        // be broad and remote from one fruit at its centroid even when the
        // target directly beneath lies tightly between two fruits.
        under.erase(std::remove_if(under.begin(), under.end(), [&](unsigned ti) {
          float first = kInf, second = kInf;
          for (unsigned f : grp) {
            const float d = InstanceDist(scene, fruit[f], targets[ti]);
            if (d < first) { second = first; first = d; }
            else if (d < second) second = d;
          }
          return second > cfg.hullSupportDist;
        }), under.end());
        if (under.empty()) { ++nSupport; return; }
        const float depth = RayToFruit(scene, p, -1.0f * n, 16.0f);
        if (depth < cfg.hullMinDepth) { ++nShallow; return; }
        // The first hit of a ray from the patch centroid can be a shoulder
        // beside the target, rather than the back of the same open channel.
        // Keep depth as a mouth-quality measurement, not a target cutoff.
        if (under.empty()) { ++nNoTargets; return; }
        Face patch;
        patch.v[0] = a; patch.v[1] = b; patch.v[2] = c;
        patch.n = n; patch.depth = depth; patch.surfaceDist = containerDist;
        patch.group = gi; patch.targetIds = std::move(under);
        faces.push_back(std::move(patch));
      };
      for (int a = 0; a < divisions; ++a)
        for (int b = 0; a + b < divisions; ++b) {
          checkPatch(gridPoint(a, b), gridPoint(a + 1, b), gridPoint(a, b + 1));
          if (a + b + 1 < divisions)
            checkPatch(gridPoint(a + 1, b), gridPoint(a + 1, b + 1), gridPoint(a, b + 1));
        }
    }
  }

  std::vector<size_t> order(faces.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    if (faces[a].targetIds.size() != faces[b].targetIds.size())
      return faces[a].targetIds.size() > faces[b].targetIds.size();
    return faces[a].surfaceDist > faces[b].surfaceDist;
  });
  std::vector<char> claimed(targets.size(), 0);
  std::vector<Face> selected;
  selected.reserve(std::min(faces.size(), targets.size()));
  for (size_t i : order) {
    bool useful = false;
    for (unsigned ti : faces[i].targetIds) if (!claimed[ti]) { useful = true; break; }
    if (!useful) { ++nDup; continue; }
    for (unsigned ti : faces[i].targetIds) claimed[ti] = 1;
    selected.push_back(std::move(faces[i]));
  }
  faces.swap(selected);
  unsigned claimedCount = 0;
  std::ofstream claimedObj(scene.outputFolder + "/crevice_faces_claimed_targets.obj");
  std::ofstream unclaimedObj(scene.outputFolder + "/crevice_faces_unclaimed_targets.obj");
  for (size_t i = 0; i < targets.size(); ++i) {
    std::ostream &out = claimed[i] ? claimedObj : unclaimedObj;
    out << "v " << targets[i][0] << ' ' << targets[i][1] << ' ' << targets[i][2] << "\n";
    claimedCount += unsigned(claimed[i]);
  }

  // fan-triangulated polygons (all faces are triangles here, sharing v0).
  std::ofstream obj(scene.outputFolder + "/crevice_faces.obj");
  for (const Face &fc : faces)
    for (const Vec3f &v : fc.v) obj << "v " << v[0] << ' ' << v[1] << ' ' << v[2] << "\n";
  for (size_t i = 0; i < faces.size(); ++i) {
    const size_t b = 3 * i + 1;
    for (size_t k = 1; k + 1 < 3; ++k) obj << "f " << b << ' ' << b + k << ' ' << b + k + 1 << "\n";
  }
  std::ofstream txt(scene.outputFolder + "/crevice_faces.txt");
  txt << "face depth_cm container_dist_cm target_count nx ny nz group_fruit_instance_ids\n";
  for (size_t i = 0; i < faces.size(); ++i) {
    const Face &fc = faces[i];
    txt << i << ' ' << fc.depth << ' ' << fc.surfaceDist << ' ' << fc.targetIds.size()
        << ' ' << fc.n[0] << ' ' << fc.n[1] << ' ' << fc.n[2];
    for (unsigned f : groups[fc.group]) txt << ' ' << fruit[f];
    txt << "\n";
  }
  LOGI("crevice faces: " << nf << " fruit, " << edges << " tight edges, " << pairGroups
                         << " pair groups, " << tris << " triangles, " << quads << " chordless quads\n");
  LOGI("crevice faces: " << nBridging << " bridging hull faces, " << nPatches
                         << " tested patches; dropped " << nInner << " inner, " << nExcl
                         << " near exclusion, " << nHidden << " not facing outside, "
                         << nFruit << " on fruit, " << nSupport << " unsupported, "
                         << nNoTargets << " over no targets, " << nShallow << " shallow, "
                         << nDup << " target duplicates; kept "
                         << faces.size() << " covering " << claimedCount << "/" << targets.size()
                         << " targets in "
                         << clock.ElapsedMS() / 1000.0 << " s\n");
}
