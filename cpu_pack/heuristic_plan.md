# Packing heuristics for surface density (final step)

Status legend: `[x]` done, `[~]` partially done, `[ ]` not started.
Companion to `improvement_plan.md`, which is about speed. This one is about
what the packer aims at. Speed work is done to the point where the final step
places ~900 berries per minute, so the open question is no longer how fast a
berry can be placed but whether that berry was worth placing.

## Goal

The sculpture is only seen from outside. The final step exists to make the
container surface *look* dense, not to fill the container. So:

- a berry that closes a hole a viewer can see is worth placing.
- a berry buried behind a big fruit, or dropped into the middle of a void two
  hand-widths deep, is wasted: it costs the same and changes nothing.
- the step should stop when the surface stops improving, not when the geometry
  runs out of room. Total berry count is an output, not a target. The 6000
  figure in `improvement_plan.md` is an estimate of what a dense surface takes,
  and if the surface is dense at 3000 the step is finished.

Method, in one sentence: cast rays inward from the container surface, group
them into neighborhoods, place a berry in the mouth of any neighborhood whose
rays see a deep pocket, and retire a neighborhood once its rays already land on
berries.

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

- [ ] new `Array3D8u classGrid` on `PackingScene`, same size and origin as
  `bg.vox`, values `0` free, `1` container (exterior plus wall), `2` big or
  medium item, `3` small item, `4` inner container shell. One byte per voxel at
  616x192x192 = **22.7 MB**, the same order as `bg.vox` at 21.7 MB, which is
  affordable next to the 356 MB RSS measured in `improvement_plan.md`.
- [ ] fill `1` in `PrepareBackground` from the same pass that calls
  `InvertContainer`, `4` in `AddInnerContainer`.
- [ ] `Put` (`PackingScene.cpp:29-61`) writes its class alongside the `Union`,
  with item classes overwriting container and inner-shell classes. A berry
  resting in the wall region must read back as a berry or the depth measurement
  in H1 mis-attributes it. Class comes from the item's max extent against the
  same `SIZE_THRESH` 3 cm boundary `PlanPackingSteps` uses, so the two
  definitions of "small" cannot drift apart.
- [ ] `LoadPack` gets the same treatment for free, since it goes through `Put`.
- [ ] add the grid to `MeasureSceneMemory` so it shows up in the bench memory
  table rather than as unexplained RSS.

## H1: measure the surface first, change nothing

The coverage metric is the whole plan's yardstick, so it is built and read
before any heuristic changes. New `SurfaceCoverage.h/.cpp`.

Ray set:
- [ ] sample the container surface at ~1 cm spacing with outward normals. The
  container is 31k triangles over roughly 2 m^2, i.e. edges already near 1 cm,
  so `SamplePoints(container.mesh, eps, pts)` plus a hash-cell subsample is
  enough. `SubsampleMeshVertices` (`PointSample.cpp:123`) drops normals, so it
  needs an index-returning variant, or take normals straight from
  `container.mesh.nv`, which is already populated because `ComputeSDF` calls
  `ComputePseudoNormals` on the container. Expect **15-20k rays**.
- [ ] verify the normal sign against `classGrid` rather than assuming it: step
  from `p` along `-n` and confirm the class goes container -> free, not free ->
  container. A flipped container mesh would otherwise silently produce a
  perfectly covered surface.
- [ ] march each ray inward with a DDA over `classGrid` at `dx` 0.3, skipping a
  leading run of class 1 or 4 voxels capped at ~3 voxels of wall thickness,
  then counting free voxels until the first class 2 or 3 voxel, up to
  `maxProbeDepth` (start at 12 cm).
- [ ] per ray record `depth`, `hitClass`, and the world point at the pocket
  mouth (the point `berryRadius + dx` inward from where the free run starts).
- cost: 20k rays x at most 40 voxel reads = under a million reads, i.e. a few
  ms for a full rebuild. Cheap enough that correctness beats cleverness here;
  the incremental update in H4 is an optimization, not a requirement.

