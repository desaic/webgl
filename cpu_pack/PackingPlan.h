#pragma once

#include "BBox.h"
#include "Vec3.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

// FillVolume: the original lattice-walk search (PackStep), aimed by
// step.force/outwards. FillSurface: the pocket-targeted pass
// (PocketDriver's PackPocketStep) for the final small-item step, aimed at
// deep pockets found by ray marching from the container surface instead of
// a fixed direction. See heuristic_plan.md H2 onward.
enum class FillMode {
    FillVolume,
    FillSurface,
};

struct PackingStep {

    std::vector<std::string> names;

    Vec3f force;

    unsigned count = 0;
    // pack towards inside or outside of container.
    bool outwards = true;
    // prevent packing at center of container
    bool useInnerContainer = false;
    FillMode fillMode = FillMode::FillVolume;

    PackingStep() : force(-1, 0, 0) {}

    std::string toString() const;

    // fillMode is read as an optional trailing token: a plan saved before
    // H5 has nothing after useInnerContainer, and a missing token must mean
    // FillVolume rather than a parse error.
    void Load(std::istream &in);
};

struct PackingPlan{
  std::vector<PackingStep> steps;
  // lists of meshes grouped by sizes.
  std::vector< std::vector<std::string> > groups;

  void Save(std::ostream & out) const;

  void Load(std::istream & in);
};

/// bounding box summary for one input mesh, cached in stats.txt.
struct MeshStat {
    std::string type;
    std::string name;
    Box3f box;

    /// max side of the bounding box. decides the size group.
    float MaxExtent() const;

    std::string toString() const;

    void Parse(std::istream &in);
};

/// scans meshDir and writes stats.txt with one MeshStat per mesh.
void ComputeMeshStats(const std::string &meshDir);

/// reads stats.txt from meshDir.
std::vector<MeshStat> LoadMeshStats(const std::string &meshDir);

/// index of the first threshold that len exceeds. thresholds descend.
unsigned GetGroupIndex(float len, const std::vector<float> &thresh);

/// groups meshes by size and builds the ordered list of packing steps.
PackingPlan PlanPackingSteps(const std::string &meshDir);
