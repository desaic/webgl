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

    // shrinkwrap field parameters, shared by Phase 2 (ComputeShrinkwrapField)
    // and the old, removed-from-pipeline nominal-size fill pass
    // (PackFillVoids, kept only for DebugFillVoids/benchmarks, ComputeVoidField).
    // voxelSize is independent of dx and needs to be fine enough to resolve
    // the smallest fruit tier (melone_test uses 0.1, 3x finer than dx=0.3).
    float shrinkwrapRadius = 1.0f;
    float shrinkwrapVoxelSize = 0.2f;
    unsigned shrinkwrapOpenRadiusVoxels = 1;
    // PackFillVoids-only (the old removed pass).
    float fillVoidClearance = 0.1f;
    unsigned fillVoidMinPlaced = 3;
    unsigned fillVoidMaxRounds = 10;

    // Phase 2 (plan.txt PIPELINE Phase 2; renamed from Phase 3 once the old
    // nominal-size Phase 2/PackFillVoids was removed from the pipeline):
    // small fruit resting on top of fruit-fruit crevices (especially where
    // 3+ fruit meet), at low volume. Targets are shrinkwrap SURFACE voxels
    // not already covered by a bigger fruit's own surface (ComputeShrinkwrapField,
    // PackShrinkWrap.h) -- not volumetric interior gaps, there is no "fill
    // void" step left in the real pipeline at all.
    float creviceClearance = 0.1f;
    // one placed fruit's claim radius (its own BoxDiagonal, often much
    // bigger than the crevice itself) can legitimately cover dozens of
    // nearby crevice spots in a single round, so unlike the old removed
    // pass, 1 placement in a round is still real progress; stop only once
    // a round places nothing at all.
    unsigned creviceMinPlaced = 1;
    unsigned creviceMaxRounds = 15;
    // crevice opening WIDTH (2*s.radius) bounds, cm, from the user's
    // manufacturability bucket table: <0.2cm self-fills (ignore), 0.2-3cm
    // is Phase 2's actual working range (rest fruit on top, shrink the
    // opening), >3cm is a real void Phase 2 is not meant to handle. The
    // lower bound is also roughly enforced by ExtractVoidSpots' own
    // threshold (shrinkwrapVoxelSize*1.1 on s.radius), but checked
    // explicitly here too so it does not silently drift if that changes.
    float creviceMinWidth = 0.2f;
    float creviceMaxWidth = 3.0f;
    // a baseline crevice spot counts as closed once less than this fraction
    // of its own baseline voxels is still open in the current void field.
    float creviceClosedFrac = 0.5f;
    // TryFillSpot attempts per baseline spot, across all rounds of one call.
    unsigned creviceMaxAttemptsPerSpot = 3;
    // a Phase 2 fruit that closed less baseline crevice volume than this
    // (cm^3) is reported as unnecessary by the coverage metric.
    float creviceUsefulVolume = 0.05f;

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
    /// trusted. numSteps means skip all steps; anything larger is clamped to
    /// that, and an empty plan is the
    /// caller's problem. returns true if the value was changed.
    /// startItem is clamped in PackStep instead, because the valid range is
    /// the kind count of whichever step is running, not a property of cfg.
    bool ClampStartStep(size_t numSteps);

    std::string toString() const;
};