Neighborhoods:
- [ ] group rays by a spatial hash on their surface point at
  `patchSize` (start at 4 cm, i.e. a few berry diameters). Expect **1000-1500
  patches** over the container. Per patch keep: ray count, open ray count, mean
  and max open depth, first-hit class histogram, berries placed by this step,
  consecutive failed searches, retired flag.
- a ray is **open** when `depth > holeDepth` (start at `holeDepth` = 1.5 cm,
  about one berry diameter; a hole shallower than a berry cannot be filled by
  one and is not what makes a sculpture look sparse).
- openness deliberately ignores `hitClass`: a ray that travels 3 cm and hits
  the inner shell is a visible hole even though the shell is not real. Class is
  used only for retirement.

Metrics and diagnostics:
- [ ] `CoverageReport`: open ray fraction overall and per patch percentile, the
  depth histogram of open rays, patch count by state, and **visible berry
  fraction** -- the share of berries placed by the final step that are the
  first hit of at least one ray. That last number is the direct measure of
  waste and is the one to beat.
- [ ] dump the open ray mouths as an obj point cloud via `SavePointsObj` so the
  pockets can be looked at in Blender next to the existing
  `bpy_scripts/load_trajectories_inst.py` output, and a couple of
  `classGrid` slices via `SaveSlice`.
- [ ] print the report at the start and end of the final step, and on the 2 s
  heartbeat already in `PackStep`.

- [ ] **baseline run**: resume from a pack file, run today's step 4 unchanged,
  and record open ray fraction and visible berry fraction against berry count.
  Everything after this is judged against those two curves. Suspicion to
  confirm or kill: most berries after the first few hundred are invisible.

Independence check, because the metric is also the objective and a self-graded
score is worth little:
- [ ] a second, independent ray set for reporting only -- orthographic rays
  from ~26 fixed directions on a 1.5 cm lattice, ~100k rays, never used for
  targeting. If the driving metric improves and this one does not, the
  heuristic is gaming the sampling.
- [ ] eyeball at least one render per configuration. No number replaces that
  for an aesthetic goal.

## H2: drive placement from pockets instead of from the lattice walk

New code path for the final step only, so every earlier step keeps its current
behavior bit for bit. Call it `PackPocketStep` in `PackingDriver.cpp`, selected
by a flag on `PackingStep` (H5). The outer loop is over *pockets*, not over
kinds and cells:

```
while (!StopStep()) {                      // H4
  patch = NextPatch()                      // traversal order, below
  if (!patch) break
  ray   = PickTargetRay(patch)             // rim first, below
  kind  = PickKind(patch, ray)             // size matched, below
  spot  = FindSpotBox(box(ray, kind), ...) // local search, below
  if (!spot) { patch.fails++; continue }
  settled = Nudge(...)                     // H3
  if (!AcceptSettle(settled, ray, patch)) { patch.rejects++; continue }
  Put(...); ReprobeLocal(patch)            // H3
}
```

### Traversal order over patches

- [ ] priority `openRayCount * meanOpenDepth`. Ray count alone favors big flat
  patches; depth alone favors one deep crevice.
- [ ] visit **round robin over the surface**, not worst patch first. Draining
  the worst patch produces a dense blob next to a bare stretch, which reads
  worse than uniform mediocrity. Concretely: bucket patches into 4-5 priority
  tiers, and within a pass walk the highest non-empty tier in an order that
  strides along the container's long x axis (patch index sorted by x, stepped
  with a large coprime stride) so consecutive placements land far apart. One
  pass = at most one placement attempt per patch, then re-tier and start the
  next pass.
- [ ] a patch that fails or is rejected goes to the back of the current pass
  rather than being retried immediately. Immediate retry on the same target
  point burns the whole per-patch budget on one impossible pocket.
- [ ] deterministic given the seed: patch order derives from patch index and
  pass number only, and rotations come from the fixed `scene.randAngles`. A
  run has to be reproducible or the metric comparisons in H1 mean nothing.

### Which ray in the patch to aim at: rim first

Aiming at the *deepest* ray centers the berry in the hole and leaves a ring of
open rays around it -- the failure mode discussed in H3. Aim at the edge of the
hole instead:

