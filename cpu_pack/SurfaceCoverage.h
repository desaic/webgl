#pragma once

#include "Array3D.h"
#include "VoxClass.h"
#include "VoxFootprint.h"

#include <cstdint>
#include <string>
#include <vector>

class TrigMesh;

// H1 measurement only: no placement policy lives here. Everything below
// takes a class grid, its world origin and dx, and (for the primary ray
// set) a container mesh -- never a PackingScene -- so a coverage field can
// be built and tested against a hand-built grid with no driver involved.

struct CoverageParams {
  // container surface sample spacing, cm. ~15-20k rays over ~2 m^2.
  float raySpacing = 1.0f;
  // must match the class grid's voxel size.
  float dx = 0.3f;
  // spatial hash cell size for grouping rays into neighborhoods, cm.
  float patchSize = 4.0f;
  // a ray deeper than this is "open" -- about one berry diameter.
  float holeDepth = 1.5f;
  // march cap per ray, cm.
  float maxProbeDepth = 12.0f;
  // used to place the mouth point berryRadius + dx inside the free run.
  float berryRadius = 0.5f;
  // cap on the leading run of container/inner voxels a surface ray may
  // skip before it is declared invalid. ~3 voxels of wall thickness, with
  // a little slack for the +/-0.5 cm quantization slop.
  unsigned wallSkipVoxels = 5;
};

// one ray cast inward from a container surface sample.
struct SurfaceRay {
  Vec3f origin;   // surface sample point.
  Vec3f dir;      // unit, verified to point toward free space.
  // false if neither +n nor -n reached free space within wallSkipVoxels --
  // a flipped container mesh or a ray that grazes the surface. Excluded
  // from every report statistic.
  bool valid = false;
  float depth = 0.0f;       // cm of free run before the first hit, or the
                             // capped maxProbeDepth if nothing was hit.
  uint8_t hitClass = VOX_FREE; // class of the first hit, VOX_FREE if capped.
  bool open = false;        // depth > holeDepth. Ignores hitClass by design.
  Vec3f hitPoint;           // world point of the first hit (or the capped end).
  Vec3f mouth;              // berryRadius + dx inside the free run's start.
  int patchId = -1;
  // open rays only: true if any adjacency neighbor is covered (H2's rim
  // targeting). false for interior-of-hole rays and for covered rays.
  bool rim = false;
};

// rays grouped by a spatial hash on their surface point at patchSize.
struct CoveragePatch {
  Vec3f center;                 // mean surface point of its rays.
  std::vector<unsigned> rayIdx; // indices into CoverageField::rays.
  unsigned openCount = 0;
  float meanOpenDepth = 0.0f;
  float maxOpenDepth = 0.0f;
  // counts of hitClass among rays that actually hit something (not
  // capped), indexed by VoxClass.
  unsigned hitClassCounts[5] = {0, 0, 0, 0, 0};
};

struct CoverageField {
  CoverageParams params;
  std::vector<SurfaceRay> rays;
  std::vector<CoveragePatch> patches;
  // ray i's neighbors within ~1.5x raySpacing, built once at raySpacing
  // resolution (finer than patchSize). Static: never rebuilt after a
  // placement, only the depths/rim flags it informs are.
  std::vector<std::vector<unsigned>> rayNeighbors;
  // diagnostics from the normal sign check (see CoverageParams::wallSkipVoxels).
  unsigned flippedRayCount = 0;
  unsigned invalidRayCount = 0;
};

// samples the container surface, verifies each ray's direction against the
// class grid rather than assuming the mesh normal is outward, marches every
// ray inward with a DDA, groups the result into patches, and builds the ray
// adjacency graph plus each open ray's rim/interior classification.
CoverageField BuildCoverageField(const TrigMesh &container,
                                 const Array3D8u &classGrid,
                                 const Vec3f &gridOrigin,
                                 const CoverageParams &params);

// patches within radius of patchId's center, patchId itself included first.
std::vector<unsigned> FindNeighborPatches(const CoverageField &field,
                                          unsigned patchId, float radius);

