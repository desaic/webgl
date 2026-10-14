#include "CreviceMouths.h"

#include "AdapSDF.h"
#include "Log.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

namespace {

constexpr float kInf = std::numeric_limits<float>::max();

struct Surface {
  float dist = kInf;
  Vec3f point;
  Vec3f normal;
  unsigned id = ~0u;
};

Surface NearestSurface(const PackingScene &scene, unsigned id, const Vec3f &p) {
  const auto &inst = scene.instances[id];
  const auto &item = scene.items[inst.itemId];
  const float scale = std::max(inst.tran.scale, 0.001f);
  const Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position) / scale;
  const float d = scale * item.sdf->GetCoarseDist(local);
  const float h = 0.35f / scale;
  Vec3f n(item.sdf->GetCoarseDist(local + Vec3f(h, 0, 0)) -
              item.sdf->GetCoarseDist(local - Vec3f(h, 0, 0)),
          item.sdf->GetCoarseDist(local + Vec3f(0, h, 0)) -
              item.sdf->GetCoarseDist(local - Vec3f(0, h, 0)),
          item.sdf->GetCoarseDist(local + Vec3f(0, 0, h)) -
              item.sdf->GetCoarseDist(local - Vec3f(0, 0, h)));
  if (n.norm2() < 1e-8f) return {};
  n.normalize();
  n = inst.tran.rotation * n;
  return {d, p - d * n, n, id};
}

float MinFruitDist(const PackingScene &scene, const Vec3f &p, float range) {
  Box3f box;
  box.vmin = box.vmax = p;
  float best = kInf;
  for (unsigned id : scene.broadPhase.GetNearby(box, range)) {
    const auto &inst = scene.instances[id];
    if (inst.isExclusion || !scene.items[inst.itemId].sdf) continue;
    const float scale = std::max(inst.tran.scale, 0.001f);
    const Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position) / scale;
    best = std::min(best, scale * scene.items[inst.itemId].sdf->GetCoarseDist(local));
  }
  return best;
}

struct Mouth {
  Vec3f p;
  Vec3f source;
  float width = 0.0f;
  float outerDist = 0.0f;
  unsigned a = ~0u, b = ~0u;
};

using Key = std::tuple<unsigned, unsigned, int, int, int>;
Key CellKey(const Mouth &m, float cell) {
  return {m.a, m.b, int(std::floor(m.p[0] / cell)),
          int(std::floor(m.p[1] / cell)), int(std::floor(m.p[2] / cell))};
}

} // namespace