- [ ] build a ray adjacency graph once, from the same hash used to build
  patches at the ray spacing (~1 cm): each ray links to the rays in the 26
  neighboring hash cells within ~1.5x the spacing. Cheap, static, ~20k nodes.
- [ ] classify each open ray as **rim** if any neighbor is covered
  (`depth <= holeDepth`), **interior** otherwise.
- [ ] target the rim ray with the greatest depth. Filling from the rim inward
  means the new berry always has an already-covered neighbor to sit against, so
  it gains a contact and the hole shrinks from its boundary. It also makes the
  open region simply connected as it closes, instead of fragmenting into rings.
- [ ] if a patch has open rays but no rim ray -- a hole wider than the patch --
  fall back to the deepest interior ray, and let the neighboring patches supply
  the rim on later passes.
- [ ] target point = the pocket mouth of that ray, i.e. `berryRadius + dx`
  inward from where the ray's free run begins. The berry belongs at the mouth:
  the aim is to block the line of sight, and a berry 8 cm down blocks nothing.

### Which kind to place: size matched to the mouth

- [ ] estimate a mouth diameter per target ray as the distance to the nearest
  covered ray in the adjacency graph, doubled, clamped to the small-kind size
  range (0.85 to 3.0 cm).
- [ ] pick the **largest small kind that fits** that diameter, breaking ties by
  the kind used least so far. A 3 cm hole plugged with a 0.85 cm berry leaves a
  visible gap ring and costs a placement; the reverse simply fails the search.
- [ ] keep variety honest: the 5.3 run placed 49-50 of each of the 10 kinds out
  of 497, and that even spread is part of the look. Report the per-kind
  histogram in the bench scenario so size matching does not silently collapse
  onto one or two kinds. If it does, cap any kind's share (say 25%) and take the
  next best fit.

### The local search: `FindSpotBox`

- [ ] `FindSpotBox(bg, part, pos, rot, const Box3f &searchBox, const SpotScore &)`
  in `PackingOps.cpp`. `FindSpotSubgrid` (`PackingOps.cpp:283`) already computes
  a crop box from `cellIdx`, crops `bg.vox` into a temporary `MeshConvo`, and
  then post-checks the container boundary and cell boundary; split it so the box
  comes from the caller, and keep the existing entry point as a thin wrapper
  that derives the box from `cellIdx`. No behavior change for existing callers.
- [ ] search box = target point +/- `(itemExtent/2 + slack)`, `slack` ~1 cm, so
  a **3-4 cm box instead of a 20 cm cell**. With the crop margin of
  `itemExtent` on each side that is ~22 voxels per side at `dx` 0.3, against
  ~77 today, i.e. **~40x fewer voxels**. This is the same win
  `improvement_plan.md` 5.4 is chasing, arrived at from the other end: the
  search gets cheap because it is aimed, not because the FFT got faster.
- [ ] **direct voxel overlap test as the primary path.** At that size the
  candidate set is ~12^3 = ~1700 offsets on the `dx` lattice and a berry
  footprint is ~30-100 voxels, so an exhaustive test is ~100k byte reads with an
  early-out on the first blocked voxel -- cheaper than any FFT, and it yields
  *every* free offset rather than one argmax, which the scoring below needs.
  Keep the FFT path for boxes above a threshold and for the medium kinds.
  - footprint = the item's voxelization at that rotation. Cache it per (kind,
    rotation), which is `improvement_plan.md` 5.4's first item; ~10 kinds x a
    few rotations is a tiny table at a ~100% hit rate.
  - the free-space predicate stays exactly `bg.vox == 0` for every footprint
    voxel, the same condition `Quantize(conv) == 0` expresses, so the two paths
    are comparable by construction.
  - [ ] verify agreement: run both on a few hundred real cases and assert the
    free/blocked verdict matches per offset. A silent disagreement here shows up
    as overlapping fruit, not as a crash.
- [ ] the two post-checks in `FindSpotSubgrid` change meaning for this path.
  The container boundary check stays (a berry must not poke outside). The
  **cell boundary check is replaced by a patch check**: the settled center must
  stay within the target patch plus one patch of slack, so a berry aimed at one
  pocket cannot be credited to another.
