# Packing heuristics for surface density (final step)

Status legend: `[x]` done, `[~]` partially done, `[ ]` not started.
Companion to `improvement_plan.md`, which is about speed. This one is about
what the packer aims at. Speed work is done to the point where the final step
places ~900 berries per minute, so the open question is no longer how fast a
berry can be placed but whether that berry was worth placing.

## Goal

**Revised.** The original framing below was aesthetic ("only seen from
outside," "a viewer") and is wrong for what this actually is: the pack gets
3D printed or milled as a physical sculpture. That reframes both what a
berry is for and what the container means:

- the container mesh is a **reference boundary** used to build the packing
  algorithm (it seeds the SDF, it's where H1's rays are cast from) -- it is
  **not part of the final object** and the result does not need to hug it.
  Once this stage is running, the container's outline stops mattering.
- a berry's job is to **fill a gap so the sculpture doesn't have a deep,
  unreachable pocket** -- not to be visible from a particular vantage point.
  A deep pocket is a fabrication problem (hard to mill into, prints as an
  overhang or a void) regardless of whether anyone would ever look into it.
- a placed berry needs **real physical support from other placed items**.
  One resting only against the virtual container wall is, physically,
  indistinguishable from one resting on nothing: once printed or milled,
  that wall isn't there, so the berry falls off or the milling tool head
  has nothing to reference. This is a **hard requirement**, not a quality
  tradeoff -- see H3's revision below, which was landed mid-session after
  this correction (before it: pushed outward against the container wall,
  the opposite of what support requires).
- a berry buried behind a big fruit, or dropped into the middle of a void two
  hand-widths deep, is still wasted: it costs the same and changes nothing.
- **a shallow slope on the surface is fine as-is and does not need filling**
  -- it is easy to 3D print or mill, so there is no fabrication reason to
  chase it (and, as above, no aesthetic reason either). Only *deep* pockets
  are a problem. This is the corrected version of "stop when the surface
  stops improving": the step should stop once every deep pocket is filled,
  not at some global density percentage or improvement rate -- see H4's
  revision below.
- the step should still stop when there is no deep pocket left to fill, not
  when the geometry runs out of room. Total berry count is an output, not a
  target. The 6000 figure in `improvement_plan.md` is an estimate of what
  filling every reachable deep pocket takes, and if none are left at 3000
  the step is finished.

Method, in one sentence: cast rays inward from the container surface (a
reference frame for finding pockets, not a surface the result must touch)
to find deep pockets, group them into neighborhoods, place a berry in the
mouth of any neighborhood with a deep pocket **and settle it against real
neighboring material**, and retire a neighborhood once its rays already
land on berries.

**Revised again, H6/H7.** The paragraph above said the ray-casting mechanism
itself does not need to change -- that turned out to be wrong, but not for
the "deep pockets don't matter" reason above. A ray stops at the first thing
it hits, so it cannot measure a gap sitting *right behind* that first hit
even when the gap is not deep at all by the definition above -- still near
the surface, one fruit-width in. H6's melon test measured this directly:
blueberries ended up in their own crust between the container wall and the
outermost fruit, with zero contact to any fruit, because that outer layer
is the only thing rays ever saw. So H1/H2's ray/patch mechanism is being
replaced by a volumetric free-space scan (H7) -- deliberately still scoped
to near-surface only, same as this section's "deep pockets don't matter"
point, just found by a mechanism that can see past the first occluder
instead of one that cannot.

What changes across H3/H4 stays as before (settle against real material,
stop when no deep pocket is left) -- H7 replaces *how a pocket is found*,
not what counts as one or when to stop looking. The *interpretation* of
"visible berry fraction" (H1's metric) also stays revised: better read as
"berry actually plugged an open gap" than "berry is visible to a viewer" --
though H6 showed that metric can still look fine while the underlying
placement is wrong, which is why H6 added the `gapingHoleFraction` metric,
and H7 replaces the detection those metrics are measuring the output of.

## What the final step does today, and why it does not serve that goal

Step 4 of the plan (`PackingPlan.cpp:228-234`): all 10 small kinds,
`count = 1000000`, `outwards = false`, `useInnerContainer = true`,
`force = (-0.1, 0, 0)`.

The driver runs it through the same `PackStep` as every other step
(`PackingDriver.cpp:219-249`):

1. **Search targets are a blind lattice walk.** Each kind holds a cursor
   `item.nextCellIdx` into the 10x3x3 = 90 subgrid cells of 20 cm, tries
   `maxTrialCount` random rotations in the current cell, and advances only when
   all of them fail. Nothing in that walk knows where the surface is or where
   the holes are. Interior cells get the same effort as boundary cells. This is
   already flagged at the end of `improvement_plan.md` 5.1 as worth revisiting
   and it is the core issue here.
2. **The only spatial preference is a global SDF sign plus a corner pull.** The
   score in `FindSpot` is
   `factor * sdf->GetCoarseDist(coord) + positionWeight * (x + y + z)` with
   `positionWeight = -1.0f` (`PackingOps.cpp:90`). With `factor = -1` for an
   inward step this prefers spots near the container wall, which is the right
   instinct, but it is measured against the *container* surface only. A berry
   sitting 0.5 cm inside the wall and completely shadowed by a melon scores the
   same as one plugging an open gap.
3. **There is no stopping criterion tied to appearance.** The step ends when
   all 10 kinds have failed all 90 cells, or on `maxSecondsPerStep`. Both are
   unrelated to how the sculpture looks.
4. **The settle can undo the placement.** `placeItem` pushes with
   `ForceDirection(..., sdfFactor = -1, ...)` (`PackingDriver.cpp:183`), i.e.
   inward, then `Put`s wherever `Nudge` ends up. For a pocket-filling berry the
   desired motion is the opposite: wedge outward into the mouth of the pocket.
   A berry that was found at the visible layer and then settles 4 cm inward has
   been converted from useful to wasted, and nothing checks for that.
5. **`useInnerContainer` is a crude stand-in for all of the above.**
   `AddInnerContainer` (`PackingScene.cpp:1273`) unions an inner shell into
   `bg.vox` so the middle of the container is unavailable. That keeps berries
   in a shell near the surface, but the shell is a fixed offset surface, not a
   visibility test, and it says nothing about which parts of the shell need
   berries.

Nothing above is measured yet. Everything in this plan is prioritized against
the baseline from H1, exactly as phase 5 of `improvement_plan.md` was
prioritized against the profiler.

## H0: a class grid, so a ray can tell what it hit

`bg.vox` is binary. `InvertContainer` (`GridUtils.cpp:128`) writes 1 for
everything outside the container, `Union` (`MeshConvo.cpp:92`) writes 1 for
every packed item voxel, and `AddInnerContainer` writes 1 for the inner shell.
So the occupancy grid cannot answer either question this plan needs: "is the
first thing this ray hits a berry or a melon" and "where does the container
wall end and the fruit begin".

### What the class grid is for

Four consumers, and each one is a thing the current code cannot do:

1. **Wall skipping in the depth march (H1).** A ray starts at a container
   surface sample, and the voxels there are already 1 in `bg.vox` because
   `InvertContainer` marks the exterior. Without classes the march cannot tell
   "I am still in the wall, keep going" from "I hit a fruit, stop", so every ray
   would report depth 0 and the whole surface would look perfectly covered.
   Skipping a *fixed* voxel count instead would be wrong in the other
   direction: it would step over a berry that is resting in the wall region.
2. **The retirement rule (H4).** "Stop adding berries where the neighborhood is
   already all berries" is exactly a query for the class of each ray's first
   hit. Class 3 everywhere in a patch means the visible layer there is berries
   and more berries only stack; class 2 means a big fruit forms the visible
   surface there and the patch was never the final step's job.
3. **The visible berry fraction (H1).** The waste metric needs to attribute a
   first hit to a *berry* specifically. Instance identity is not needed, only
   the size class, which is why one byte per voxel is enough and no instance-id
   grid is required.
4. **Diagnostics.** Class slices via `SaveSlice` make "wall vs big fruit vs
   berry vs inner shell" readable in one image, which is the fastest way to see
   that the shell is doing something unintended or that berries are landing
   somewhere absurd.

Not a consumer: collision. `bg.vox` stays the single source of truth for free
space, and `FindSpot` / `FindSpotBox` keep reading it. The class grid is
metadata only, so a bug there cannot produce an overlapping placement -- it can
only produce bad targeting, which the H1 metrics will show.

### Resolution caveat: a voxel is a third of a berry

`dx` is 0.3 cm because 616x192x192 is already 21.7 MB for `bg.vox` and the FFT
work scales with it; the class grid doubles that. Halving `dx` would be 8x both
numbers, which is why the resolution is what it is. The consequence, already
noted in `improvement_plan.md`: **the smallest kinds are 0.85 to 3.0 cm, i.e.
3 to 10 voxels across, so a blueberry is only 3-5 voxels wide.** Implications
this plan has to respect:

- a berry occupies roughly 30-100 voxels, so its class footprint is a blob a
  few voxels wide. A ray can miss a berry that is genuinely in its path by
  landing in a corner voxel that voxelized empty. Expect a few percent of ray
  classifications to be wrong and do not build any rule that needs a single ray
  to be right -- every decision in H4 is over a patch of 15-30 rays for this
  reason.
- depth is quantized to 0.3 cm and the wall itself is 2-3 voxels thick, so
  reported depths carry roughly +/- 0.5 cm of slop. `holeDepth` at 1.5 cm is 5
  voxels, which clears that; anything under ~1 cm would be measuring
  voxelization noise.
- adjacent berries can share a voxel of class, so a class-3 region cannot be
  counted to recover a berry count. Berry counts come from `instances`, never
  from the grid.
- last writer wins per voxel. A berry touching a melon overwrites a shared
  boundary voxel, which is harmless for all four uses above but means the grid
  is not a partition of the instances.

- [x] new `Array3D8u classGrid` on `PackingScene`, same size and origin as
  `bg.vox`, values `0` free, `1` container (exterior plus wall), `2` big or
  medium item, `3` small item, `4` inner container shell. One byte per voxel at
  616x192x192 = **22.7 MB**, the same order as `bg.vox` at 21.7 MB, which is
  affordable next to the 356 MB RSS measured in `improvement_plan.md`.
  Landed as `VoxClass.h` (enum plus `VoxClassOfExtent`) and
  `PackingScene::classGrid` (`PackingScene.h`).
- [x] fill `1` in `PrepareBackground` from the same pass that calls
  `InvertContainer`, `4` in `AddInnerContainer`. Via new `ClassFromBinary`
  (seeds `VOX_CONTAINER`, `PackingDriver.cpp:PrepareBackground`) and
  `StampClass` with `overwriteMask = 1u << VOX_FREE` so an already-placed
  item's class is never clobbered by the shell (`PackingScene.cpp:AddInnerContainer`).
- [x] `Put` (`PackingScene.cpp:29-61`) writes its class alongside the `Union`,
  with item classes overwriting container and inner-shell classes. A berry
  resting in the wall region must read back as a berry or the depth measurement
  in H1 mis-attributes it. Class comes from the item's max extent against the
  same `SIZE_THRESH` 3 cm boundary `PlanPackingSteps` uses, so the two
  definitions of "small" cannot drift apart. Implemented with an unconditional
  `overwriteMask = ~0u` (last writer wins, per the resolution caveat below);
  guarded by `classGrid.GetSize() == bg.GridSize()` so `Put` calls before
  `PrepareBackground` allocates the grid are a no-op rather than a crash.
- [x] `LoadPack` gets the same treatment for free, since it goes through `Put`.
  No change needed, confirmed by inspection.
- [x] add the grid to `MeasureSceneMemory` so it shows up in the bench memory
  table rather than as unexplained RSS. New `SceneMemory::classGrid` row;
  confirmed at 21.66 MB (matching `bg.vox`) in the `smallfruit_packstep` bench.

## H1: measure the surface first, change nothing

The coverage metric is the whole plan's yardstick, so it is built and read
before any heuristic changes. New `SurfaceCoverage.h/.cpp`.

Ray set:
- [x] sample the container surface at ~1 cm spacing with outward normals.
  Landed as a direct call to the existing `SamplePoints(container.mesh,
  raySpacing, pts)` (`SurfaceCoverage.cpp:BuildCoverageField`) -- it already
  spatial-decimates at `eps` and returns per-triangle normals, so no
  `PointSample.cpp` change was needed; the index-returning variant the plan
  proposed turned out to be unnecessary. A `smallfruit_packstep`-adjacent
  loaded pack gave **48444 rays**, above the 15-20k estimate (the container's
  actual triangle density is finer than the 31k/2m^2 average suggested).
- [x] verify the normal sign against `classGrid`, but **per ray, not once
  globally** as literally written here: `container.mesh.nv` is a per-vertex
  normal computed before `ReverseWinding` in `InitDataStructures`
  (`PackingScene.cpp:114-115`), while `SamplePoints`' per-triangle samples use
  a face normal computed from the *current* (post-reverse) winding, so the two
  can disagree within the same call. `MarchInward` tries `-n` first, falls
  back to `+n`, and marks the ray invalid if neither reaches free space within
  `wallSkipVoxels` -- this subsumes the single global flip check and is
  robust to the per-sample inconsistency instead of assuming it away. On the
  same loaded pack, **30253 of 48365 valid rays needed the flip**, i.e. the
  naive single-direction assumption would have been wrong for the majority of
  rays; only 79 were invalid.
- [x] march each ray inward with a DDA over `classGrid` at `dx` 0.3, skipping a
  leading run of class 1 or 4 voxels capped at `wallSkipVoxels` (5, not the
  literal 3 -- the resolution caveat's +/-0.5 cm slop is 1-2 voxels of margin,
  so 3 was cutting it close), then counting free voxels until the first class
  2 or 3 voxel, up to `maxProbeDepth` (12 cm default).
- [x] per ray record `depth`, `hitClass`, and the world point at the pocket
  mouth. Also recorded (not in the original list, needed for visible berry
  fraction below): `hitPoint`, the actual first-hit world point.
- cost: confirmed cheap. `coverage.march` for the primary set took **28-800
  ms** depending on ray count in the bench run below, not the "few ms"
  estimate, because `MarchInward` runs twice per ray in the worst case (the
  sign check) -- still negligible next to a placement step.

Neighborhoods:
- [x] group rays by a spatial hash on their surface point at `patchSize`
  (4 cm default). **2325 patches** on the loaded-pack test, above the
  1000-1500 estimate (tracks the higher ray count above). Per patch:
  ray count (`rayIdx.size()`), open ray count, mean and max open depth,
  first-hit class histogram. `berries placed by this step` and `consecutive
  failed searches` are H4/H2 policy state with no meaning yet and are not on
  `CoveragePatch`; `retired flag` likewise waits for H4. Adding them now would
  be state nothing reads, which is exactly what H0/H1 additive is supposed to
  avoid.
- a ray is **open** when `depth > holeDepth` (1.5 cm default).
- [x] openness ignores `hitClass` (`SurfaceRay::open` is set from `depth`
  alone in `FillRayFromMarch`).

Metrics and diagnostics:
- [x] `CoverageReport`: open ray fraction overall and per-patch p50/p90, the
  depth histogram of open rays, **visible berry fraction**, and a
  covered/not-covered patch split (`patchesFullyCovered`). "Patch count by
  state" beyond that waits for H4's retirement states, same reasoning as
  above. Visible berry fraction cannot be computed inside `SurfaceCoverage`
  itself (it must not depend on `PackingScene`/instances per the file
  boundary below), so it is a separate `ComputeVisibleBerryFraction(field,
  BerryVisibilityInput)` the caller runs with the placed instances' centers;
  `BenchCoverageBaseline` (`BenchPocket.cpp`) does this. Matching is by
  proximity (a spatial hash on instance centers, radius-matched against each
  ray's `hitPoint`) rather than an instance-id grid, per the H0 caveat that
  the class grid must never be asked to recover instance identity.
- [x] dump the open ray mouths as an obj point cloud. Used `SaveVec3fObj`
  (`SaveOpenMouthsObj`), not `SavePointsObj` -- the mouth points have no
  natural normal to draw, so the point-with-normal-segment format didn't fit.
  Plus classGrid slices via the existing `SaveSlice`, both wired into
  `DebugTools::DebugCoverage` and `BenchCoverageBaseline`.
- [ ] print the report at the start and end of the final step, and on the 2 s
  heartbeat already in `PackStep`. **Deliberately skipped for H1.** The H1
  phase table below only lists `BenchRegistry`/`BenchReport`/`benchmarks.cpp`/
  `DebugTools` as changed files -- not `PackingDriver.cpp` -- and the loop
  this would instrument is `PackStep`'s lattice walk, which H2 replaces
  wholesale for the final step rather than wrapping. Wiring this print into
  code that is about to be deleted isn't worth the risk of touching the hot
  loop before H2 lands; `coverage_baseline` and `DebugCoverage` give the same
  numbers on demand today.

- [~] **baseline run**: mechanism proven -- `coverage_baseline` run against
  `pack_944_0721.txt` (944 instances, only 6 small-kind so far) gave open ray
  fraction 0.444, visible berry fraction 0.333. Not yet run against a fully
  packed step-4 output to get the real "open/visible fraction vs. berry
  count" curve the plan wants; that needs a resumed pack file with the small
  step actually run out (e.g. the 1741-instance pack `smallfruit_packstep`
  produces), which isn't wired to `coverage_baseline`'s resume path without
  editing the tracked `pack_fruits.cfg`. Left for whoever runs the real sweep
  rather than guessed at here.

Independence check, because the metric is also the objective and a self-graded
score is worth little:
- [x] a second, independent ray set for reporting only -- `BuildOrthoCoverageReport`,
  all 26 axis/diagonal directions, ~1.5 cm lattice per direction
  (`OrthoCoverageParams::spacing`). **103194 rays** on the same test run,
  matching the ~100k target; open fraction 0.557 against the primary set's
  0.444 -- same ballpark, not the sharp divergence that would indicate the
  primary set is gaming the sampling (expected at this stage anyway, since
  nothing is driving placement from the metric yet).
- [ ] eyeball at least one render per configuration. Not automatable here;
  `coverage_open_mouths.obj` and the two `classGrid` slice PNGs are dumped by
  both `DebugCoverage` and `coverage_baseline` specifically so this step has
  something to look at.

## H2: drive placement from pockets instead of from the lattice walk

New code path for the final step only, so every earlier step keeps its current
behavior bit for bit. Landed as `PackPocketStep` in `PocketDriver.cpp`/`.h`,
called directly (not yet gated by a `PackingStep` flag -- that plumbing is
H5) from the new `pocket_fill` bench scenario. The outer loop is over
*pockets*, not over kinds and cells, matching the sketch below with one
difference: `AcceptSettle` runs before `Put`, exactly as sketched, but
`ReprobeLocal` reprobes the touched patch's *neighbors* (via
`FindNeighborPatches`) rather than the patch itself a second time, since
`AttemptPlacement`'s own patch was already reprobed as part of accept/reject.

```
while (!StopStep()) {                      // H4 (backstops only, see below)
  patch = NextPatch()                      // traversal order, below
  if (!patch) break
  ray   = PickTargetRay(patch)             // rim first, below
  kind  = PickKind(patch, ray)             // size matched, below
  spot  = FindSpotLocal(box(ray, kind), ...) // local search, below
  if (!spot) { patch.fails++; continue }
  settled = Nudge(...)                     // H3
  if (!AcceptSettle(settled, ray, patch)) { patch.rejects++; continue }
  Put(...); ReprobeLocal(patch)            // H3
}
```

**Measured result** (`pocket_fill --items 50 --validate` against the same
944-instance pack `coverage_baseline` used for the H1 baseline): 187 placed
in 437 attempts, exit reason "diminishing returns". Visible berry fraction
went from **0.333 to 0.736** and open ray fraction from 0.444 to 0.418 for
187 berries -- 6.66 open rays closed per berry. `--validate`: 0 failures,
worst overlap 0.6 cm, worst outside 0.6 cm, mean overlap 0.289 cm, matching
the lattice walk's own baseline tolerances from `improvement_plan.md`. The
visible-berry-fraction jump is the number this whole plan is aimed at, and
it more than doubled with only 187 placements against a 944-instance pack.

### Traversal order over patches

- [x] priority `openRayCount * meanOpenDepth`. Ray count alone favors big flat
  patches; depth alone favors one deep crevice. `PocketPlanner::RebuildOrder`.
- [x] visit **round robin over the surface**, not worst patch first: eligible
  patches (open rays > 0, under `patchMaxFails`) are ranked by priority, the
  top `1/priorityTiers` (5 default) fraction is taken as this pass's tier,
  sorted by patch-center x, then walked with a coprime stride
  (`CoprimeStride`) so consecutive visits jump far in x. One pass = at most
  one attempt per patch in that tier; `NextPatch` rebuilds (re-tiers) once the
  deque empties. Deviation from the literal spec: only the *top* tier is
  walked per pass, not "the highest non-empty tier" checked against all 4-5 --
  since eligibility already filters to patches with open rays, the top tier
  by construction is always non-empty when eligible is non-empty, so the
  other tiers are never actually needed for this check and were dropped.
- [x] a patch that fails goes to the back of the *current* pass (pushed onto
  `order_`'s tail in `RecordFail`) rather than retried immediately -- but only
  while `consecutiveFails < patchMaxFails`; past that it drops out of the
  deque and waits for the next pass's rebuild. Rejects are folded into the
  same fail-count path (`PocketDriver.cpp` calls `RecordFail` on every reject
  branch), rather than tracked as a separate reject counter.
- [x] deterministic given the seed: patch order derives from patch index,
  current-pass priority ranking, and a fixed stride formula only -- no RNG.
  Rotations come from `scene.randAngles` indexed by trial number, per H2's
  own rotation-trial bullet below.

### Which ray in the patch to aim at: rim first

Aiming at the *deepest* ray centers the berry in the hole and leaves a ring of
open rays around it -- the failure mode discussed in H3. Aim at the edge of the
hole instead:

- [x] ray adjacency graph, `SurfaceCoverage.h/.cpp`'s `BuildRayAdjacency`
  (called from `BuildCoverageField`): a hash at `raySpacing` resolution (finer
  than `patchSize`), each ray linked to neighbors within 1.5x `raySpacing`.
  Stored as `CoverageField::rayNeighbors`, static after build -- see the
  incremental-update caveat below.
- [x] rim/interior classification, `ClassifyRim`/`ClassifyRimRay`:
  `SurfaceRay::rim` true iff the ray is open and any neighbor has
  `depth <= holeDepth`. Re-run for a patch's own rays inside `ReprobePatch`
  after a placement -- but **not** for neighboring patches whose rim status
  depends on one of those rays; that goes stale until *that* patch is itself
  reprobed. Accepted per `SurfaceCoverage.h`'s own comment: the round-robin
  traversal guarantees regular revisits, so staleness self-corrects rather
  than needing an immediate cross-patch fixup.
- [x] `PocketPlanner::PickTargetRay`: rim ray with the greatest depth, else
  the deepest interior ray, else -1 (no open rays).
- [x] fallback to interior ray covered by the same function (no separate
  code path needed).
- [x] target point = `SurfaceRay::mouth`, already computed as `berryRadius + dx`
  inward from the free run's start when the ray field is built (H1).

### Which kind to place: size matched to the mouth

- [x] mouth diameter estimate: `PocketPlanner::PickKind`, distance to the
  nearest neighbor with `depth <= holeDepth`, doubled, clamped to
  `[smallMin, smallMax]` = `[0.85, 3.0]` cm. Falls back to `smallMax` (not a
  measured value) when the target ray has no covered neighbor at all --
  matches the "hole wider than the patch" case already falling back to an
  interior ray above.
- [x] largest kind that fits, tie-broken by least used, implemented exactly as
  specified, plus the share cap folded into the same function rather than a
  separate "if it collapses" escape hatch: `kindShareCap` (0.25 default) is
  enforced on every pick, not just when a collapse is detected after the
  fact, then a second least-used-regardless-of-cap pass runs if every fitting
  kind is over cap (so a narrow mouth still gets a placement rather than none).
- [~] per-kind histogram reporting: not added to `pocket_fill`'s output yet.
  `PocketPlanner::kindUseCount_` already has the data (needed for the share
  cap); exposing it is a small follow-up, not a design gap.

### The local search: `FindSpotBox` and `FindSpotLocal`

- [x] `FindSpotBox(bg, part, pos, rot, score, searchBox, shrink, outCenter)`
  in `PackingOps.cpp`, split out of `FindSpotSubgrid` exactly as specified:
  the crop/search/container-boundary logic moved into `FindSpotBox`,
  `FindSpotSubgrid` reduced to computing the cell box and calling it, then
  redoing its own cell-boundary check using an `outCenter` out-param
  `FindSpotBox` reports (the settled rotated-mesh center) so the wrapper does
  not need to recompute the shrink/rotation itself. Confirmed
  behavior-preserving: `smallfruit_packstep --validate` after the split still
  reports 0 failures, worst overlap 0.6 cm, mean overlap ~0.319 cm, matching
  the pre-split numbers.
- [x] search box = target point +/- `(itemExtent/2 + slack)`, `slack` = 1 cm
  default (`PocketStepConfig::searchSlack`).
- [x] **direct voxel overlap test as the primary path**: `FindSpotLocal`
  (`PackingOpsLocal.cpp`), exhaustive over the dx lattice within the search
  box, early-out on the first blocked footprint voxel, free-space predicate
  exactly `bg.vox == 0`. Landed as the **only** path for this driver -- the
  "keep the FFT path for boxes above a threshold and for the medium kinds"
  branch does not exist, because `PackPocketStep` only ever handles the
  small-kind catalogue (by construction: `step.names` is filtered to
  `VOX_ITEM_SMALL`), so the medium-kind case this branch existed for cannot
  occur through this entry point. Revisit if a caller ever feeds
  `PackPocketStep` a medium kind.
  - [x] footprint caching per (kind, rotation): `VoxFootprint.h/.cpp`,
    keyed by `(kindId, rotIdx)` where `rotIdx` is the caller's stable index
    into `scene.randAngles` -- also serves `improvement_plan.md` 5.4's
    `fg_voxelize` cache need, per the plan's intent.
  - [x] free-space predicate parity with the FFT path: both read `bg.vox == 0`
    per voxel by construction (same array, same condition), so there is
    nothing to drift -- this is true by inspection rather than by a
    cross-validation harness.
  - [ ] **not done**: the "run both on a few hundred real cases and assert
    the free/blocked verdict matches" cross-validation test. Given the
    predicates are textually identical reads of `bg.vox`, the risk this
    guards against (a silent disagreement producing overlapping fruit) is
    covered by `--validate`'s overlap check instead, which passed (0
    failures) on the `pocket_fill` run below. A dedicated offset-by-offset
    comparison test would still be worth writing before trusting this at
    scale.
- [x] the boundary checks: the container-boundary check is **not needed** for
  `FindSpotLocal` (unlike `FindSpotBox`) -- `bg.vox` already reads 1 outside
  the container (`InvertContainer`), so a footprint that pokes out is already
  caught by the overlap test itself, with no separate check required. The
  **patch check** (settled center within patch + 1 patch of slack) is
  implemented in `PocketDriver.cpp` as part of H3's accept/reject on the
  *settled* position, not inside the search -- see H3.
- [x] the shrink is a parameter (`FindSpotBox`'s `shrink` argument, default
  0.5 preserving `FindSpotSubgrid`'s exact behavior). `FindSpotLocal` has no
  shrink at all (direct overlap needs none: it tests the real footprint, not
  an FFT collision map), which is a stronger version of "set it to zero for
  this path" -- there is no shrink code path to set to zero because the
  search that needed shrinking isn't used here.

### Scoring: maximize rays closed, not proximity

- [x] `SpotScore` mode `POCKET`: landed as a generic `scorer(disp, coord)`
  callback on `SpotScore` (`PackingOps.h`) rather than a hardcoded ray-count
  formula inside `FindSpot`/`FindSpotLocal` -- keeps `PackingOps.h` free of a
  `SurfaceCoverage`/`VoxFootprint` dependency for its many existing
  non-pocket callers. The actual ray-count query,
  `CountRaysBlocked(field, rayIdx, footprint, candidatePos, dx)`, lives in
  `SurfaceCoverage.h/.cpp` (a geometry query over rays + a footprint, which
  fits that file's charter) and is what `PocketDriver.cpp` wraps into the
  `scorer` closure it hands to `FindSpotLocal`. "Mouth segment blocked" is
  tested as a marched line from `ray.mouth` to `ray.hitPoint` in dx steps
  against the footprint's local voxel space, not a single point test.
- [~] tie-breaks: only **distance to the target point** is implemented
  (`score = closedCount * 1000 - distanceToTarget` in `PocketDriver.cpp`).
  "Fewer voxels deeper than the mouth" and "more already-occupied adjacent
  voxels" (contact preference) are not implemented -- the second needs
  exactly the contact-count plumbing H3 defers (no `NudgeResult` yet), and
  both are true secondary tie-breaks (only matter when ray-count ties), so
  their absence should not change which candidate wins in the common case.
  Revisit once H3's contact counting lands.
- [x] this replaces the corner-pull expression for this path only -- the
  6-argument `FindSpot(bg, part, pos, rot, sdf, factor)` overload (used by
  every other step) is untouched and still hardcodes `positionWeight = -1.0f`
  via a `SpotScore` it builds internally; only callers that construct their
  own `SpotScore` (`FindSpotLocal`'s callers) can reach `POCKET` mode.
- [x] rotation trials: `PocketStepConfig::pocketTrialCount` defaults to 3.
  Not yet confirmed against the placement rate for the non-isotropic small
  kinds (blackberry, raspberry) specifically -- the 187-placement run below
  used all 10 small kinds together and did not break that out per kind.

### Bookkeeping that must not be reused

- [x] `item.nextCellIdx` / `item.noMoreFit` are untouched: `PocketPlanner`
  and `PocketDriver.cpp` never read or write `MeshInfo` fields, only
  `PocketPatchState` (per-patch, in `PocketPlanner`) and `VoxFootprintCache`
  (per kind+rotation, local to the call). Retirement-by-kind cannot happen
  because nothing here has a per-kind retirement concept at all.
- [x] `PackStep`'s round-robin loop is **not replaced**, it is left fully
  intact; `PackPocketStep` is a parallel, independent entry point
  (`PocketDriver.cpp`). `PackingDriver.cpp`'s `PackScene` dispatches to it via
  `PackingStep::fillMode` (H5, landed): `FillSurface` calls the new
  `PackSurfaceStep` wrapper, everything else still calls `PackStep`
  unchanged. The "reuse only `placeItem`'s body" factoring
  (`CommitPlacement` / `MaybeSaveProgress`) also did not happen:
  `PackPocketStep` duplicates the handful of lines it needs (`Put` +
  trajectory recording) directly rather than sharing code with
  `PackingDriver.cpp`'s `placeItem` lambda, since the two paths' settle step
  differs enough (custom H3 push blend vs. `ForceDirection`) that the shared
  surface would have been just those few lines anyway. Periodic saves
  (`trajSaveInterval`/`packSaveInterval`) are not implemented in
  `PackPocketStep` at all -- only `pocket_fill`'s own end-of-run
  `SaveInstances` dump exists today.

## H3: settle against real material, not against the container wall

**Revised mid-session.** The plan originally written here pushed the berry
*outward*, against the container wall, on the theory that the container
boundary was the visible surface and hugging it was the goal (see the old
version of this section preserved in git history at commit `6338c4b` if
needed). That theory is wrong for what this object actually is: **the pack
is 3D printed or milled as a physical sculpture, the container mesh is a
reference boundary that is not part of the final object, and the container's
outline stops mattering at this stage** (see the Goal section's revision).
Pushing outward produces exactly the failure mode this correction fixes: a
berry resting only against a wall that will not physically exist, which
prints/mills as a loose piece -- it "would just fall off or is unmillable
with a tool head," in the words that triggered this revision.

What actually matters, unchanged: a berry's job is still to close a deep
pocket (fabrication problem, not an aesthetic one -- a pocket a tool head
can't reach is a problem whether or not anyone would ever see it). What's
new: a placed berry must have **real physical support from other placed
items**, checked as a hard requirement, not inferred from geometry.

### The corrected push: toward real material, never the container

- [x] `PocketDriver.cpp`'s `PocketPushDirection`, rewritten: push toward
  whichever nearby solid material is closest, restricted to items
  (`hitClass == VOX_ITEM_BIG` or `VOX_ITEM_SMALL`), **never the container**.
  Priority: (1) the shallowest neighbor ray in `rayNeighbors[targetRayIdx]`
  whose hit is an item, using that neighbor's actual obstruction point
  (`hitPoint`) as the target, not just its surface origin -- a genuine 3D
  direction toward material, not a direction tangent to the (now irrelevant)
  container surface; (2) if no such neighbor exists, the target ray's own
  current obstruction (`ray.hitPoint`), i.e. push toward whatever this
  specific pocket already hits, deeper in; (3) if neither exists (an
  isolated void with nothing nearby within `maxProbeDepth`), push nothing
  and let `Nudge`'s own internal contact-seeking (`attractionDir`,
  `PackingScene.cpp:643` region) find something on its own -- there is
  nothing in the ray field to aim at in that case, and inventing a direction
  would be worse than none.
- [x] this still bypasses `ForceDirection` entirely rather than extending
  it, and `step.force` as a bias is still not implemented -- both notes from
  the original writeup hold, just with a different function body.
- [x] `NudgeResult` **landed** (not deferred as the original plan called
  for): `PackingScene.h`/`.cpp` gained the struct and `Nudge` a defaulted
  `NudgeResult *out = nullptr` parameter, filled from the last contact-gather
  pass's `Contact::meshIndex` compared against that step's
  `intersectingInstances.size()` (contacts at an index below that count are
  with other items; at or above are the container/inner-container grids,
  appended after every item grid -- see `Nudge`'s broadphase-to-narrowphase
  loop). All 5 existing callers were unaffected by the added defaulted
  parameter -- confirmed by rebuild, no signature changes needed at any call
  site. This was promoted from "deferred, nice-to-have diagnostic" to
  "required for the accept decision" by the correction above: a warning
  isn't good enough if the physical consequence is a piece that doesn't
  print.
- [ ] the two-stage settle escalation: still not implemented, and now less
  clearly motivated -- the corrected push already aims at real material
  directly rather than sliding tangentially along a wall, so the specific
  failure mode (ring gap from a single wall contact) it was designed for
  doesn't really apply in the same way. Revisit only if the item-contact
  reject rate (measured below, currently low) rises.

### Accept or reject a settled placement

`Nudge` returns a transform and only `Put` commits it, and `Nudge` mutates
nothing but the `kindGrids` cache, so a settled placement can be dropped for
free -- confirmed unchanged and relied on directly in `PocketDriver.cpp`.

- [x] **reject on zero item contact -- hard reject, not a warning.** This is
  the corrected version of the plan's original "accept with a warning" bullet:
  given the container isn't part of the final object, contact with it alone
  is not support, so `nudgeResult.itemContactCount == 0` is now a hard reject
  (`PocketStepResult::rejectedNoContact`), checked first, before any of the
  distance-based rejects below.
- [x] reject on inward sink: `inwardSink = moved.dot(rayDir)` where `rayDir`
  is the target ray's own (inward) direction -- same formula as the original
  plan's `n`-based version, just expressed directly rather than through a
  wall-normal variable that no longer exists in the push logic. Rejected
  past `itemMaxExtent` (one berry diameter).
- [x] reject on leaving the patch: settled center vs. `patch.center`,
  threshold `2 * patchSize` (patch + one patch of slack). Unchanged by this
  revision.
- [~] reject on "still open": unchanged from before this revision -- still
  the distance-proxy approximation (settled center more than
  `itemMaxExtent + searchSlack` from the target mouth), not the literal
  re-probe (which needs a commit/undo the grid has no path for). Still the
  largest approximation in H3.
- [x] count rejections by reason: `PocketStepResult::rejectedNoContact` /
  `rejectedInwardSink` / `rejectedLeftPatch` / `rejectedStillOpen`, reported
  by `pocket_fill`.

**Measured effect of the correction** (`pocket_fill --items 50 --validate`,
same 944-instance pack as the pre-correction run): 190 placed in 426
attempts (was 187/437) -- rejects: no item contact 12 (2.8% of attempts),
inward sink 58 (up from 4 -- expected, since pulling toward material now
actually risks over-sinking, where pulling toward a virtual wall never did),
left patch 0, still open (approx) 142 (down from 218 -- fewer placements
drift far from the mouth now that nothing pushes them away from it toward a
wall). `--validate`: 0 failures, worst overlap 0.6 cm, **worst outside
container dropped from 0.6 cm to 0.3 cm** (berries are no longer being
pushed to poke against/through the boundary), mean overlap 0.311 cm. Visible
berry fraction 0.719 (was 0.736 pre-correction) -- similar, and now backed by
a guarantee (every accepted placement has real item contact) that the old
number did not carry. Rays closed per berry 6.82 (was 6.66). The correction
cost almost nothing in the metric that matters and fixed a placement that
would not have survived fabrication.

### After a commit

- [x] re-cast only the touched patch and its neighbors: `PocketDriver.cpp`
  calls `ReprobePatch` on the touched patch (inside the accept/reject checks
  above, which need the patch's current state) and then again via
  `FindNeighborPatches(patchId, patchSize*1.5)` after a successful commit.
  The resulting drop in open ray count is not read back into a per-placement
  log line, only into the next loop iteration's `CoverageReport` (used for
  the diminishing-returns stop, H4) and the before/after report `pocket_fill`
  prints at the end of the run.
- [~] per-placement record of rays closed / contact counts / settle
  distance: only the aggregate (before/after `CoverageReport`, reject counts
  by reason) is recorded, not a per-placement log line. `placeItem`'s
  existing per-placement `LOGI` (settle distance, cell, instance id) is not
  replicated for the pocket path -- `PackPocketStep` has no logging at all
  during the loop, only the summary `PocketStepResult` at the end. Contact
  counts are now available (`NudgeResult`, landed above) but still only
  checked against the reject threshold, not logged per placement.

## H4: stopping criteria

**Revised mid-session**, in plain words from the person actually building
the physical object: stop adding small items once **every deep pocket is
filled**. A shallow slope on the surface is fine left as-is -- it is easy to
3D print or mill, so there is no fabrication reason to chase it, and (per
the Goal section's revision) there is no aesthetic reason either. The
original `targetOpenFraction`/diminishing-returns design below was a
proxy for this that turned out to be the wrong shape: both stop on a global
rate or percentage, which can trigger while real, fillable deep pockets
remain (stopping too early) or fail to trigger on a container that is
mostly harmless shallow terrain sitting at a nonzero open fraction forever
(not actually a stopping signal at all). Replaced with a criterion that
means literally "no deep pocket is left to fill":

- [x] **eligibility now gates on depth, not just openness**:
  `PocketPlannerParams::deepPocketThreshold` (3.0 cm default, deliberately
  larger than `SurfaceCoverage`'s `holeDepth` of 1.5 cm) is the line between
  "shallow slope, leave it" and "deep pocket, worth an attempt."
  `RebuildOrder`'s eligibility filter now excludes a patch whose
  `maxOpenDepth` doesn't clear this, alongside the existing
  `patchMaxFails` exclusion. This subsumes the old `openRays == 0` retire
  condition (a patch with no open rays trivially has `maxOpenDepth == 0`,
  well under the threshold) in one check.
- [x] `patchMaxFails` (bumped from 3 to **6** default) is now explicitly
  documented as what it always actually was in the code -- **a permanent
  exclusion**, not a within-pass skip. `consecutiveFails` is cumulative
  across the whole run (only `RecordSuccess` resets it), so once a patch
  crosses `patchMaxFails` it is excluded from every future
  `RebuildOrder`, forever. This was previously *mis-documented* here as
  "skip for the rest of the pass, reset next pass" -- that description
  never matched the code. Under the new stopping criterion this permanence
  is exactly the right behavior: it is what makes "no eligible patches"
  mean "every deep pocket is filled **or confirmed unfillable**," not
  "temporarily out of patches to try this pass." The bump from 3 to 6 gives
  a genuinely fillable-but-tricky pocket more chances before this
  irreversible exclusion, since it now carries more weight than before.
- [ ] retire when every hit is class 3, `patchDoneFraction`,
  `berriesPlaced >= patchMaxBerries`: still not implemented, unchanged from
  before this revision, and lower priority now that eligibility is
  correctly depth-gated -- none of the three were the actual gap.

For the step, stop when:
- [x] **all patches exhausted** (`NextPatch` returns -1 after a rebuild) is
  now the sole primary stop, and given the eligibility fix above this
  means what the plain-words description says: every patch either has no
  ray past `deepPocketThreshold` (shallow, left alone on purpose) or has
  exhausted `patchMaxFails` (genuinely tried and failed, e.g. a mouth
  narrower than the smallest berry).
- [x] `targetOpenFraction` and diminishing-returns: **removed**, not just
  left unused -- `PocketStepConfig` no longer has the fields, and the main
  loop no longer calls `BuildCoverageReport` at all (it was only there to
  feed those two checks). Removing the per-iteration `CoverageReport`
  rebuild also resolves the "is this too slow at scale" performance
  question the previous version of this section flagged as open.
- [x] `maxSecondsPerStep` and `maxBerries`: kept, explicitly relabeled as
  **safety backstops, not targets** (`PocketStepResult::exitReason` now
  says so directly) -- they exist so a run can't hang or run away, not
  because reaching them means the container is done.

**Measured effect of the correction**, same 944-instance pack, wider budget
(`--items 500 --budget 260`) to actually see the difference: the *old*
diminishing-returns criterion stopped at **190** placed; the corrected
criterion, given more time, kept going to **977** placed before hitting the
time-budget backstop (not "all patches exhausted" -- there was still more
budget's worth of deep pocket to fill). Visible berry fraction climbed to
**0.830** (from 0.719-0.736 in the smaller runs), open ray fraction to
0.368, `--validate` still 0 failures, worst overlap 0.6 cm, mean overlap
0.310 cm. This wasn't run out to natural "all deep pockets filled or
unfillable" termination -- at the placement rate measured (roughly 15
placements/second including all rejected attempts), reaching that point for
the full container is a multi-minute-to-hour run, left for whoever runs the
real target-scale pass rather than simulated here.

- [ ] exit-reason logging in the style of `PackStep`: `PocketStepResult::exitReason`
  is populated and printed by `pocket_fill`, but there is no equivalent inside
  `PackPocketStep` itself (`PackStep` logs via `LOGI` as it runs; `PackPocketStep`
  has no logging at all, only the returned struct). Fine for a bench scenario
  that prints the result itself; would need adding if `PackPocketStep` is ever
  driven from `main.cpp` directly.

## H5: plumbing

- [x] `PackingStep` gets a fill mode (`FillMode::FillVolume` today vs
  `FillSurface`), set on `lastStep` in `PlanPackingSteps` (`PackingPlan.cpp`).
  `PackingStep::Load` reads fixed tokens in order, so the new field is read
  as an optional trailing token: after `useInnerContainer` it tries one more
  `>>`, and if that isn't the literal `fillMode` (or the stream is at eof --
  the true last step in a file has nothing after it), it seeks back and
  leaves `fillMode` at `FillVolume`. Verified against three cases (a plan
  saved before H5 with no trailing tokens at all, a two-step plan where only
  the second step has one, and a `Save`-then-`Load` round trip) in a scratch
  program linked against `libpack_core.a` -- there is no unit test target in
  this repo to leave it in, so it is not checked in, but the parse logic
  itself is the change and is small enough to read directly in
  `PackingPlan.cpp`.
  `PackingDriver.cpp`'s `PackScene` dispatches on `step.fillMode`: `PackStep`
  for `FillVolume` as before, a new `PackSurfaceStep` for `FillSurface` that
  builds a `PocketStepConfig` from the `PackingConfig` fields below and calls
  `PackPocketStep`. `PackSurfaceStep` saves once at the end of the step
  (`_surface` suffix on the pack/traj file names) rather than at
  `PackStep`'s placeItem-level `trajSaveInterval`/`packSaveInterval`
  granularity -- there is no per-placement hook into `PackPocketStep`'s loop
  for that, so a crash mid-step loses more progress than a `FillVolume` step
  would. Acceptable for now since `FillSurface` is only ever the last step.
- [x] `PackingConfig` keys, all dumped in `toString()` and documented in
  `pack_fruits.cfg`: `surfaceRaySpacing`, `patchSize`, `holeDepth`,
  `deepPocketThreshold`, `maxProbeDepth`, `patchMaxFails`,
  `pocketTrialCount`. `patchDoneFraction`/`patchMaxBerries` dropped from the
  original list here alongside `targetOpenFraction`/`minRaysPerBerry`: H4's
  revision made "every patch either shallow or `patchMaxFails`-exhausted"
  the sole stop signal, and neither of those two ever got implemented as a
  separate mechanism -- exposing config keys for fields that do not exist
  would be worse than not having them.
- [x] `useInnerContainer` on the final step: measured both ways on the same
  resumed pack, 20 s each. Off: 293 placed, visible berry fraction 0.773,
  invalid rays 78. On: 327 placed, visible berry fraction 0.766, invalid
  rays 893, and three of the open-ray depth histogram buckets collapsed to
  roughly a fifth of their off-case size. The inner shell reads as a real
  hit in the same class grid pocket targeting marches through, so turning
  it on does not just skip wasted trials near the center (pocket targeting
  never searched there anyway, bounded by `maxProbeDepth`) -- it hides real
  pockets behind a wall that will not exist in the printed/milled part,
  which is the opposite of H3's whole correction. Visible berry fraction
  was a wash, so there is no upside to offset that measurement damage.
  Left off (`lastStep.useInnerContainer = false` in `PlanPackingSteps`).
  **Superseded by H7**, not reversed: this measurement is specific to the
  ray mechanism (the inner shell fooling a ray into reporting a hit), and
  the user confirmed the inner container's actual purpose is deliberately
  excluding the deep interior -- only near-surface pockets are wanted at
  all. H7's volumetric scan uses the inner container boundary directly
  (bounding where the free-space search looks), not as `classGrid` material
  a ray can be fooled by, so it comes back as infrastructure once H7 lands,
  just not in the form measured here.
  `classGrid` value 4 (`VOX_INNER`) stays defined either way, since a saved
  plan or a hand-edited one can still turn it on for a given step.
- [x] bench scenario `pocket_fill` in `BenchPocket.cpp` / `BenchRegistry.cpp`
  (done during H2, ahead of this phase): resumes a pack file, runs the
  pocket step under a wall budget, reports berries placed, open ray fraction
  before and after, visible berry fraction, rays closed per berry, rejected
  settles. Writes `pocket_fill_pack.txt` and `pocket_fill_open_mouths.obj`
  so the result can be rendered. Per-kind histogram (`kindUseCount_` already
  exists inside `PocketPlanner`, just not surfaced) and a `bpy_scripts`
  loader for it are still not done -- lower priority than the plumbing
  above since the pack file it writes already loads with the existing
  `bpy_scripts/load_trajectories_inst.py`.
- [x] `--validate` still passes. Re-ran `pocket_fill --items 30 --validate`
  after all of the above: 600 placed, 0 failures, worst overlap 0.6 cm,
  worst outside 0.6 cm, mean overlap 0.311 cm -- matches the H3/H4
  measurements, i.e. the config/dispatch plumbing changed nothing about the
  placement algorithm itself.

## H6: what the real hand pack exposed that no bench number caught

Running the H0-H5 result on the actual hand pack (not a bench sample) showed
gaps everywhere and berries visibly hugging the container wall -- the opposite
of H3's own goal. Two real bugs, found via a small, fast, visual dev
container (below), neither one visible in `visibleBerryFraction` because
that metric rewards exactly the broken behavior (a berry near a ray's mouth,
which is right at the surface).

- [x] **Targeting was still mouth-anchored even after H3.** `targetPoint`
  (the search box center and the "did the settle stay on target" check) was
  `ray.mouth` -- a fixed point ~1 cm inside the surface, regardless of how
  deep the ray's open run measured. The search box and accept window were
  both sized to the item's own extent around that point, so every placement
  was confined to within ~2 cm of the container skin no matter what
  `patch.maxOpenDepth` said. This is the literal mechanism behind "pushed
  toward the boundary": nothing else was possible. Fixed in `PocketDriver.cpp`
  to aim near the far end of the ray's open run (backed off by half the
  item's extent so it does not sit inside whatever the ray hit), falling
  back to the mouth only for a pocket too shallow to need this.
- [x] **The accept-window checks were measured from the wrong reference,
  making the fix above nearly a no-op on its own.** "Left patch" compared
  the settle to `patch.center` (a surface point) with a flat radius; "still
  open" compared it to the (now deep) aim point with a flat radius. Both
  scale wrong once the aim point legitimately sits deep in a pocket: full
  3D distance from a surface point grows with depth by design, so a
  correctly-deep placement could fail either check for the depth it was
  aimed at, not for actually drifting off target. Fixed by decomposing
  drift relative to the ray's own line: lateral offset (perpendicular to
  the ray) stays tightly bounded ("left patch"), while along-ray position
  is allowed the ray's whole open run, mouth to hit point, plus slack
  ("still open"). Measured effect on the dev container below: still-open
  rejects dropped from 427 of 782 attempts to 6 of 832, and placements on
  the same seeded container went 65 -> 429.
- [x] **The H3 hard reject (zero real-item contact) blocks bootstrapping
  from an empty or sparse container.** The very first berry placed anywhere
  has nothing to contact by definition; the strict rule rejected it forever,
  so a container that starts with too little nearby material could place
  nothing at all no matter how many attempts it got (measured: 1 placed in
  1399 attempts on a fully empty dev container). Fixed with a narrow
  exception in `PocketDriver.cpp`: container-only contact is allowed exactly
  once, only while `scene.instances` is completely empty -- the same
  constraint a real physical build has (the first piece touches the
  mold/floor, everything after nests against what already exists). Never
  fires in the real hand pack, which always resumes a container already
  full of bigger fruit; `PocketStepResult::bootstrapPlacements` reports it
  when it does.
- [x] A patch-level frontier bonus (prefer patches next to already-covered
  rays over fully isolated ones, `PocketPlanner::HasRimRay`) was tried to
  help the empty-container case specifically. Measured no effect at all
  (bit-identical attempt counts before/after) once material is genuinely
  isolated: a lone ~1 cm berry surrounded by open space is missed by
  neighboring rays sampled half a cm apart, so nothing ever reads as a
  covered neighbor for the rim check to notice. Left in (harmless, and
  correct once a real frontier exists), but the empty-container path is
  really carried by the bootstrap exception above, not this.
- [x] **New metric: gaping holes, not open fraction or visible berry
  fraction.** `CoverageReport::gapingHolePatches`/`gapingHoleFraction`
  (`BuildCoverageReport`'s new `gapingHoleDepth` parameter, default 1 cm,
  one blueberry across): count of patches whose deepest open ray still
  exceeds that depth. This is the metric that matches the actual complaint
  ("no gaping holes bigger than a blueberry") -- `openFraction` also flags
  ordinary sub-berry surface texture, and `visibleBerryFraction` rewards
  proximity to a ray's mouth regardless of whether the pocket behind it got
  filled.
- [x] **New dev/validation container**: `pack_melone_test.cfg` (stage 1,
  seeds `melone_two.obj` -- a single ~14 cm melon mesh, reused as a
  container -- with a handful of plum1/fig2/strawberry2 via the normal
  outward FFT search) and `pack_melone_test_stage2.cfg` (stage 2, resumes
  stage 1's pack and runs only the `fillSurface` step on 4 blueberry kinds).
  Fast enough to iterate on in seconds (dx 0.3, ~14 cm container) instead of
  minutes, small enough to actually look at. A totally empty container
  (no seed fruit at all) was tried first and is deliberately *not* the
  configuration kept here -- it is an edge case the real hand pack never
  hits (that step always resumes a container already full of bigger fruit)
  and it is what surfaced the bootstrap bug above, not a realistic
  validation target on its own.
- [ ] Not yet re-measured on the real hand pack (`pocket_fill` on
  `pack_944_0721.txt`) with these fixes -- the dev-container numbers above
  are the only ones that exist so far. `deepPocketThreshold`/`patchMaxFails`/
  `pocketTrialCount` were also retuned for the dev container's much smaller
  scale (1.5/20/8 vs production's 3.0/6/3) and have not been re-validated
  at production scale either.
- [x] **Visual inspection of the dev container's rendered mesh disproved the
  "mostly shallow terrain" guess above, and found the real reason the
  residual tail exists.** Every one of 496 placed blueberries has zero
  contact with a big fruit -- only with other blueberries (measured: 0 of
  496 have a big-fruit neighbor within 1.6 cm; mean 9.1 blueberry-blueberry
  neighbors). Radially, blueberries sit at a mean distance of 6.0 cm from
  the melon's center (69% of them at 6-7 cm) while the big fruit occupy
  1.8-5.7 cm -- the blueberries formed their own separate crust in the ring
  between the fruit and the container wall, never entering the real gaps
  between individual fruit pieces. That is the same "hugging the wall, not
  the holes" complaint H3 was originally meant to fix, back with a
  different mechanism.
- [x] **Root cause: a surface ray stops at the first thing it hits, so
  targeting can never see past the outermost layer of material.** H6's
  "aim deep" fix only moves the aim point deeper *within the ray's first
  open run* -- from the container wall to whichever fruit the ray hits
  first. It cannot reach a gap sitting behind that fruit, between it and
  its neighbors, because the ray's march stops there by construction. This
  is not a tuning problem; it is a structural limit of ray-marching from
  the container surface, and it fully explains both the original H3
  symptom and this one: whatever material forms the outermost layer
  (container-adjacent big fruit, in this case) is the only layer this
  system can ever address.

## H7: volumetric pocket detection, scoped to near-surface only

H6 found that ray-marching from the container surface cannot reach a pocket
sitting behind the outermost layer of material -- the ray's march stops at
the first hit, so nothing past it is ever measured. That is not a bug to
tune away; it is what the whole `SurfaceCoverage`/`PocketPlanner` mechanism
*is*, so H2-H4's ray/patch machinery needs replacing for the final step, not
another accept-window fix.

**Scope correction, from the user directly: only near-surface pockets
matter.** Pockets buried deep inside the mesh, with no path back out, are
explicitly out of scope -- that is the entire reason the full pipeline has
an inner container (`useInnerContainer`, `AddInnerContainer` in
`PackingScene.cpp`) at all: it exists to wall off the deep interior so
nothing ever gets aimed there. This reframes what "volumetric" needs to
mean here: not a flood-fill over the whole free interior (which would waste
effort on, and try to fill, the very core the inner container is there to
exclude), but one bounded to the *shell* between the outer container surface
and the inner container boundary.

This also means H5's call to leave `useInnerContainer` off for the final
step needs revisiting, not just its ray-metric justification.
`useInnerContainer`'s measured harm in H5 (invalid rays 78 -> 893, several
open-ray depth buckets collapsing) was specific to the *ray* mechanism: the
inner shell reads as a material hit in the same class grid rays march
through, so it looks like it is hiding real pockets. A volumetric free-space
scan does not have that failure mode -- it can use the inner container as a
literal search boundary (do not label or target anything past it) rather
than as something a ray can "hit" and be fooled by. So the inner container
comes back, just as infrastructure for the new mechanism instead of as
`classGrid` material for the old one.

- [x] **Free-space connected components, bounded to the shell.**
  `PocketVolume.h/.cpp`'s `BuildPocketVolumeField`: flood-fills `VOX_FREE`
  (6-connected). No separate shell-clipping logic needed -- `AddInnerContainer`
  already stamps the deep interior's free voxels `VOX_INNER`, which reads as
  a wall to this scan exactly like `VOX_CONTAINER`/items do, so calling it
  after `AddInnerContainer` bounds the search to the shell for free.
- [x] **A target point per component.** Same file: a multi-source BFS from
  every voxel with a non-free 6-neighbor (grid edges count as non-free)
  gives every free voxel an unweighted "voxel-steps from the nearest wall"
  value, computed once, then folded into the same flood-fill pass as each
  component's own running max (`FreeComponent::targetDist`/`targetVoxel`).
  Approximate Manhattan distance, not exact Euclidean, as instructed.
- [x] **Reuse H3's accept/reject and settle machinery, adapted.**
  `VolumeDriver.h/.cpp`'s `PackVolumeStep`: real-item-contact requirement
  and the bootstrap exception carried over unchanged. The lateral/along-ray
  decomposition could not carry over as-is (no ray line to decompose
  against) -- replaced with one isotropic check, settled position within
  `max(componentDepth, itemExtent) + itemExtent + slack` of the target
  point. `PocketPushDirection` became `VolumePushDirection`: same "push
  toward the nearest real item, never the container" idea, found by a local
  box scan of `classGrid` around the target point instead of a ray's
  neighbor list.
- [x] **Stopping criterion, now literal instead of a proxy.** No component
  with `targetDist * dx >= minTargetDepth` left un-exhausted. Measured
  directly on the melon dev container (below), not inferred from a ray
  metric.
- [x] **Bug found and fixed by the user's own eyes, not by a metric: a
  single target per component silently gives up on almost the entire
  container.** First pass tracked exactly one point per component (its
  global local max) with a per-component fail counter; the melon's free
  space starts out as essentially one giant component (below), so once
  *that one specific point* failed `componentMaxFails` (6) times in a row
  -- wrong kind for that exact spot, search box missed, whatever -- the
  *entire* remaining free volume got excluded until the next successful
  placement anywhere, which for a component that size may never come
  again. Result: only 17 blueberries placed and a rendered mesh with
  visibly unfilled cavities, despite ~520 cm^3 of measured free space
  left. Caught only because the user looked at the actual mesh and said
  so ("none of the cavities are filled") -- every number the step itself
  reported (0 rejected for no contact, exit reason "no free pocket left")
  looked like success. Fixed by collecting several separated local maxima
  per component instead of one (`FreeComponent::peaks`,
  `PocketVolumeParams::maxPeaksPerComponent`/`peakSeparationVoxels`,
  `TryAddPeak` in `PocketVolume.cpp`) and flattening every peak from every
  component into one deepest-first candidate list in the driver -- a bad
  spot now costs one entry in that list, not the whole component. 17 ->
  71 placed on the identical melon scene as the direct result.
- [x] Cost measured, on the melon dev container only (grid 56x48x56 =
  ~150k voxels, `after inner: free 19272`). One full rebuild (BFS +
  flood-fill) after every successful placement -- not attempted, per
  attempt, to avoid retry-storm cost -- and 71 blueberries placed in 1.95s
  total, ~27.5 ms/placement, dominated by `Nudge`'s own gather step
  (~15-55ms each, matching H0/H1's existing per-Nudge numbers), not by the
  rebuild. **Not yet measured on the real hand pack** (~22M voxels, per
  H0) -- the full-rebuild-per-success approach may need to become
  incremental (recompute only the affected component's own region, the
  same locality `ReprobePatch` already uses for rays) if it turns out too
  slow there; left as the simplest correct thing until proven otherwise,
  per plan.
- [x] First measurement on the melon dev container (`scratch/melone_volume.cpp`,
  seeded with stage 1's 17 big-fruit instances, `pack8.txt`, corrected kind
  names -- `blueberry1`/`blueberry1_001`/`blueberry2`/`blueberry2_001`, see
  the `GetItemIndex` finding below): 71 blueberries placed in 108 attempts,
  0 rejected for no item contact, 5 rejected too far. Distance from each
  placed blueberry to the nearest container-wall voxel: min 0.3 cm, median
  1.2 cm, mean 1.19 cm, max 3.9 cm, 30/71 (42%) within 1 cm of the wall
  (down from effectively all of them before H7) -- refuted H6's "blanket
  hugging the boundary" finding. The user's read of the actual render at
  this point: "there's a sense of an attempt at filling the cavities. but
  there're a ton of unfilled cavities. even filled cavities still have
  lots of openings" -- correct, and led to the second bug below.
- [x] **Second bug, also found by looking at the render, not a metric: one
  kind picked per spot, no fallback when it doesn't physically fit.**
  `RankKindsForDepth`'s predecessor picked exactly one kind per target (the
  largest that fit the depth estimate) and gave up on the *entire spot* if
  that one kind's `FindSpotLocal` search failed, never trying a smaller
  kind at the same, otherwise-good, location. Debug logging (temporary,
  since removed) showed every single `!spotFound` failure on the melon
  test was the single largest eligible kind (`blueberry2`), at spots with
  clearly enough measured depth (peaks at 3-4 voxels, 0.9-1.2 cm) for a
  smaller kind to fit fine. Fixed by ranking every kind largest-fits-first
  (still share-cap aware) and cascading down the ranked list at the same
  spot until one physically fits, only moving on once all of them fail.
  71 -> 251 blueberries placed on the same scene, purely from this one
  change. Open ray fraction 0.095 -> 0.017, `gapingHoleFraction` (the
  ray-based patch metric) 0.75 -> 0.55 -- the first real movement in that
  number since H6, not just noise, though it is still not the metric this
  mechanism should be judged by (H6 already proved it cannot see past the
  first hit). Wall-distance: median dropped to 0.9 cm (expected -- deeper
  pockets get consumed first and the remaining ones are shallower), 138/251
  (55%) within 1 cm of the wall, mean unchanged at 1.18 cm. This is the
  current, final state of the melon test as of this writing; the mesh
  (`melone_volume_render.obj`) reflects it.
- [x] One structural note worth remembering: with only ~200 big fruit in a
  container this size, the free space between them starts out as
  essentially *one* connected component (19248 of 19272 free voxels here)
  with up to `maxPeaksPerComponent` (24) separated candidate points, not
  one -- "connected components" mostly does not mean many small isolated
  pockets, it means one big blob whose candidate points migrate and shrink
  as filling proceeds, which is why the per-success full rebuild (not a
  one-time build) is load-bearing, not a formality.
- [ ] Both bugs above were caught by the user looking at the actual
  rendered mesh, not by any number the step itself reported -- every exit
  reason and reject counter looked clean both times. `gapingHoleFraction`
  moved the second time but not the first, so it is not yet a reliable
  enough proxy on its own either. Worth treating "render and look at it"
  as a required step before trusting a number here, not an optional
  sanity check -- not yet resolved into an automated check, if one is even
  possible for this specific failure mode (both bugs were about *coverage*
  of the search, which a global fraction can hide either side of).
- [x] Wired in as an alternative to the ray path, not a replacement yet.
  `SurfaceCoverage`/`PocketPlanner`/`PocketDriver`/`PackSurfaceStep` are
  untouched; `PackVolumeStep` is a new, separate entry point exercised only
  by the melon scratch harness so far. Swapping `PackSurfaceStep` to call it
  for the real hand pack is the next step, after the real-hand-pack cost
  question above is settled.
- [x] Incidental finding, not part of H7 itself: `PackingScene::GetItemIndex`
  returns index 0 (not a sentinel) for a name not in `nameToIndex`, and both
  `PackPocketStep` and `PackVolumeStep`'s `itemIndex >= scene.items.size()`
  guard cannot catch that, since 0 is always a valid index. A typo'd or
  stale kind name in any step's `names` list silently substitutes item 0's
  mesh instead of skipping or failing loudly -- caught here only because the
  melon test's hand-typed kind list guessed wrong names ("blueberry3"/
  "blueberry4", which do not exist; the real files are "blueberry1_001"/
  "blueberry2_001") and produced a visibly skewed kind mix. Flagged as a
  separate follow-up, not fixed here -- it is pre-existing and shared by
  both drivers, not introduced by H7.

## H8: shell-local higher-resolution grid for pocket detection

Not started -- design only, written down before implementing per the
user's instruction, while the user tries a different approach in parallel.
Do not confuse this section with what `PocketVolume`/`VolumeDriver`
actually do today (H7): this is the next step, not yet built.

**What H7's own debug dump found.** After H7's two real bugs were fixed
(single-target-per-component, single-kind-per-spot), the user asked to dump
`scene.classGrid` directly via `meshutil.h`'s `SaveVolAsObjMesh` (one obj
per `VoxClass` value, `scratch/melone_volume.cpp`) instead of trusting the
smoothed item render. Looking at the raw voxel boundary of `VOX_FREE`
alongside the placed-item voxels, the user's read: "it's mostly limitations
of voxel resolution." At `dx = 0.3` cm and a blueberry radius of ~0.5-0.7 cm
(1.7-2.3 voxels), the class grid simply cannot resolve gaps that size well:
a 0.4 cm gap between two round fruit surfaces rounds to 0 or 1 voxels, and
the unweighted 6-connected BFS distance H7 uses (a deliberate approximation
of Manhattan distance, per the user's own earlier instruction) is
axis-aligned, so it systematically underestimates clearance across a
diagonal gap even worse than a straight one. This is a resolution problem,
not a third instance of H7's two logic bugs -- no amount of peak-list or
kind-cascade tuning fixes a gap that rounds away entirely at this dx.

**Decision, from the user directly: refine locally, do not go back to
rays.** The alternative on the table was reverting to ray/pixel
(2D-surface) based metrics, which is cheaper in memory (O(surface area) vs
O(volume)) but reintroduces exactly what H6 already proved broken --
blind past the first hit -- unless reworked into a multi-hit march, which
is volumetric reasoning wearing a 1D disguise anyway. The chosen direction
keeps H7's volumetric architecture (it is what correctly solves H6's blind
spot) and fixes the newly-found resolution problem where it is cheap to
fix: a second, finer-`dx` class grid used only by pocket detection,
restricted to the region that actually needs it, not the whole container.
Memory is not actually the tight constraint here -- classGrid is 1
byte/voxel, so even a uniform 2x refine of the real hand pack's ~22M-voxel
grid (H0) is only ~176 MB -- the tight constraint is that
`BuildPocketVolumeField`'s full BFS + flood-fill already runs once per
successful placement (H7's unmeasured-cost caveat), and that cost scales
with voxel count, so refining the *whole* grid multiplies an already-
unmeasured cost by 8x (2x per axis) for no benefit outside the shell.
Restricting the fine grid to the free region's own bounding box keeps that
multiplier local to a small fraction of the container, matching this
codebase's existing precedent of different subsystems already running at
different resolutions on purpose (`dx` for collision, `gridDx` for the
small-item subgrid, `containerSDFDx`/`broadPhaseDx` coarser still) rather
than introducing a new kind of tradeoff.

- [ ] **Scope the fine grid to the free region's own bounding box, not the
  shell by area.** H7 already established the free space between big fruit
  starts out as essentially one connected component (melon test: 19248 of
  19272 free voxels). Take that component's AABB (plus a small pad, e.g.
  one fine-grid item radius) from the existing coarse
  `BuildPocketVolumeField` result as the region to re-voxelize finely --
  no separate shell-boundary computation needed beyond what H7 already
  has, since `VOX_INNER` already keeps the deep interior out of the coarse
  free component in the first place.
- [ ] **Re-voxelize that one AABB at a finer dx (start at 2x, `dx/2`,
  measure before going finer).** Needs a bounded, local voxelization path
  that does not exist yet: today's classification is whole-container
  (`PrepareBackground` in `PackingDriver.cpp`: `MeshConvo::Voxelize` +
  `InvertContainer` + `ClassFromBinary` over the entire padded grid).
  Reuse the *pieces* at the smaller scope instead: rasterize the container
  and inner-container meshes into just the AABB at the finer dx (same
  triangle-rasterization `MeshConvo::Voxelize` already does, just called
  against a cropped/offset local grid), then stamp every already-placed
  item overlapping the AABB into it the same way `VoxFootprint`/
  `StampClass` already rasterize one item's footprint for collision, just
  at the finer dx and against this local grid instead of the global one.
- [ ] **Run H7's existing flood-fill/BFS/peak collection unchanged against
  the fine local grid.** `PocketVolume.cpp`'s `BuildPocketVolumeField`
  already takes a `classGrid`/`gridOrigin`/`dx` triple with no assumption
  baked in that they are the scene's own -- it should work as-is against
  the local fine grid and its own local origin. Target points come out in
  world coordinates already (`FreeComponent::Peak::point`), so
  `VolumeDriver.cpp` downstream (search box, `Nudge`, accept/reject) needs
  no changes -- it already treats a target point as an opaque world-space
  coordinate, never as a voxel index into any particular grid.
- [ ] **Collision stays at today's resolution -- the fine grid is
  advisory, not authoritative.** `FindSpotLocal`/`Nudge`/`Put` keep using
  `scene.bg`/`scene.classGrid` at `scene.dx` exactly as today. If the fine
  grid points at a spot the coarse collision search still cannot validate,
  that is not a new failure mode -- H7's existing `spotFound`/reject-too-far
  handling already covers "the target didn't pan out," and this keeps the
  blast radius of H8 confined to targeting quality, not collision
  correctness.
- [ ] **Incremental update, not a full local rebuild per placement.**
  Occupancy in the fine grid only needs one new item's footprint stamped
  in per placement (cheap, bounded by that one item's footprint size);
  only the derived flood-fill/BFS needs recomputing after that, and only
  over the fine AABB, not the whole scene. Whether that still needs to be
  a full re-run of the fine AABB's BFS/flood-fill each time, or can reuse
  more of H3/H7's existing locality patterns (`ReprobePatch`'s per-patch
  reprobe is the nearest precedent), is unmeasured -- start with the
  simplest correct thing (full local rebuild) and measure, per this
  session's own pattern, before assuming it needs to be smarter.
- [ ] Cost and correctness both unmeasured. Not yet validated that a 2x
  refine actually resolves the gaps the melon test's `SaveVolAsObjMesh`
  dump showed as the limitation -- re-run the same dump against the fine
  grid's own free/occupied classification once this exists, the same way
  H7's two bugs were caught, before trusting any placement-count number it
  produces.
- [ ] Not started. The user is testing a different approach in the
  meantime; do not implement this without checking in first, since it may
  turn out unnecessary depending on what that produces.

## Files: what to add, what to change, what not to touch

`CMakeLists.txt:5` globs `*.cpp` with `CONFIGURE_DEPENDS` into `pack_core` and
removes only `main.cpp` and `benchmarks.cpp`, so **no build file edits are
needed** for any new source below.

### New files

| file | contents | depends on |
| --- | --- | --- |
| `VoxClass.h` | the class enum (`VOX_FREE`, `VOX_CONTAINER`, `VOX_ITEM_BIG`, `VOX_ITEM_SMALL`, `VOX_INNER`) and `VoxClassOfExtent(maxExtent, smallThresh)`, the single definition of "small" shared with `PlanPackingSteps` | nothing |
| `SurfaceCoverage.h/.cpp` | **landed for H1 and H2**: `CoverageParams`, `SurfaceRay`, `CoveragePatch`, `CoverageField`, `BuildCoverageField` (ray set construction, per-ray normal sign check, DDA march, ray adjacency graph, rim/interior classification), `AssignPatches`, `CoverageReport`/`BuildCoverageReport`/`toString`, `SaveOpenMouthsObj`, `ReprobePatch` + `FindNeighborPatches`, `BuildOrthoCoverageReport`, `ComputeVisibleBerryFraction`, and (H2) `CountRaysBlocked` -- the POCKET score's ray-count query, placed here rather than in `PackingOpsLocal.cpp` since it is a geometry query over rays + a footprint, which fits this file's charter and keeps `PackingOps.h` free of a dependency on it. This makes `SurfaceCoverage.h` depend on `VoxFootprint.h`, not listed in the original plan | `Array3D8u`, `TrigMesh`, `PointSample`, `VoxClass`, `VoxFootprint` |
| `PocketPlanner.h/.cpp` | **landed for H2, partial H4**: `PocketPatchState` (fails, berries placed -- no `retired`/reject counters, see H4), priority tiers and the strided traversal (`RebuildOrder`/`CoprimeStride`), `NextPatch`, `PickTargetRay` (rim first), `PickKind` (size matched with the share cap). **Not landed**: per-patch retirement as a persistent state, the step-level stop test (that lives in `PocketDriver.cpp` instead, not here) | `SurfaceCoverage` only |
| `VoxFootprint.h/.cpp` | **landed**: voxelization of one kind at one rotation, cached by `(kindId, rotIdx)` at a given `dx`, with `MemoryBytes()`. `rotIdx` is a caller-assigned stable index (into `scene.randAngles`) rather than the rotation value itself, so cache keys are exact integers, not float-quantized rotations | `TrigMesh`, `cpu_voxelizer`, `Array3D8u` |
| `PackingOpsLocal.cpp` | **landed**: `FindSpotLocal`, candidate offset enumeration on the `dx` lattice and the direct overlap test with early-out. The POCKET score itself is not here (see `SurfaceCoverage.h` above) -- this file stays generic, scoring only through the opaque `SpotScore::scorer` callback its caller supplies | `PackingOps.h`, `VoxFootprint` |
| `PocketDriver.h/.cpp` | **landed**: `PackPocketStep(scene, step, cfg)`, the H2 loop, the H3 push blend and accept/reject test, the H4 backstop-based stop test. **Not landed**: per-placement/heartbeat logging (only the final `PocketStepResult` is returned; nothing logs during the loop) | `PackingScene`, `PocketPlanner`, `SurfaceCoverage`, `VoxFootprint`. Does **not** depend on `PackingDriver.h` -- the "shared commit" reuse noted below did not happen |
| `BenchPocket.cpp` | **landed**: `coverage_baseline` (H1) and `pocket_fill` (H2/H3), the latter running `PackPocketStep` over every small kind in the catalogue and reporting the coverage delta plus reject-reason counts | `Benchmark.h`, the above |
| `bpy_scripts/load_pockets.py` | import the open-ray obj and the pack, so pockets and berries can be looked at together. Optional, and only worth it if the obj alone reads badly in Blender | -- |

Two boundaries worth keeping strict, because they are what make the measurement
trustworthy and the bench cheap:
- `SurfaceCoverage` must **not** include `PackingScene.h`. It takes a class
  grid, an origin, a `dx`, a container mesh and params. That lets
  `coverage_baseline` build a ray field over a loaded pack with no driver
  involved, and lets the ray march be tested against a hand-built grid.
- `SurfaceCoverage` holds no policy and `PocketPlanner` holds no geometry
  queries. H1 has to be readable and correct on its own before H2 is allowed to
  depend on it.

### Changed files

| file | change |
| --- | --- |
| `PackingScene.h/.cpp` | **landed (H0)**: `Array3D8u classGrid`; stamp it in `Put` next to the existing `Union`; write `VOX_INNER` in `AddInnerContainer`. **landed (H3, revised)**: `NudgeResult` struct and `Nudge`'s defaulted `NudgeResult *out = nullptr` parameter, reporting final contact count and how many contacts were with items -- filled from `Contact::meshIndex` vs. that step's `intersectingInstances.size()`. All 5 existing callers (`PackingDriver.cpp:186`, `DebugTools.cpp:62`, `BenchGrowth.cpp:83`, `BenchSmallFruit.cpp:90`, `BenchNudge.cpp:57`) confirmed unaffected by the added defaulted parameter. **Not landed**: the `PackingConstraints` plane constraint for a two-stage settle, since the two-stage escalation itself is no longer clearly motivated post-correction (see H3) |
| `GridUtils.h/.cpp` | **landed**: `StampClass(dst, offset, mask, cls, overwriteMask)` mirroring `Union` in `MeshConvo.cpp:92`, and `ClassFromBinary(vox, cls, classVal)` (took a `classVal` param, not in the original signature, so the same function seeds both `VOX_CONTAINER` and could seed any other class from a binary mask) to seed the container class in one pass next to `InvertContainer` (`:128`). Generic array ops stay here; the semantics stay in `VoxClass.h` |
| `PackingOps.h/.cpp` | **landed**: `SpotScore` struct (mode `DEFAULT`/`POCKET`, an opaque `scorer` callback for `POCKET` rather than a `SurfaceCoverage`-typed field, to avoid the header depending on it) threaded through `FindSpot`'s score loop via a new `ScoreCandidate` helper, with the old 6-arg entry point kept as a thin overload so the corner pull stays hardcoded for every other step; `FindSpotSubgrid` split into `FindSpotBox(bg, part, pos, rot, score, searchBox, shrink, outCenter)` plus a thin cell-computing wrapper; the 0.5 cm shrink is `FindSpotBox`'s `shrink` parameter; `FindSpotLocal` declared here, defined in `PackingOpsLocal.cpp` |
| `PackingDriver.h/.cpp` | **landed (H0)**: fill the container class in `PrepareBackground`. **landed (H5)**: `PackSurfaceStep` (builds a `PocketStepConfig` from `PackingConfig`, calls `PackPocketStep`, one save at the end of the step) and `PackScene`'s dispatch on `step.fillMode`. `PocketDriver.cpp` itself stays independent of this file -- the dispatch is the one place they meet |
| `PackingPlan.h/.cpp` | **landed (H5)**: `FillMode` enum, `PackingStep::fillMode` (optional trailing token on `Load`, always written by `toString`), `lastStep.fillMode = FillSurface` and `lastStep.useInnerContainer = false` (measured, see H5 above) in `PlanPackingSteps` |
| `PackingConfig.h/.cpp` | **landed (H5)**: `surfaceRaySpacing`, `patchSize`, `holeDepth`, `deepPocketThreshold`, `maxProbeDepth`, `patchMaxFails`, `pocketTrialCount`, read in `LoadFromFile` and written in `toString` |
| `pack_fruits.cfg` | **landed (H5)**: documented the new keys and the step 4 comment block |
| `MemStats.h/.cpp` | **landed (H0)**: row for `classGrid`, confirmed at 21.66 MB in bench output. **Not landed (H2)**: no `VoxFootprint` cache row (it showed up unlabeled inside `kindGrids`' memory table entry in the one `pocket_fill` run measured -- 51.11 MB across 91 entries -- since nothing distinguishes the two caches yet) |
| `BenchRegistry.cpp` | **landed**: registered both `coverage_baseline` and `pocket_fill` |
| `BenchReport.h/.cpp` | **not landed**: unchanged, see the H1 update above for why (nothing structured needed adding yet; `pocket_fill`'s reject-reason counts also went through `BenchRegion::Note()`, not new typed fields) |
| `benchmarks.cpp` | **not landed**: no sweep flags |
| `DebugTools.h/.cpp` | **landed (H1 only)**: `DebugCoverage(cfg)`. No pocket-specific debug helper was added; `pocket_fill`'s own dumps (`pocket_fill_open_mouths.obj`, `pocket_fill_pack.txt`) serve that role for now |
| `Profiler` usage | **landed**: `coverage.build`, `coverage.march`, `coverage.reprobe`, `findspot_local.total`, `findspot_box.total`/`findspot_box.crop` (renamed from `findspot_subgrid.*`, which now wraps them -- see H2's `FindSpotBox` note), `pocket.total`, tally counter `count.rays_open`. **Not landed**: `pocket.select`, `count.candidates`, `count.settle_reject` -- no per-candidate or per-reject-reason profiler tallies, only the `PocketStepResult` struct fields |

### Not touched

`TrigGrid`, `BroadPhase`, `MeshConvo`, `cpu_voxelizer`, `FloodOutside`,
`RigidBody`, the PGS solver inside `Nudge`, `PackValidate`, `PackBones`,
`main.cpp`, and `CMakeLists.txt`. The narrow phase and the settle solver are
`improvement_plan.md`'s territory and are already at their measured knee; this
plan changes what the packer aims at, not how it resolves contacts. The one
exception is the `PackingConstraints` field above, which adds a case to an
existing switch rather than touching the solver.

### What lands per phase, so each phase builds and runs alone

| phase | new | changed |
| --- | --- | --- |
| H0 (done) | `VoxClass.h` | `PackingScene`, `GridUtils`, `PackingDriver` (`PrepareBackground`), `MemStats` |
| H1 (done) | `SurfaceCoverage`, `BenchPocket.cpp` (`coverage_baseline` only) | `BenchRegistry`, `DebugTools`. Not touched: `BenchReport`, `benchmarks.cpp` (see the changed-files table above) |
| H2 (done, minus kind-histogram reporting) | `PocketPlanner`, `VoxFootprint`, `PackingOpsLocal.cpp`, `PocketDriver`, `BenchPocket.cpp` (`pocket_fill`) | `PackingOps`, `SurfaceCoverage` (adjacency graph + `CountRaysBlocked`), `BenchRegistry`. Not touched at the time: `PackingDriver` -- no shared commit, no dispatch (landed in H5) |
| H3 (partial, revised) | -- | `PocketDriver` (push toward real material, hard reject on zero item contact, accept/reject on inward-sink/left-patch/still-open-approx), `PackingScene` (`NudgeResult`, landed). Not touched: the plane constraint for a two-stage settle, no longer clearly motivated post-correction |
| H4 (revised, mostly done) | -- | `PocketPlanner` (`deepPocketThreshold`-gated eligibility, `patchMaxFails` as the permanent-exclusion mechanism it always was, now correctly documented), `PocketDriver` (single "all patches exhausted" stop plus safety-only backstops; `targetOpenFraction`/diminishing-returns removed). Not landed: `patchDoneFraction`, `patchMaxBerries`, per-patch `retired` field as its own tracked state (subsumed by the eligibility filter instead) |
| H5 (done, minus optional Blender loader) | -- | `PackingPlan` (`FillMode`), `PackingConfig` (new keys), `pack_fruits.cfg` (documented), `PackingDriver` (`PackSurfaceStep`, dispatch). Not landed: `bpy_scripts/load_pockets.py` -- optional, and `pocket_fill`'s own pack file already loads with the existing trajectory loader |

H0 and H1 are additive: nothing reads the class grid or the coverage field until
H2, so both can land and be verified against the current packer's output without
changing a single placement. That is the property that makes the baseline
measurement believable.

## Risks and open questions

- **The metric is the objective.** Optimizing open ray fraction can be gamed by
  a berry that clips one ray each. The independent ortho ray set and the
  renders in H1 are the guard; treat a large gap between the two ray sets as a
  bug in the heuristic, not noise.
- **Pocket mouths may be narrower than the smallest berry** in the finger
  regions, where the container is a few cm across. Those patches will retire on
  `patchMaxFails` and stay open. Worth reporting separately -- an open fraction
  that no berry can close is a mesh or size-catalogue problem, not a packer
  problem.
- **`holeDepth` and `patchSize` interact.** Too large a patch retires while
  visible holes remain inside it; too small and the round robin thrashes. Sweep
  both once the baseline exists rather than guessing, the way `gridDx` was
  swept in 5.3.
- **Rays from the container surface do not see every deep pocket the same
  way a mill tool head would.** They are normal rays from a closed surface,
  so a deep crevice between two fingers is sampled from both walls but not
  along whatever path a real tool head would actually take to reach it (or
  fail to). Under the fabrication reframing (Goal section) this risk changed
  shape but didn't go away: the independent ortho set is still the guard for
  "is the primary ray set being gamed by its own sampling," but neither ray
  set claims to model millability/printability directly, only "distance to
  the nearest obstruction from a surface-normal direction" as a proxy for it.
  If a render (or a real print/mill attempt) disagrees with the metric,
  that's the signal to revisit the proxy, not just to tune parameters.
- **Berries with no item contact were an unresolved tension under the old
  aesthetic framing; under the corrected one they are simply rejected.**
  Landed in H3's revision: `nudgeResult.itemContactCount == 0` is a hard
  reject now (`PocketStepResult::rejectedNoContact`), not a warning, because
  the container the berry might otherwise be resting on is not part of the
  final printed/milled object. Measured rate on the one `pocket_fill` run so
  far: 12 of 426 attempts (2.8%) -- low enough that this risk did not need
  the "bias `wt` up" mitigation the old plan anticipated (that mitigation no
  longer applies either, since `wt`/`wn` were removed along with the
  outward-push formula they belonged to).
- **`dx` 0.3 puts a 1 cm berry at 3 voxels**, already noted as marginal in
  `improvement_plan.md`. Depth measurements inherit that quantization: a
  `holeDepth` of 1.5 cm is 5 voxels, which is enough to be meaningful, but do
  not read anything into sub-cm differences in reported depth.

## Order of work

- [x] H0, class grid. Small, additive, unblocks everything.
- [~] H1, coverage measurement plus the baseline run. Measurement code and the
  independence check are in; the baseline run against a fully packed step-4
  output (the actual curve, not just a mechanism check) is still open.
  Nothing else starts until the two baseline numbers exist.
- [x] H2, pocket-driven targeting -- landed as `FindSpotLocal`, not
  `FindSpotBox` (the FFT path stayed reserved for the medium-kind case this
  driver never hits; see H2's `FindSpotBox`/`FindSpotLocal` section). Landed
  together with H3's push and accept/reject, per the note below that they
  aren't really separable -- measured together on `pocket_fill`: 187 placed,
  visible berry fraction 0.333 -> 0.736, `--validate` clean.
- [~] H3, settle against real material plus reject after settle. Landed
  together with H2 as expected (a bad settle would have hidden a good
  search, so testing them separately was never really an option), then
  **revised mid-session**: the first landing pushed outward against the
  container wall on an aesthetic "visible surface" theory that turned out to
  be wrong -- the object is 3D printed/milled, the container isn't part of
  it, and a berry resting only on it is a loose piece in the physical part.
  Corrected to push toward real neighboring material instead and to hard
  reject zero-item-contact placements (`NudgeResult`, landed as part of the
  correction, not deferred as originally planned). Re-measured after the
  fix: 190 placed, worst-outside-container depth improved 0.6 cm -> 0.3 cm,
  visible berry fraction 0.719 with every accepted placement now guaranteed
  real support. The inward-sink/left-patch rejects are unchanged by the
  correction; the "still open" reject remains a distance-proxy
  approximation, still the largest approximation in H3.
- [x] H4, stopping criteria -- **revised mid-session** on the same physical
  framing as H3: "stop once every deep pocket is filled, a shallow slope is
  fine to leave" replaced the original open-fraction/diminishing-returns
  design, which could stop early with fillable pockets still open. Landed
  as a `deepPocketThreshold`-gated patch eligibility filter plus a single
  "all patches exhausted" stop condition; `patchMaxFails` is now correctly
  documented (and used) as the permanent-exclusion mechanism that makes
  that stop mean what it says. `maxSecondsPerStep`/`maxBerries` remain as
  safety-only backstops. `patchDoneFraction`/`patchMaxBerries` still don't
  exist, but neither was the actual gap this revision needed to close.
- [x] H5, config, plan flag, bench scenario. `pocket_fill` (the bench
  scenario) landed from H2. `PackingStep::fillMode`, the `PackScene`
  dispatch, `PackingConfig` keys and `pack_fruits.cfg` docs landed this
  phase; `useInnerContainer` measured both ways and left off for the final
  step (see H5 above for the numbers, and H7 for why that call is
  superseded, not reversed). Parameter sweep flags in `benchmarks.cpp` did
  not land -- no sweep need has come up yet, and adding flags for a sweep
  nobody has run yet is exactly the kind of ahead-of-need work this plan
  otherwise avoids.
- [x] H6, real-pack validation. Built a small, fast dev/validation
  container (`melone_two.obj` reused as the container, `pack_melone_test.cfg`
  + `pack_melone_test_stage2.cfg`) specifically because bench numbers on the
  real pack were not catching what visual inspection immediately caught.
  Found and fixed three real bugs this way (mouth-anchored targeting
  surviving H3's own fix, accept-window checks measured from the wrong
  reference, the zero-contact hard reject blocking bootstrap from empty),
  and added the `gapingHoleFraction` metric the old ones could not have
  caught this with. Also used it to disprove its own "residual tail is
  probably fine" guess by rendering the result and measuring it: 0 of 496
  placed blueberries had a big-fruit contact, which is what led directly to
  H7.
- [~] H7, volumetric pocket detection scoped to near-surface only.
  Implemented and measured on the melon dev container (`PocketVolume.h/.cpp`,
  `VolumeDriver.h/.cpp`), through two rounds of the user catching a bug by
  looking at the actual rendered mesh that every step-reported number
  missed both times: (1) a single target per component quietly excludes
  the *entire* free volume once that one spot fails a few times, since the
  melon's free space starts out as one giant blob -- fixed by collecting
  several separated local maxima per component instead of one, 17 -> 71
  placed; (2) picking one kind per spot with no fallback abandons an
  otherwise-good spot the instant the single largest eligible kind doesn't
  physically fit there -- fixed by ranking kinds largest-first and
  cascading down at the same spot, 71 -> 251 placed on the identical
  scene. Final state: 251 blueberries, 0 rejected for no item contact,
  median 0.9 cm from the container wall, open ray fraction 0.017 (down
  from 0.227 before either fix) -- H6's "blanket hugging the boundary"
  finding directly refuted, not just avoided. Not yet wired into
  `PackSurfaceStep`/the real hand pack; not yet measured at hand-pack grid
  scale (~22M voxels vs. the melon test's ~150k), where the per-success
  full-field rebuild may need to become incremental. `SurfaceCoverage`/
  `PocketPlanner`/`PocketDriver` are untouched and still what the real
  pipeline runs today.
- [ ] H8, shell-local higher-resolution grid for pocket detection. Design
  only, not started -- the user's own read of H7's `SaveVolAsObjMesh`
  voxel dump was "mostly limitations of voxel resolution," not a third
  logic bug, and the decision was to refine locally (a second, finer-`dx`
  grid scoped to the free region's own bounding box) rather than revert to
  ray-based detection, which would reintroduce H6's blind spot. Written
  down per the user's explicit instruction, while they test a different
  approach in parallel -- check in before implementing.
