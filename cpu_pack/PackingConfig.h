#pragma once

#include <string>

/// all tunable inputs for a packing run.
/// pulled out of main.cpp so benchmarks can build a scenario
/// without editing production code.
struct PackingConfig {
    // root of the mesh data set. everything below is relative to it.
    std::string dataDir = "/media/desaic/WD/meshes/fruit_hand/";
    // subdirectory holding the items to pack.
    std::string fruitSubdir = "fruits_1";
    // container meshes, relative to dataDir.
    std::string containerFile = "hands/finger4.8m.stl";
    std::string innerContainerFile = "hands/finger_4.8m_inner.stl";
    // output directory, relative to dataDir.
    std::string outSubdir = "out";

    // voxel size for the packing occupancy grid.
    float dx = 0.3f;
    // voxel size for the container sdf. 2cm.
    float containerSDFDx = 2.0f;
    // broad phase cell size.
    float broadPhaseDx = 2.0f;
    // cell size for the TrigGrid narrow phase acceleration grids.
    // a contact query costs one closestPointTriangle per triangle in the
    // cells it touches, and the fruit meshes carry a few hundred triangles
    // per square cm, so this is the dominant term in narrow phase time.
    // measured on the 6k berry case: 1.0 -> 124 ms per placement, 0.5 -> 67,
    // 0.25 -> 57 but with grids at 400 MB. 0.5 is the knee.
    float gridDx = 0.5f;
    // subgrid cell size used by FindSpotSubgrid for small items.
    float subgridCellSize = 20.0f;

    // number of random rotations tried per item per spot search.
    unsigned maxTrialCount = 10;
    // first plan step to run. skips earlier steps.
    unsigned startStep = 3;
    // first item index to consider within a step.
    unsigned startItem = 0;

    // resume a previous run from this pack file, relative to dataDir.
    std::string resumePackFile = "pack_944_0721.txt";
    bool resume = true;

    // save progress every N placements. 0 disables the write.
    // benchmarks set these to 0. each save rewrites the whole
    // accumulated state, so they are slow and grow with run length.
    unsigned trajSaveInterval = 10;
    unsigned packSaveInterval = 20;

    // recompute stats.txt before planning.
    bool computeStats = true;

    // give up on a plan step after this many seconds. 0 means no limit.
    // once the container is nearly full a single placement can cost
    // thousands of failed spot searches, so a benchmark needs a way to
    // stop mid step. production runs leave this at 0.
    // also the safety backstop for a fillSurface step (PocketDriver);
    // its actual stop condition is running out of eligible pockets.
    float maxSecondsPerStep = 0.0f;

    // fillSurface step tuning (PocketDriver / heuristic_plan.md H2-H4).
    // container surface sample spacing, cm.
    float surfaceRaySpacing = 1.0f;
    // spatial hash cell size for grouping surface rays into patches, cm.
    float patchSize = 4.0f;
    // a ray deeper than this is "open" for rim/mouth-diameter targeting.
    float holeDepth = 1.5f;
    // a patch needs some ray open past this to be worth visiting at all;
    // short of it is a shallow slope, left as terrain.
    float deepPocketThreshold = 3.0f;
    // surface ray march cap, cm.
    float maxProbeDepth = 12.0f;
    // consecutive fails before a patch is permanently excluded -- the
    // mechanism behind H4's "every deep pocket filled or unfillable" stop.
    unsigned patchMaxFails = 6;
    // spot search trials per pocket placement attempt.
    unsigned pocketTrialCount = 3;

    // path helpers. all return absolute paths.
    std::string MeshDir() const;
    std::string ContainerPath() const;
    std::string InnerContainerPath() const;
    std::string OutputFolder() const;
    std::string ResumePackPath() const;

    /// argv[1] is taken as a config file if it names a regular file, as
    /// dataDir if it names a directory. "--config <file>" also works, and is
    /// what the benchmark binary uses since it has its own flags. Falls back
    /// to the FRUIT_HAND_DIR env var, then the defaults above. Keeps the
    /// per-machine paths out of the source.
    void ParseArgs(int argc, char **argv);

    /// reads "key value" lines, ignoring blank lines and # comments.
    /// unknown keys are reported and skipped rather than being fatal, so an
    /// old config file still runs after a field is renamed.
    /// see pack_fruits.cfg for the template.
    bool LoadFromFile(const std::string &path);

    /// startStep comes from a hand written file, so it is clamped rather than
    /// trusted. out of range means the last step, and an empty plan is the
    /// caller's problem. returns true if the value was changed.
    /// startItem is clamped in PackStep instead, because the valid range is
    /// the kind count of whichever step is running, not a property of cfg.
    bool ClampStartStep(size_t numSteps);

    std::string toString() const;
};