- [ ] the 0.5 cm shrink `FindSpotSubgrid` applies to sub-5 cm items
  (`PackingOps.cpp:295-310`) makes a 1 cm berry 0.5 cm, i.e. **halves it**, and
  the search then reports a fit the real berry may not have. That was tolerable
  when the goal was volume; for surface work it means the berry ends up deeper
  and looser than intended. Make the shrink a parameter, set it to ~1 voxel
  (0.3 cm) or zero for this path, and measure the effect on the reject rate in
  H3.

### Scoring: maximize rays closed, not proximity

- [ ] new `SpotScore` mode `POCKET`: for each free candidate offset, count the
  **open rays of the patch whose mouth segment the candidate footprint would
  block**, and maximize that count. Only the patch's own 15-30 rays need
  testing, and each test is a short walk along a ray that already has its
  origin and direction cached, so this is on the order of 30 segment tests per
  candidate -- affordable at ~1700 candidates, and it can be restricted to the
  best few dozen candidates by a cheap pre-rank on distance to the target point
  if it turns out not to be.
- [ ] tie-breaks in order: fewer voxels of the footprint deeper than the mouth
  (prefer sitting shallow), then more already-occupied voxels adjacent to the
  footprint (prefer contact, which H3 needs), then distance to the target point.
- [ ] this replaces `factor * dist + positionWeight * (x + y + z)`
  (`PackingOps.cpp:90`) for this path only. The existing score stays the default
  so no other step moves. Note the corner pull in that expression is a
  deliberate tie-break for volume packing and would fight the pocket score, so
  it must not be carried over.
- [ ] rotation trials: a berry is nearly isotropic, so drop the trial count for
  this path (start at 3 instead of `maxTrialCount` 10) and spend the budget on
  more patches. Confirm against the placement rate rather than assuming; the
  non-berry small kinds (blackberry, raspberry) are not isotropic.

### Bookkeeping that must not be reused

- [ ] `item.nextCellIdx` and `item.noMoreFit` (`MeshInfo.h:26`) are the lattice
  walk's state and have no meaning here. The pocket path must not touch them,
  and in particular must not retire a *kind* on failure: a kind that fails three
  narrow pockets is still the right kind for a wide one. Retirement is per patch
  only (H4).
- [ ] the round-robin `startNameIndex` / `count` loop in `PackStep`
  (`PackingDriver.cpp:158-260`) is replaced wholesale by the pocket loop, not
  wrapped around it. Reuse only `placeItem`'s body -- force direction, `Nudge`,
  `Put`, trajectory recording, periodic saves -- which should be factored out of
  the lambda so both drivers share it.

## H3: settle into the pocket corner, not just outward against the wall

Why outward at all: the container is a virtual hand-shaped boundary, not a
visible shell. The visible surface of the sculpture is the fruit closest to that
boundary, so "outward" and "into the visible layer" are the same direction, and
today's inward push (`sdfFactor = -1`, `PackingDriver.cpp:183`) is pointed the
wrong way for this step's purpose.

### Outward alone is not enough: the residual gap

A pure `+n` push wedges the berry against the container wall and stops there.
The wall is locally smooth, so that is typically a **single contact**, and
nothing pulls the berry sideways toward the fruit that forms the rim of the
pocket. Two different gaps are left behind, and they matter differently:

- **Gap behind the berry**, between it and the fruit mass deeper in. Invisible
  from outside, so it costs nothing aesthetically -- the whole point of aiming
  at the mouth is to stop caring what is behind. But it means the berry is
  supported only by a virtual wall, i.e. in the physical build it is floating and
  needs glue or a neighbor. Measure and report it; do not chase it.
- **Ring gap around the berry at the mouth**, between it and the fruits that
  form the pocket rim. This one is **visible and defeats the placement**: a disc
  of open rays becomes a ring of open rays, the patch still looks sparse, and the
  berry has bought almost nothing.

The ring gap is the real defect, and it has three lines of defense, cheapest
first.

### Defense 1: aim at the rim, so a neighbor is there to touch (H2)

