# cpu_pack: main.cpp findings

## Measured results

Numbers from `cpu_pack_bench` on the 95-item `fruits_1` set and the
`finger4.8m` container (bg grid 616x192x192 at dx 0.3), resuming from
`pack_944_0721.txt`. Run `cpu_pack_bench --list` for the scenarios; it
writes `results.txt` and `summary.html` next to the executable and holds
itself to a 300 s wall budget by default.

Target scale for reading these numbers: the container holds a few hundred
fruits, and the item catalogue stays near 100 kinds. Scenarios therefore
place only a handful of items each and report per-placement cost and
growth trend rather than large-N throughput.

Ranked by impact:

1. **The retry storm dominates everything.** `end_to_end` on plan step 3
   spent 177 s to place 4 items: 9091 `FindSpotSubgrid` calls, 99.1% of
   wall time, i.e. ~2270 failed searches per success. With the container
   already at 944 items nearly every cell x rotation combination fails,
   and each failure still pays a full crop + FFT + IFFT. Cutting the
   number of *attempts* matters far more than making an attempt faster.
2. **`bg_fft` is recomputed per trial, as suspected.** Full-container
   `FindSpot` costs 474 ms per call, of which `bg_fft` is 98 ms (21%).
   `bg` only changes after a `Put`, so 9 of every 10 trials recompute an
   identical transform. Caching it saves ~20% of the big-item path
   outright.
3. **The instance caches grow without bound, as suspected.** 9 placements
   into the resumed pack took `instanceMeshCache` from 0 to 52 MB over 51
   entries (~1 MB per cached neighbor) plus 12 MB of TrigGrids, and RSS
   from 196 MB to 280 MB. Nothing is ever evicted. At the real target
   scale, a few hundred instances in the container, that is a few hundred
   MB of cache against a 130 MB item-mesh floor: unpleasant but survivable
   on a desktop, so this is a cost-of-doing-business issue rather than the
   run-ending one. Worth capping, but it ranks below the retry storm.
4. **Narrow phase is the whole cost of `Nudge`.** `nudge.narrowphase` is
   60-76% of `Nudge` (877 ms of 1594 ms for a big item over 100 steps).
   First-time `nudge.triggrid_build` adds 7%.
5. **The subgrid path is worth its complexity.** Small items search in
   10.4 ms against 474 ms for the full-container path, a 45x saving.

Two things the earlier speculation in this document got wrong:

- **`Put` is not a bottleneck.** 0.485 ms per call and flat in instance
  count (first-quarter mean 0.489 ms vs last-quarter 0.477 ms over 32
  calls); 0.6% of a placement. `PackingScene.h`'s "revoxelizes because
  it's not a bottleneck" comment is correct and needs no revisiting. It
  only shows up in `LoadPack`, where 944 sequential `Put` calls cost 4.1 s
  of the 4.1 s total.
- **The PGS solver is free.** `nudge.pgs_solve` totals 0.2-0.4 ms across
  200-270 calls, under 0.05%. Tuning `pgsPasses` will not buy anything.

Correctness was checked separately with `--validate`, which samples the
new item's surface against a container grid (shell and exterior marked
occupied) and an accumulated fruit grid. It reported 0 violations for
placements made on top of the 944-item pack. That result is only
meaningful because `validate_selftest` confirms the checker rejects a
placement pushed through the container wall (11420 deep samples), one off
the grid (20288), an exact duplicate of a neighbor (2), and one shifted
halfway into a neighbor (2203 samples, up to 2.7 cm deep), while passing
a placement taken straight from the pack file. Building that self-test
found a real bug: the first tolerance model flagged genuine placements,
because `Nudge` deliberately permits 0.2 cm of interpenetration and both
voxelization and the shell marking round outward, stacking to 3 voxels of
apparent overlap at a legitimate contact.

## Pipeline
main.cpp packs fruit meshes into a hand-shaped container:
1. ComputeMeshStats + PlanPackingSteps: group items by size, build ordered PackingSteps.
2. PackFruits -> PackScene: voxelize container, build SDF, run PackStep per plan step.
3. PackStep: for each item, try FindSpot/FindSpotSubgrid (FFT collision search) then Nudge (physics settle), then PackingScene::Put (voxelize + union into bg, add to broadphase).

## Speed

- FindSpot (PackingOps.cpp) does a full-container FFT (bg.FFT) on every single trial (up to MAX_TRIAL_COUNT=10 per item), even though bg's voxel grid only changes after a successful Put(). The background FFT should be cached and only recomputed after Union() mutates bg. Currently it is recomputed for every rotation trial of every item, which dominates runtime for large containers.
- MeshConvo::FFT allocates a fresh Array3D<complex<float>> (and a padded copy of vox) on every call for both bg and fg. Reusing preallocated buffers across trials would cut allocation/copy overhead substantially.
- PackingScene::Put() revoxelizes the placed instance's mesh and re-floods the whole thing per placement; comment says "not a bottleneck" but this is O(mesh size) work done for every one of up to 1000 placements plus every Nudge trajectory step touches broadphase + narrow-phase queries.
- Nudge() builds a TrigGrid (via TrigGrid::Build, which voxelizes + allocates a dense Array3D<uint32_t>) for every newly-encountered neighboring instance the first time it's seen, which is correct amortization, but it happens inside the hot simulation loop's broadphase step, contending with per-step cout logging (std::cout for every placement and every Nudge call) that adds I/O overhead when packing hundreds of items.
- SaveTrajectories/SaveInstances are called periodically (every 10/20 iterations) and each rewrites the *entire* accumulated state to disk from scratch, so total disk I/O grows roughly O(n^2) in number of placed items over a long run.

