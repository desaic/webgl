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

/// splits the graph into vertex-disjoint paths. each path runs from a leaf-like start to
/// the end with the lower x, which is its last element. singletons are kept.
std::vector<std::vector<unsigned>> BuildMediumPaths(const MediumGraph &g);

/// writes the graph vertices and one polyline per path.
void SaveMediumPaths(const MediumGraph &g, const std::vector<std::vector<unsigned>> &paths,
                     const std::string &filename);

/// per path, last fruit first: pushes the last fruit along the path tangent, then each
/// earlier fruit toward its successor's new position. repeats up to `passes` times or until
/// little moves. moves that end in penetration are reverted. updates scene.instances and the
/// node centers. returns the number of passes run.
unsigned NudgeAlongPaths(PackingScene &scene, MediumGraph &g,
                         const std::vector<std::vector<unsigned>> &paths, unsigned passes);

/// puts the first fruit of each path (the exposed end) back at its original pose if it moved
/// less than diameterFraction * its bounding sphere diameter. returns the number reverted.
unsigned RevertShortMoves(PackingScene &scene, MediumGraph &g,
                          const std::vector<std::vector<unsigned>> &paths,
                          const std::vector<RigidTransform> &origTran, float diameterFraction);

/// after NudgeAlongPaths: per path, from the original vertex next to the packed end back toward
/// the path start, spawns the kind that was packed at that vertex (at its original pose),
/// nudges it toward the packed end, and repeats until it no longer fits there. spawned instance
/// ids are appended to `spawned`. returns spawned.size().
unsigned SpawnAlongPaths(PackingScene &scene, const MediumGraph &g,
                         const std::vector<std::vector<unsigned>> &paths,
                         const std::vector<RigidTransform> &origTran,
                         std::vector<unsigned> &spawned);

/// writes the trajectories of the given instances (those with a saved trajectory).
void SaveNudgedTrajectories(const PackingScene &scene, const std::vector<unsigned> &ids,
                            const std::string &filename);

/// loads cfg.resumePackFile, builds the graph for plan group 2 with x < cfg.sphereMaxX,
/// saves it into the output folder. true on success.
bool RunMediumGraph(const PackingConfig &cfg, const std::vector<std::string> &mediumNames, bool refine = false);