Rim-first target selection is what keeps this from arising in the first place. A
rim ray has a covered neighbor by definition, so the mouth point sits next to
existing fruit rather than in the middle of a hole, and there is a corner --
wall on one side, fruit on the other -- for the berry to settle into. Centering
on the deepest ray is what manufactures rings. This is why H2 targets rims even
though deepest-first looks like the more obvious greedy choice.

### Defense 2: push into the corner, not just outward

- [ ] push direction = a blend of outward and tangential descent:
  `dir = normalize(wn * n + wt * t)`, `wn ~ 1`, `wt ~ 1`, where `n` is the
  container surface normal at the target ray and `t` is the tangential direction
  toward the **shallowest neighboring ray** in the adjacency graph, i.e. the
  direction in which existing fruit comes closest to the surface. `t` is the
  tangential part of the negative gradient of the ray depth field, projected to
  remove the `n` component. This slides the berry along the wall into the
  wall-fruit corner instead of stopping at first wall contact.
- [ ] `ForceDirection` (`PackingScene.cpp:63`) already blends a gravity term
  with an SDF term, so this is a new blend for this path, not a rewrite. Keep
  `step.force` as a weak global bias only.
- [ ] if the single blended push leaves too many ring gaps, escalate to a
  **two-stage settle**: stage 1 pushes `+n` to reach the visible layer, stage 2
  pushes along `t` with the `n` component of motion suppressed so the berry
  cannot sink back in. Two `Nudge` calls at ~0.5 ms each for a berry
  (`improvement_plan.md`: 66.6 ms per placement total, narrowphase 26% of it),
  so the cost is affordable if it is needed. Try the single blend first and let
  the measured ring-gap rate decide.
- [ ] contact count is the cheap proxy for "settled into a corner":
  `GatherActiveContacts` already produces the contact set inside `Nudge`, so
  return the final count and how many of those contacts are with items rather
  than with the container. Target: at least one item contact. A berry with only
  wall contacts is the floating case above.

### Defense 3: let the next pass fill the ring

- [ ] a ring gap leaves its rays open, so rim-first targeting will come back to
  them on a later pass, and H2's size matching will pick a smaller kind for the
  narrower opening. The ring is self-healing **provided the patch is not retired
  while the ring is still open**. That is a hard constraint on H4: patch
  retirement must key on open rays and hit class, and `patchDoneFraction` must
  be low enough that a ring of a few open rays does not read as done. Flagged
  again in H4.

### Accept or reject a settled placement

`Nudge` returns a transform and only `Put` commits it
(`PackingDriver.cpp:178-190`), and `Nudge` mutates nothing but the `kindGrids`
cache, so a settled placement can be dropped for free.

- [ ] reject when the berry sank inward more than about one berry diameter from
  the mouth point, when its center left the target patch plus one patch of
  slack, or when the re-probe of the target ray shows it still open.
- [ ] accept with a warning, rather than rejecting, when the only complaint is
  zero item contacts: it still closes the ray, and the fabricator can glue it.
  Count these separately so the number is visible.
- [ ] count rejections by reason. A high inward-sink rate means the force blend
  is wrong; a high left-the-patch rate means the search box or the shrink factor
  is wrong; a high still-open rate means the scoring is picking offsets that do
  not actually block the ray, which would be a bug in the H2 score.

### After a commit

- [ ] re-cast only the rays of the touched patch and its neighbors and record
  the drop in open ray count. That per-berry number is the efficiency signal H4
  stops on.
- [ ] record, per placement: rays closed, rays still open inside the footprint's
  own neighborhood (the ring measure), contact counts, and the settle distance
  which `placeItem` already logs. These are the numbers that tell the difference
  between "the search is bad" and "the settle is bad", and without them the two
  are indistinguishable in the output.

## H4: stopping criteria

Per patch, retire when any of:
- [ ] `openRays == 0`, or open fraction below `patchDoneFraction` (start 0.15).
  The hole is filled.
- [ ] every ray in the patch whose first hit is an item hits **class 3**, i.e.
  the visible layer here is already all berries. This is the user's rule
  directly: more berries here would only stack berries on berries. Guard it
  with a minimum ray count so a patch of three rays cannot retire on noise.
- [ ] `consecutiveFailedSearches >= patchMaxFails` (start 3). Geometric dead
  end; the pocket mouth is narrower than the smallest berry.
