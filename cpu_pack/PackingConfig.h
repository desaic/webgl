#pragma once

#include <limits>
#include <string>
#include <vector>

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
    // optional mesh treated as a pre-placed fixed obstacle (not saved in pack
    // output). placed at its original world position before any packing starts.
    std::string exclusionMeshFile;
    // optional .obj whose vertices are the medium-patch target points,
    // relative to dataDir. when set, targets are loaded instead of recomputed.
    std::string mediumTargetsFile;
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
    // abandon a step after this many seconds, even if step.count has not
    // been reached and kinds have not all retired. 0 means no limit. A
    // LARGE_INT ("pack until it stops fitting") step has no other bound --
    // an emptier container than expected (e.g. an earlier step placing
    // fewer than intended) can let one kind keep finding room for far
    // longer than intended before it finally, naturally exhausts.
    double maxSecondsPerStep = 0.0;
    // first plan step to run. skips earlier steps.
    unsigned startStep = 3;
    // one past the last plan step to run -- default (max) means run to the
    // end of the plan. lets a run stop early (e.g. after the big/medium-big
    // bulk steps, before crevice fill) to inspect the trajectory/pack state
    // PackFruits always saves at the end, without editing the plan itself.
    unsigned endStep = std::numeric_limits<unsigned>::max();
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

    // sphere cover (SphereCover.h): when sphereCover is set, PackScene skips the
    // plan steps and instead covers the medium targets with spheres, writing
    // sphere_cover.txt (x y z r per line) and sphere_cover_uncovered.obj.
    bool sphereCover = false;
    // uniform scales applied to each medium fruit's effective radius to get the
    // discrete sphere radii.
    std::vector<float> sphereScales = {1.2f, 1.1f, 1.0f, 0.9f, 0.8f};
    // max proxy overlap (sphere-fruit, sphere-sphere) as a fraction of radius. proxies
    // are only a search filter, so this is loose; exact fruit checks come later.
    float sphereOverlapCm = 0.2f;
    // max depth of a sphere into the container wall / exclusion mesh as a fraction of
    // radius.
    float sphereHardFrac = 0.3f;
    // a sphere must come within this many cm of a fruit or accepted sphere.
    float sphereContactTol = 0.15f;
    // score = newly covered target weight - sphereCost - sphereSizeCost * r.
    float sphereCost = 3.0f;
    float sphereSizeCost = 0.2f;
    unsigned sphereMaxCount = 5000;
    // Low-poly outer container shell (ContainerShell.h). shellVoxel <= 0 is off.
    float shellVoxel = 0.0f;     // SDF voxel size (cm)
    float shellOutset = 0.5f;    // marching-cubes level outside the container (cm)
    float shellError = 1.0f;     // simplification error bound (cm)
    float shellFraction = 0.02f; // target triangle count fraction
    // inside/outside from voxelization + flood fill only, ignoring the triangle SDF sign
    bool shellFromVoxels = true;
    float patchRadius = 12.0f; // shell patch extent from its seed triangle (cm)
    float patchAngle = 35.0f;  // max normal deviation from the seed (degrees)
    // shell passes (ShellPasses.h): filler fruit diagonal range, 8 to 14 cm group.
    bool shellPasses = false;
    float fillMinDiag = 8.0f;
    float fillMaxDiag = 14.0f;
    float shellSampleSpacing = 1.5f; // ray lattice spacing on the shell (cm)
    float holeMinArea = 30.0f;       // min open blob area (cm2)
    float crevDepth = 4.0f;      // min hull-relative depth (cm) for a pass 2 crevice sample
    float crevMinArea = 30.0f;   // min crevice blob area (cm2)
    unsigned crevGroupMax = 3;   // max fruit in a hull group
    // old FFT FindSpot + Nudge on the 8 to 14 cm group only, limited to x < sphereMaxX
    bool oldWayMedium = false;
    bool oldWayOverlap = false;   // FindSpot overlap heuristic (see PackingOps.h)
    std::string oldWayKinds;      // comma separated kind names to restrict the old way step to (empty: whole 8 to 14 group)
    bool oldWayInner = false;     // also treat the inner container (innerContainerFile) as an exclusion zone
    bool clearanceGate = false;   // skip subgrid cells without free space for the fruit's inscribed sphere (ClearanceGate.h)
    unsigned oldWayGroup = 2;     // size group of the old way step: 2 is 8 to 14 cm, 3 is 4 to 8 cm
    // contact score for the old way step (see SpotScoreParams in PackingOps.h): needs the items-only
    // background, so it implies the overlap heuristic. replaces the sdf/position score, skips the
    // inward nudge, and tries snugRotations distinct orthogonal-ish rotations per cell, keeping the best.
    bool snugScore = false;
    unsigned snugNudgeSteps = 80; // short contact-only settle after the spot search (0 = place as found)
    // plan then commit: score every spot on the untouched starting pack first (local maxima only), save them
    // as snug_candidates.obj / .txt, then place best first. gaps made by the new fruit never become candidates.
    bool snugPlan = false;
    float snugPlanNms = 1.5f;     // candidates closer than this (cm) to a better one of the same kind and rotation are dropped
    unsigned snugPlanMax = 24;    // local maxima kept per cell and rotation
    // geometric wrap score, fruit only (the container wall does not count): wrap = touching fraction * (1 - R).
    // touching fraction: share of the fruit's surface samples within snugTouchCm of another fruit's surface.
    // R: length of the mean outward normal of the touched surfaces, 0 when they push from all sides, 1 when
    // they all push from one side (a fruit lying on one convex surface). hand placed fruit score 0.04 to 0.14,
    // fruit resting on one fruit score 0.011 or less.
    float snugMinWrap = 0.03f;    // a spot needs at least this wrap score
    float snugWrapW = 100.0f;     // score += this * wrap
    float snugTouchCm = 0.4f;
    // review phase after the commit: every newly placed fruit is run through the rigid body Nudge with a contact
    // seeking field (each surface sample is pulled toward the nearest neighbor surface within snugRefineReach, new
    // neighbors included), so it slides and turns until it is wedged. the result is kept only when its wrap rose, its
    // wrap against the starting pack alone stays at snugMinWrap or more, and its overlap stays within the cap.
    bool snugRefine = true;
    unsigned snugRefineSteps = 150;  // Nudge steps per fruit and pass
    unsigned snugRefinePasses = 2;
    float snugRefineReach = 2.5f;    // cm, samples this far from a neighbor surface are pulled toward it
    float snugRefineMaxPen = 0.25f;  // cm, cap on the deepest sample inside a neighbor or outside the container
    // debug: log the commit decision of every candidate within snugDebugRadius cm of snugDebugSpot
    float snugDebugX = 0.0f, snugDebugY = 0.0f, snugDebugZ = 0.0f;
    float snugDebugRadius = 0.0f;
    float snugShrinkCm = 0.5f;    // cm, the spot search shrinks fruit under 5 cm by this much before the FFT (0 = true size)
    unsigned snugVoxTol = 0;      // voxels a planned pose may hit in the updated background (the voxel mapping has about one voxel of slack)
    unsigned snugRotations = 12;  // distinct rotations tried per cell (at most 24)
    float snugWallW = 0.5f;       // weight of the distance to the container wall (cm), positive = prefer far
    float snugWallCap = 10.0f;    // wall distance beyond this does not score more (cm)
    float snugPosW = 0.01f;       // weight of -(x+y+z), tie breaker
    unsigned snugMinDirs = 2;     // contact needed in at least this many of the 6 axis directions
    unsigned snugMinFace = 4;
    float snugShellCm = 2.0f;     // enclosure shell thickness around the fruit (cm)
    float snugMinBuried = 0.6f;   // occupied fraction of that shell (fruit only, container ignored) needed for a spot
    float snugBuriedW = 10.0f;    // score weight of the occupied fraction     // voxels of contact for a direction to count
    unsigned oldWayErode = 1;     // voxels eroded from the candidate fruit before the FFT search
    std::string forceKind = "LightGreen";  // inner force field: kind whose leftmost instance is saved
    float forceGridDx = 0.5f;      // inner force field: local grid voxel size (cm)
    float forceGridMargin = 20.0f; // inner force field: local grid margin around the fruit (cm)
    unsigned vhacdMaxHulls = 16;   // vhacd: max convex hulls per mesh
    unsigned vhacdResolution = 200000; // vhacd: voxel resolution
    float vhacdErrorPct = 2.0f;    // vhacd: hull is close enough within this percent of the voxel volume
    std::string vhacdFill = "flood"; // vhacd: interior fill, flood | raycast | surface
    std::string vhacdOnly;         // vhacd: comma separated mesh names to process (empty: all)
    unsigned vhacdDepth = 10;      // vhacd: max recursion depth
    unsigned vhacdMaxVerts = 48;   // vhacd: max vertices per hull
    std::string hullDir;           // inner nudge: subfolder of dataDir with <name>.hulls.txt; empty = triangle grid nudge
    float hullInset = 0.2f;        // inner nudge: hulls shrink by this many cm
    float hullSampleSpacing = 0.8f; // inner nudge: spacing of the hull surface samples
    float forceExclusionSkip = 5.0f; // inner nudge: skip fruit whose surface is within this many cm of the exclusion mesh
    unsigned forcePasses = 3;      // inner nudge: passes over all fruit
    float graphGap = 3.0f;       // medium graph: max surface gap (cm) for an edge
    // cavity refine (CavityRefine.h)
    float cavDx = 0.4f;        // occupancy voxel size (cm)
    float cavBand = 10.0f;     // only cavities this close to the container wall (cm)
    float cavR = 1.0f;         // tool ball and shaft radius: free space it cannot reach from outside is a cavity (cm)
    float cavCone = 70.0f;     // tool axis tilt limit from the outward direction (degrees)
    float cavObjClear = 0.3f;  // unreachable voxels count in the objective above this clearance (cm)
    float cavMinClear = 1.0f;  // anchors need a free ball this big, narrower pockets are left to small fruit (cm)
    float cavWallTol = 1.0f;   // cap fruit may stick out of the container surface by this much (cm)
    unsigned cavCap = 0;       // 1: also run the mouth cap pass (off: it found no gain on the left region)
    unsigned cavCapOnly = 0;   // 1: run only the cap pass
    unsigned cavDimple = 0;    // 1: also run the dimple fill pass (fill-fraction scored, no tool model)
    unsigned cavSnug = 0;      // 1: dimple pass uses the snug-fit rule instead of the tool gate
    unsigned cavContacts = 2;  // snug fruit must touch at least this many placed fruit
    float cavSnugFill = 0.9f;  // and fill this fraction of its volume with free space
    float cavReach = 6.0f;     // fruit only go where the free path from outside is at most this long (cm)
    unsigned cavRelocate = 1;  // 0: skip the relocate pass (placed fruit stay put)
    unsigned cavDimpleOnly = 0; // 1: run only the dimple pass
    float cavFillMin = 0.6f;   // dimple fruit must fill this fraction of its volume with free space inside the container
    float cavFillVol = 6.0f;   // and at least this much free volume (cm3)
    float cavProtrude = 0.5f;  // dimple fruit may stick out of the container surface by this much (cm)
    float cavDimpleClear = 0.8f; // dimple seeds need a free ball this big (cm)
    unsigned cavPasses = 2;
    unsigned cavTrials = 16;   // candidate poses per cavity
    float cavMinGain = 4.0f;   // min cavity volume removed (cm3) to accept a pose
    unsigned cavMaxPerPass = 0; // cap on cavities per pass, 0 = all
    bool shellEvalOnly = false;    // only report ray metrics for the loaded pack
    bool shellSwap = false;        // swap balls for fruit and nudge them inward
    unsigned swapTrials = 24;      // orientations tried per ball
    float swapForceW = 0.5f;       // Nudge direction weight vs contact attraction
    float swapMaxOverlap = 0.4f;   // max accepted penetration after nudging (cm)
    float shellFruitMargin = 25.0f;  // load fruit this far past sphereMaxX for ray queries (cm)
    // only existing fruit with box diagonal above this (cm) are obstacles and
    // support; smaller (medium) fruit already in the pack are what this
    // cover replaces, so they are ignored.
    float sphereSupportMinDiag = 14.0f;
    // field targets: when sphereFieldVoxel > 0, regenerate targets from the large
    // fruit instead of loading mediumTargetsFile. a lattice (sphereFieldSpacing cm)
    // of points where the shrinkwrap closing of radius sphereFieldRadius is
    // filled but no fruit is, at least sphereFieldMinClear cm from any fruit.
    float sphereFieldVoxel = 0.0f;
    float sphereFieldRadius = 6.0f;
    float sphereFieldSpacing = 1.0f;
    float sphereFieldMinClear = 0.5f;
    // crevice faces (CreviceFaces.h): when sphereHullFaces is set, export the
    // external convex-hull faces of small fruit groups that cover a pocket.
    bool sphereHullFaces = false;
    float hullGap = 1.5f;          // max surface gap (cm) for two fruit to be neighbors
    float hullBridgeGap = 8.0f;    // sparse two-fruit groups across wider mouths
    unsigned hullMaxNeighbors = 4; // cap wide-mouth neighbors per fruit
    unsigned hullSamples = 300;    // vertex samples per fruit
    float hullMinDepth = 1.0f;     // min pocket depth (cm) under a kept face
    float hullExclMargin = 2.0f;   // drop faces within this (cm) of the exclusion mesh
    float hullSurfaceBand = 12.0f; // only mouths this close to container surface
    float hullPatchSize = 2.0f;    // subdivision edge length for face validation
    float hullSupportDist = 12.0f; // second group fruit must be this close to a target
    // Surface-derived passage centers between opposing fruit, exported as
    // mouth points and a short same-pair graph (CreviceMouths.h).
    bool sphereMouths = false;
    float mouthNeighborDist = 12.0f;
    float mouthMaxNormalDot = -0.2f;
    float mouthOuterBand = 12.0f;
    float mouthCell = 1.5f;
    float mouthTargetWeight = 1.0f; // >1 prioritizes nearby volume targets
    float mouthGainWeight = 0.0f; // direct reward for each uncovered medium-width mouth
    unsigned mouthPlacementLimit = 0; // greedy sphere placements across mouth centers
    bool sphereFrontRequireMouth = false; // front spheres must cover a new medium-width mouth
    bool sphereAuditOpenMouths = false; // grid-search remaining mouths for feasible spheres
    unsigned sphereMouthRepairPasses = 0; // local swaps that improve mouth coverage
    // debug: only use targets and fruit with x < sphereMaxX.
    float sphereMaxX = std::numeric_limits<float>::max();
    // debug: flat x y z triples; each is probed after the cover (see SphereCover.cpp).
    std::vector<float> sphereProbes;

    // path helpers. all return absolute paths.
    std::string MeshDir() const;
    std::string ContainerPath() const;
    std::string InnerContainerPath() const;
    std::string ExclusionMeshPath() const;
    std::string MediumTargetsPath() const;
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