## RAM growth (main risk for hundreds/thousands of items)

- PackingScene::instanceMeshCache and instanceGrids (PackingScene.h) are unbounded maps keyed by instance id, populated inside Nudge() and never cleared. Each entry holds a full transformed TrigMesh copy plus a TrigGrid with its own dense Array3D<uint32_t> voxel grid and a vector<vector<unsigned>> triangle list. With a debug file named pack_944_0721.txt already referenced in the code, this cache is on track to hold ~1000 mesh+grid copies simultaneously, each with real per-voxel-cell vector overhead (~24 bytes/cell just for empty vector headers, per TrigGrid.h comment). This is the most likely cause of RAM blowing up on long packing runs.
- Fix direction: cap the cache (e.g. LRU by spatial locality using broadPhase results already computed), or drop TrigGrid entries once an instance is far from the current broadphase query region, or share one persistent-but-coarser acceleration structure (e.g. per-cell triangle refs into a single global TrigGrid) instead of one full TrigGrid per instance.
- MeshConvo::vox and fft buffers are reallocated (not reused) for bg and fg every FindSpot call; each is transient so it doesn't accumulate, but for a large container grid the transient peak (vox + padded copy + fft complex array) can spike memory well above steady-state. Reusing scratch buffers reduces peak, not just speed.

## Packing density

- FindSpot's score is `factor * sdf_dist + positionWeight * (x+y+z)`, a fixed linear bias toward one corner. This is a coarse heuristic; it does not reward tighter local contact (e.g. surface-area of contact, or number of neighbor voxels touched), so items may settle in the first "no-collision" cell that satisfies the corner bias rather than the tightest available pocket.
- FindSpotSubgrid shrinks small items (maxExtent<5cm) by 0.5cm before the FFT search "to find spots more easily" (PackingOps.cpp:281-293), which trades density for placement success rate -- shrunk items likely settle looser than their true size, since Nudge afterward starts from an already-slack position.
- MAX_TRIAL_COUNT=10 random-rotation trials per item per subgrid cell is small; increasing trial count (cheaply, once FFT caching in the Speed section is fixed) or trying rotations biased by item PCA/inertia axes instead of pure uniform-random should improve fill density without much extra cost.
- The outer PackStep loop advances startNameIndex by 1 each full round and marks an item noMoreFit after it fails once for subgrid items (nextCellIdx reaches totalCells) or fails all MAX_TRIAL_COUNT trials for non-subgrid items -- once noMoreFit is set it is permanent for the rest of the run, even though later placements may open up new room. Density could improve by re-trying noMoreFit items after N other successful placements, since the container occupancy has changed.

## Code organization

- main.cpp mixes production pipeline (PackFruits/PackScene/PackStep) with one-off debug/experiment functions (MakeInnerMesh, DebugPointSampling, DebugNudge, ComputeMeshStats-as-CLI-tool) and hardcoded absolute paths for two different machines (commented in/out: `F:/meshes/...` vs `/media/desaic/WD/...` vs `/media/desaic/ssd2/...`). Moving debug helpers to a separate debug_tools.cpp/.h and reading paths from argv or a config file would make main.cpp reflect only the current production path.
- PackScene() has several dead locals: NUM_COPIES, outputScale, ANGLE_TRIALS, dxVec, voxRes, origin (main.cpp:213-220) are computed but never used. PlanPackingSteps has an unused LARGE_INT-named constant repurposed inline. Removing these clarifies what actually drives behavior (step.count, scene.dx, subgridCellSize).
- PackStep hardcodes DEBUG_ITEM=0 as the loop's starting index (main.cpp:95) and PackScene hardcodes DEBUG_STEP=3 to skip early plan steps (main.cpp:227) and an absolute LoadPack path resuming from a specific run (main.cpp:224). These look like leftover debug state rather than parameters; promoting them to PackingScene/PackingPlan fields (with a documented "resume" flag) would make resuming a packing run explicit instead of implicit in commented code.
- PackingScene.h documents Put() as "revoxelizes because it's not a bottleneck" -- worth revisiting if profiling data (already printed by Nudge's Stopwatch breakdown) shows otherwise once FFT caching is fixed, since Put's relative cost will grow as other bottlenecks shrink.
- NudgeConstrained and Nudge duplicate most of the simulation loop (broadphase gather, contact gathering, damping, trajectory recording) with different integrators (PGS velocity-based vs PBD position-based). Factoring the shared setup (sample generation, broadphase query, TrigGrid caching) into one helper used by both would reduce ~150 lines of near-duplicate code and reduce risk of the two functions drifting (e.g. contactGatherInterval throttling exists only in Nudge).