// re-marches every ray of one patch, recomputes its aggregates, and
// reclassifies rim/interior for that patch's own rays (using their existing
// neighbor list, which may span into other patches) in place. Does not
// touch any other patch's rays or aggregates -- a neighboring patch whose
// rim status depends on one of these rays goes stale until it is next
// reprobed itself, which the round-robin traversal (H2) already guarantees
// happens regularly. Used after a placement touches this patch's rays.
void ReprobePatch(CoverageField &field, unsigned patchId,
                  const Array3D8u &classGrid, const Vec3f &gridOrigin);

struct CoverageReport {
  unsigned totalRays = 0;
  unsigned validRays = 0;
  unsigned invalidRays = 0;
  unsigned flippedRays = 0;
  unsigned openRays = 0;
  float openFraction = 0.0f;

  unsigned patchCount = 0;
  unsigned patchesFullyCovered = 0; // openCount == 0.
  float openFractionP50 = 0.0f;     // percentiles of per-patch open fraction.
  float openFractionP90 = 0.0f;

  // depth histogram of open rays, bucket width max(0.5, holeDepth) cm.
  std::vector<unsigned> depthHistogram;
  float depthBucketWidth = 1.0f;

  // share of small-kind instances that are the first hit of at least one
  // ray. -1 when not computed (the primary consumer, PackingScene, is
  // outside this file's dependencies; see ComputeVisibleBerryFraction).
  float visibleBerryFraction = -1.0f;

  // patches whose deepest open ray exceeds gapingHoleDepth (BuildCoverageReport's
  // parameter, default one blueberry diameter) -- literally "a hole bigger
  // than a blueberry" on the finished, milled/printed surface. This is the
  // manufacturability metric: openFraction counts rays open past holeDepth
  // (~1.5 cm), which flags ordinary surface texture as "open" too, and
  // visibleBerryFraction rewards a berry sitting near a ray's mouth
  // regardless of whether the pocket behind it actually got filled. Neither
  // one answers "is there a gap someone would notice."
  unsigned gapingHolePatches = 0;
  float gapingHoleFraction = 0.0f; // share of non-empty patches.

  std::string toString() const;
};

// gapingHoleDepth defaults to 1.0 cm, roughly one blueberry across; pass the
// real kind's diameter when it differs.
CoverageReport BuildCoverageReport(const CoverageField &field, float gapingHoleDepth = 1.0f);

// second, independent ray set for reporting only, never for targeting:
// parallel rays from ~26 fixed directions over a lattice spanning the
// class grid's bounding sphere. If this fraction disagrees sharply with
// the primary set's, the primary set is being gamed by the sampling, not
// genuinely closing holes.
struct OrthoCoverageParams {
  float spacing = 1.5f;
  float dx = 0.3f;
  float holeDepth = 1.5f;
  float maxProbeDepth = 12.0f;
  // rays per direction is spacing-derived; this only caps it, ~26 * cap.
  unsigned maxRaysPerDirection = 4000;
};

CoverageReport BuildOrthoCoverageReport(const Array3D8u &classGrid,
                                        const Vec3f &gridOrigin,
                                        const OrthoCoverageParams &params);

// centers of the small-kind instances placed by the step under
// measurement, and how close a ray's hit point must land to count as
// hitting that instance specifically (not the class grid, which has no
// instance identity -- see VoxClass.h's resolution caveat).
struct BerryVisibilityInput {
  std::vector<Vec3f> smallInstanceCenters;
  float matchRadius = 1.0f;
};

// fraction of smallInstanceCenters that are the first hit of at least one
// ray in field (any ray whose hitClass is VOX_ITEM_SMALL, open or not).
float ComputeVisibleBerryFraction(const CoverageField &field,
                                  const BerryVisibilityInput &input);

// dumps every open ray's mouth point as an obj point cloud, for Blender
// alongside bpy_scripts/load_trajectories_inst.py.
void SaveOpenMouthsObj(const std::string &filename, const CoverageField &field);

// H2's POCKET score: how many of rayIdx (open rays only; others don't
// count as "closed" by definition) would have their mouth-to-hit segment
// blocked by footprint placed at candidatePos. A pure geometry query, no
// patch/traversal policy -- PocketPlanner builds the actual SpotScore
// scorer closure around this plus its own tie-breaks.
unsigned CountRaysBlocked(const CoverageField &field, const std::vector<unsigned> &rayIdx,
                         const VoxFootprint &footprint, const Vec3f &candidatePos, float dx);