std::vector<Vec3f> ExportCreviceMouths(PackingScene &scene, const PackingConfig &cfg,
                                       const std::vector<Vec3f> &targets,
                                       float minMediumRadius) {
  Utils::Stopwatch clock;
  clock.Start();
  std::shared_ptr<AdapSDF> inner;
  if (!scene.containerInner.mesh.v.empty())
    inner = ComputeSDF(0.002f, 2.0f, scene.containerInner.mesh);
  const AdapSDF *excl = nullptr;
  if (scene.exclusionItemIndex != ~0u)
    excl = scene.items[scene.exclusionItemIndex].sdf.get();

  std::vector<Mouth> raw;
  raw.reserve(targets.size());
  unsigned noPair = 0, notOpposed = 0, blocked = 0, innerCount = 0, farFromSkin = 0;
  for (const Vec3f &target : targets) {
    Box3f box;
    box.vmin = box.vmax = target;
    Surface first, second;
    for (unsigned id : scene.broadPhase.GetNearby(box, cfg.mouthNeighborDist)) {
      const auto &inst = scene.instances[id];
      if (inst.isExclusion || !scene.items[inst.itemId].sdf) continue;
      // Computing three SDF gradients for every nearby fruit is expensive;
      // rank by distance first and evaluate gradients for only the best pair.
      const float scale = std::max(inst.tran.scale, 0.001f);
      const Vec3f local = inst.tran.rotation.transposed() *
                          (target - inst.tran.position) / scale;
      const float d = scale * scene.items[inst.itemId].sdf->GetCoarseDist(local);
      if (d < first.dist) { second = first; first = {d, {}, {}, id}; }
      else if (d < second.dist) second = {d, {}, {}, id};
    }
    if (second.id == ~0u || second.dist > cfg.mouthNeighborDist) {
      ++noPair; continue;
    }
    first = NearestSurface(scene, first.id, target);
    second = NearestSurface(scene, second.id, target);
    if (first.id == ~0u || second.id == ~0u ||
        first.normal.dot(second.normal) > cfg.mouthMaxNormalDot) {
      ++notOpposed; continue;
    }
    const Vec3f p = 0.5f * (first.point + second.point);
    const float width = (first.point - second.point).norm();
    if (width < 0.3f || width > 2.0f * cfg.mouthNeighborDist ||
        MinFruitDist(scene, p, 2.0f) < 0.3f ||
        (excl && excl->GetCoarseDist(p) < 0.0f)) {
      ++blocked; continue;
    }
    if (inner && inner->GetCoarseDist(p) <= 0.0f) {
      ++innerCount; continue;
    }
    const float outerDist = scene.sdf->GetCoarseDist(p);
    if (outerDist > 0.5f || outerDist < -cfg.mouthOuterBand) {
      ++farFromSkin; continue;
    }
    raw.push_back({p, target, width, outerDist,
                   std::min(first.id, second.id), std::max(first.id, second.id)});
  }

  // The lattice repeats the same passage center at several depths. Retain
  // one center for each fruit pair and 1.5-cm cell, favoring the exposed side.
  std::map<Key, Mouth> cells;
  for (const Mouth &m : raw) {
    const Key key = CellKey(m, cfg.mouthCell);
    auto it = cells.find(key);
    if (it == cells.end() || m.outerDist > it->second.outerDist ||
        (m.outerDist == it->second.outerDist && m.width < it->second.width))
      cells[key] = m;
  }
  std::vector<Mouth> mouths;
  mouths.reserve(cells.size());
  for (const auto &entry : cells) mouths.push_back(entry.second);

  const float minWidth = std::max(0.0f, 2.0f * (minMediumRadius - 0.2f));
  std::vector<Vec3f> feasible;
  std::ofstream points(scene.outputFolder + "/crevice_mouth_points.obj");
  std::ofstream fit(scene.outputFolder + "/crevice_mouth_medium_width.obj");
  std::ofstream table(scene.outputFolder + "/crevice_mouth_points.txt");
  table << "x y z width_cm outer_dist_cm fruit_a fruit_b target_x target_y target_z medium_width\n";
  for (const Mouth &m : mouths) {
    points << "v " << m.p[0] << ' ' << m.p[1] << ' ' << m.p[2] << '\n';
    const bool mediumWidth = m.width >= minWidth;
    if (mediumWidth) {
      fit << "v " << m.p[0] << ' ' << m.p[1] << ' ' << m.p[2] << '\n';
      feasible.push_back(m.p);
    }
    table << m.p[0] << ' ' << m.p[1] << ' ' << m.p[2] << ' ' << m.width << ' '
          << m.outerDist << ' ' << m.a << ' ' << m.b << ' '
          << m.source[0] << ' ' << m.source[1] << ' ' << m.source[2] << ' '
          << int(mediumWidth) << '\n';
  }

  // A local same-pair graph is useful for grouping a visible groove into one
  // region. Edges are short; they cannot jump across unrelated cavities.
  std::ofstream graph(scene.outputFolder + "/crevice_mouth_graph.obj");
  for (const Mouth &m : mouths)
    graph << "v " << m.p[0] << ' ' << m.p[1] << ' ' << m.p[2] << '\n';
  std::map<Key, std::vector<unsigned>> bins;
  for (unsigned i = 0; i < mouths.size(); ++i) bins[CellKey(mouths[i], cfg.mouthCell)].push_back(i);
  unsigned edges = 0;
  for (unsigned i = 0; i < mouths.size(); ++i) {
    const Mouth &m = mouths[i];
    const auto [a, b, cx, cy, cz] = CellKey(m, cfg.mouthCell);
    std::vector<std::pair<float, unsigned>> near;
    for (int dz = -2; dz <= 2; ++dz)
      for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
          const auto it = bins.find({a, b, cx + dx, cy + dy, cz + dz});
          if (it == bins.end()) continue;
          for (unsigned j : it->second) {
            if (j == i || std::fabs(m.width - mouths[j].width) > 2.0f) continue;
            const float d2 = (m.p - mouths[j].p).norm2();
            if (d2 <= 2.4f * 2.4f) near.push_back({d2, j});
          }
        }
    std::sort(near.begin(), near.end());
    for (size_t k = 0; k < std::min<size_t>(3, near.size()); ++k)
      if (near[k].second > i) {
        graph << "l " << i + 1 << ' ' << near[k].second + 1 << '\n';
        ++edges;
      }
  }
  LOGI("crevice mouths: " << targets.size() << " targets; " << raw.size()
       << " opposed two-fruit crossings, " << mouths.size() << " local centers, "
       << feasible.size() << " wide enough for minimum medium sphere (" << minWidth
       << " cm), " << edges << " graph links; dropped " << noPair << " no pair, "
       << notOpposed << " same-facing, " << blocked << " obstructed, "
       << innerCount << " inner, " << farFromSkin << " away from outer shell in "
       << clock.ElapsedMS() / 1000.0 << " s\n");
  return feasible;
}