- [ ] `berriesPlaced >= patchMaxBerries` (start ~8 at a 4 cm patch). A fairness
  cap so no single patch can absorb the run, and a cheap guard against a
  runaway loop if a re-probe ever fails to converge.

The ring gap from H3 constrains the first and last of those. A berry that leaves
a ring of open rays around itself has *not* finished its patch, so:
- [ ] `patchDoneFraction` at 0.15 of a 20-ray patch is 3 rays, which is about
  the size of a ring left by one badly settled berry. If the ring measure in H3
  reports rings of that size, lower it or gate retirement on "no open rim ray
  remains" instead of a fraction.
- [ ] `patchMaxFails` counts *consecutive* failures and must be reset by any
  successful placement in the patch, or a patch that fails twice on a narrow
  pocket and then succeeds elsewhere retires with its ring still open.
- [ ] `patchMaxBerries` is a fairness cap, not a quality judgment: log how often
  it is what retires a patch. If it is common, the patch is genuinely a deep
  crevice that berries cannot fill and no amount of berries will fix it.

For the step, stop when any of:
- [ ] all patches retired.
- [ ] global open ray fraction below `targetOpenFraction` (start 0.05). This is
  the criterion that expresses "the container does not need to be full".
- [ ] **diminishing returns**: over the last `K` placements (start 50), the
  open ray count fell by less than `K * minRaysPerBerry` (start 0.5). A berry
  that closes less than half a ray is not paying for itself. Report the reason.
- [ ] the existing `maxSecondsPerStep` and an optional max berry count, kept as
  backstops only.

Log the exit reason the same way `PackStep` already does, plus the final
coverage report, so a run that stopped early and a run that ran out of room are
told apart at a glance.

## H5: plumbing

- [ ] `PackingStep` gets a fill mode (`fillVolume` today vs `fillSurface`), set
  on `lastStep` in `PlanPackingSteps` (`PackingPlan.cpp:228`). `PackingStep::Load`
  reads fixed tokens in order, so the new field must be optional on read or
  every saved plan breaks; treat a missing trailing token as `fillVolume`.
- [ ] `PackingConfig` keys, all with the defaults above, all dumped in
  `toString()` and documented in `pack_fruits.cfg`: `surfaceRaySpacing`,
  `patchSize`, `holeDepth`, `maxProbeDepth`, `targetOpenFraction`,
  `patchDoneFraction`, `patchMaxFails`, `patchMaxBerries`, `minRaysPerBerry`,
  `pocketTrialCount`.
- [ ] whether the final step still needs `useInnerContainer` becomes a
  measurable question rather than an assumption: with pocket targeting the
  inner shell should be redundant. Run it both ways and keep whichever gives
  the better visible berry fraction. If it stays, `classGrid` value 4 keeps it
  distinguishable in the diagnostics.
- [ ] bench scenario `pocket_fill` next to `smallfruit_packstep` in
  `BenchSmallFruit.cpp` / `BenchRegistry.cpp`: resume a pack file, run the
  pocket step under a wall budget, report berries placed, open ray fraction
  before and after, visible berry fraction, rays closed per berry, rejected
  settles, and patch states. Write the pack file so the result can be rendered.
- [ ] `--validate` must still pass. Aiming berries at the visible layer pushes
  them against the container wall, so the *worst outside container* number is
  the one at risk here, not overlap. Baseline to hold: 0 failures, worst
  outside 0.3-0.6 cm, mean overlap ~0.32 cm from `improvement_plan.md` 5.3.

## Files: what to add, what to change, what not to touch

`CMakeLists.txt:5` globs `*.cpp` with `CONFIGURE_DEPENDS` into `pack_core` and
removes only `main.cpp` and `benchmarks.cpp`, so **no build file edits are
needed** for any new source below.

### New files

