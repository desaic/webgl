#include "MediumGraph.h"

#include "AdapSDF.h"
#include "Log.h"
#include "PackingDriver.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>

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
  radius = 0.5f * (it.box.vmax - it.box.vmin).norm();
  return inst.tran.rotation * c + inst.tran.position;
}

}  // namespace

MediumGraph BuildMediumGraph(PackingScene &scene, const std::vector<std::string> &mediumNames,
                             float maxX, float maxGap) {
  MediumGraph g;
  std::set<unsigned> mediumKinds;
  for (const std::string &n : mediumNames) {
    auto it = scene.nameToIndex.find(n);
    if (it != scene.nameToIndex.end()) mediumKinds.insert(it->second);
  }
  // samples and sdf for every kind that appears in the pack.
  for (const InstanceInfo &inst : scene.instances) {
    scene.EnsureItemSamples(inst.itemId);
    scene.items[inst.itemId].ComputeSDFCached();
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

bool RunMediumGraph(const PackingConfig &cfg, const std::vector<std::string> &mediumNames) {
  PackingScene scene;
  if (!BuildScene(scene, cfg)) return false;
  PrepareBackground(scene, cfg);
  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    scene.instances[scene.Put(scene.exclusionItemIndex, identity)].isExclusion = true;
  }
  LoadPack(scene, cfg.ResumePackPath());
  if (scene.instances.empty()) {
    LOGI("no instances loaded from " << cfg.ResumePackPath() << "\n");
    return false;
  }
  MediumGraph g = BuildMediumGraph(scene, mediumNames, cfg.sphereMaxX, cfg.graphGap);
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
  return true;
}
