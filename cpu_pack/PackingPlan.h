#pragma once

#include "BBox.h"
#include "Vec3.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

enum class StepKind {
  Bulk,     // PackStep: force/gravity-driven placement.
  Crevice,  // PackFillCrevices: small fruit resting on this step's own
            // crevice baseline (names is the small-fruit kind list).
};

struct PackingStep {

    std::vector<std::string> names;

    Vec3f force;

    // weight for force. such as bias towards -x.
    float biasW = 1.0f;

    // weight for biasForce + sdfForce against attraction towards other objects.
    float forceW = 0.5f;

    unsigned count = 0;
    // pack towards inside or outside of container.
    bool outwards = true;
    // prevent packing at center of container
    bool useInnerContainer = false;
    // Bulk only: target the centroid of nearby free container-surface
    // points (see PackStep) instead of the default force field.
    bool useFreeSurfacePoints = false;
    // Bulk only, mutually exclusive with useFreeSurfacePoints: target
    // points on the CURRENT shrinkwrap surface of already-placed fruit
    // (see ComputeShrinkwrapSurfacePoints) instead of the container wall --
    // lets this tier nestle onto the pack built so far rather than just the
    // container. Uses this step's own shrinkwrapRadius/shrinkwrapVoxelSize.
    bool useShrinkwrapSurfacePoints = false;

    StepKind kind = StepKind::Bulk;
    // Used by StepKind::Crevice, and by Bulk when useShrinkwrapSurfacePoints
    // is set. Independent per step so a bigger-fruit step can use a coarser
    // grid than a small-fruit one -- keeps the dense shrinkwrap field
    // affordable as fruit/container size grows.
    float shrinkwrapRadius = 1.0f;
    float shrinkwrapVoxelSize = 0.2f;
    unsigned shrinkwrapOpenRadiusVoxels = 1;

    PackingStep() : force(-1, 0, 0) {}

    std::string toString() const;

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