| file | contents | depends on |
| --- | --- | --- |
| `VoxClass.h` | the class enum (`VOX_FREE`, `VOX_CONTAINER`, `VOX_ITEM_BIG`, `VOX_ITEM_SMALL`, `VOX_INNER`) and `VoxClassOfExtent(maxExtent, smallThresh)`, the single definition of "small" shared with `PlanPackingSteps` | nothing |
| `SurfaceCoverage.h/.cpp` | H1 measurement only, no policy: `CoverageParams`, `SurfaceRay`, ray set construction from the container mesh, the normal sign check, the ray adjacency graph, the DDA march over the class grid, rim/interior classification, patch grouping by spatial hash, `CoverageReport` and its printer, obj dump of open ray mouths, local re-probe of one patch and its neighbors | `Array3D8u`, `TrigMesh`, `PointSample`, `VoxClass` |
| `PocketPlanner.h/.cpp` | H2/H4 policy: `PocketPatchState` (open counts, fails, rejects, berries placed, retired), priority tiers and the strided traversal, `NextPatch`, `PickTargetRay` (rim first), `PickKind` (size matched with the share cap), per-patch retirement and the step-level stop test with its reason string | `SurfaceCoverage` only |
| `VoxFootprint.h/.cpp` | voxelization of one kind at one rotation, cached by (kind, rotation) at a given `dx`, with a `MemoryBytes()`. Serves both the local search here and `improvement_plan.md` 5.4's `fg_voxelize` cache | `TrigMesh`, `cpu_voxelizer`, `Array3D8u` |
| `PackingOpsLocal.cpp` | `FindSpotLocal`: candidate offset enumeration on the `dx` lattice, the direct overlap test with early-out, and the `POCKET` score that counts open rays blocked. Separate translation unit, but declared in the existing `PackingOps.h` so callers see one search interface family | `PackingOps.h`, `VoxFootprint`, `SurfaceCoverage` |
| `PocketDriver.h/.cpp` | `PackPocketStep(scene, step, cfg)`: the H2 loop skeleton, the H3 accept/reject test, per-placement and heartbeat logging, and the exit reason | `PackingScene`, `PocketPlanner`, `SurfaceCoverage`, `PackingDriver` (for the shared commit) |
| `BenchPocket.cpp` | the `pocket_fill` scenario, plus a `coverage_baseline` scenario that only measures a loaded pack so H1 can be read before H2 exists | `Benchmark.h`, the above |
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
| `PackingScene.h/.cpp` | add `Array3D8u classGrid`; stamp it in `Put` (`:29-61`) next to the existing `Union`; write `VOX_INNER` in `AddInnerContainer` (`:1273`); `Nudge` gains a defaulted `NudgeResult *out = nullptr` reporting final contact count and how many contacts were with items (5 existing callers keep compiling: `PackingDriver.cpp:186`, `DebugTools.cpp:62`, `BenchGrowth.cpp:83`, `BenchSmallFruit.cpp:90`, `BenchNudge.cpp:57`); `PackingConstraints` gains an optional "suppress motion along this axis" plane constraint for the H3 stage-2 settle, honored in `ApplyPositionConstraints` (`:906`) |
| `GridUtils.h/.cpp` | `StampClass(dst, offset, mask, cls, overwriteMask)` mirroring `Union` in `MeshConvo.cpp:92`, and `ClassFromBinary(vox, cls)` to seed the container class in one pass next to `InvertContainer` (`:128`). Generic array ops stay here; the semantics stay in `VoxClass.h` |
| `PackingOps.h/.cpp` | `SpotScore` struct with the existing behavior as the default mode; thread it through `FindSpot`'s score loop (`:90`) so the corner pull is no longer hardcoded; split `FindSpotSubgrid` (`:283`) into cell-box computation plus `FindSpotBox(bg, part, pos, rot, searchBox, score)`, keeping the old entry point as a wrapper; make the 0.5 cm small-item shrink (`:295-310`) a parameter; declare `FindSpotLocal` |
| `PackingDriver.h/.cpp` | fill the container class in `PrepareBackground` (`:53`); factor the `placeItem` lambda body (`:178-215`) into a shared `CommitPlacement` plus `MaybeSaveProgress` so `PocketDriver` reuses force direction, `Nudge`, `Put`, trajectory recording and the periodic saves instead of copying them; dispatch on the step's fill mode in `PackScene` (`:283`) |
| `PackingPlan.h/.cpp` | `PackingStep::fillMode`; set it on `lastStep` (`:228-234`); emit it in `toString()`; read it as an **optional trailing token** in `Load` so existing saved plans still parse |
| `PackingConfig.h/.cpp` | the H5 key list, with defaults, `toString()` entries and `LoadFromFile` cases. Unknown keys are already skipped rather than fatal, so an old `pack_fruits.cfg` keeps working |
| `pack_fruits.cfg` | document each new key with its measured effect, matching the style of the existing `gridDx` comment |
| `MemStats.h/.cpp` | rows for `classGrid` (~22.7 MB) and the `VoxFootprint` cache, so the added memory shows up in the bench table and not as unexplained RSS |
| `BenchRegistry.cpp` | register `coverage_baseline` and `pocket_fill` (the file is just a decl list plus a table, `:1-40`) |
| `BenchReport.h/.cpp` | coverage rows in the report and the csv: open ray fraction before and after, visible berry fraction, rays closed per berry, rejects by reason, patch states |
| `benchmarks.cpp` | flags for the H1/H2 parameters worth sweeping without a rebuild, in the style of the existing `--griddx`: `--patchsize`, `--holedepth`, `--rayspacing` |
| `DebugTools.h/.cpp` | `DebugCoverage(scene)`: dump the ray field and class slices for a loaded pack. Same role as the existing `DebugPointSampling` / `DebugNudge` |
| `Profiler` usage | no file change, only new scope names: `coverage.build`, `coverage.march`, `coverage.reprobe`, `pocket.select`, `findspot_local.total`, plus tally counters `count.rays_open`, `count.candidates`, `count.settle_reject` |

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
| H0 | `VoxClass.h` | `PackingScene`, `GridUtils`, `PackingDriver` (`PrepareBackground`), `MemStats` |
| H1 | `SurfaceCoverage`, `BenchPocket.cpp` (`coverage_baseline` only) | `BenchRegistry`, `BenchReport`, `benchmarks.cpp`, `DebugTools` |
| H2 | `PocketPlanner`, `VoxFootprint`, `PackingOpsLocal.cpp`, `PocketDriver` | `PackingOps`, `PackingDriver` (shared commit + dispatch) |
| H3 | -- | `PackingScene` (`NudgeResult`, plane constraint), `PocketDriver` |
| H4 | -- | `PocketPlanner` |
| H5 | `bpy_scripts/load_pockets.py` | `PackingPlan`, `PackingConfig`, `pack_fruits.cfg`, `BenchPocket.cpp` (`pocket_fill`) |

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
- **Rays from the container surface do not see everything a camera sees.** They
  are normal rays from a closed surface, so a deep crevice between two fingers
  is sampled from both walls but not from the direction a viewer stands. If the
  renders disagree with the metric, adding the ortho set as a second *driving*
  set is the fix, not more tuning.
