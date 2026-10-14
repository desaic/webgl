#pragma once

#include "PackingConfig.h"
#include "PackingScene.h"

#include <string>
#include <vector>

// graph over the placed medium fruit. nodes are medium instances, edges join
// pairs whose surfaces are within a gap threshold.
struct MediumGraph {
  struct Node {
    unsigned instance = 0;   // index into scene.instances
    Vec3f center;            // world-space bbox center of the fruit
    float radius = 0.0f;     // bounding sphere radius
    unsigned fixedNbrs = 0;  // non-medium neighbors (large fruit, exclusion) within the gap
    bool containerNbr = false;
  };
  struct Edge {
    unsigned a = 0, b = 0;   // node indices
    float gap = 0.0f;        // min surface distance, negative if overlapping (cm)
  };
  std::vector<Node> nodes;
  std::vector<Edge> edges;
};

/// nodes: instances of the named kinds with center x < maxX. neighbors are
/// looked up among all instances.
MediumGraph BuildMediumGraph(PackingScene &scene, const std::vector<std::string> &mediumNames,
                             float maxX, float maxGap);

/// writes <prefix>.txt (nodes and edges) and <prefix>.obj (edges as line segments).
void SaveMediumGraph(const PackingScene &scene, const MediumGraph &g, const std::string &prefix);

/// loads cfg.resumePackFile, builds the graph for plan group 2 with x < cfg.sphereMaxX,
/// saves it into the output folder. true on success.
bool RunMediumGraph(const PackingConfig &cfg, const std::vector<std::string> &mediumNames);
