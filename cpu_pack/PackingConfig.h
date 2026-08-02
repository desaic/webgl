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
    float gridDx = 1.0f;
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
    float maxSecondsPerStep = 0.0f;

    // path helpers. all return absolute paths.
    std::string MeshDir() const;
    std::string ContainerPath() const;
    std::string InnerContainerPath() const;
    std::string OutputFolder() const;
    std::string ResumePackPath() const;

    /// reads dataDir from argv[1] if given, else the FRUIT_HAND_DIR env var,
    /// else leaves the default. keeps the per-machine paths out of the source.
    void ParseArgs(int argc, char **argv);

    std::string toString() const;
};