- **Berries at the visible layer may have no item contact at all**, since the
  container wall they rest against is virtual. That is an aesthetic success and a
  fabrication problem at the same time, and the packer cannot resolve the
  tension: it can only report how many placements are in that state so the
  decision about glue or a support is made with a number. If the count is large,
  the answer is probably to bias `wt` up in the H3 blend and accept a slightly
  deeper berry.
- **`dx` 0.3 puts a 1 cm berry at 3 voxels**, already noted as marginal in
  `improvement_plan.md`. Depth measurements inherit that quantization: a
  `holeDepth` of 1.5 cm is 5 voxels, which is enough to be meaningful, but do
  not read anything into sub-cm differences in reported depth.

## Order of work

- [ ] H0, class grid. Small, additive, unblocks everything.
- [ ] H1, coverage measurement plus the baseline run. Nothing else starts until
  the two baseline numbers exist.
- [ ] H2, pocket-driven targeting with `FindSpotBox`. The largest change, and
  the one that should move the visible berry fraction most.
- [ ] H3, outward settle plus reject after settle. Cheap once H2 lands, and H2
  is not really testable without it since a bad settle hides a good search.
- [ ] H4, stopping criteria. Only meaningful once the placements are aimed.
- [ ] H5, config, plan flag, bench scenario, parameter sweep.
