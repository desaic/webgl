# Benchmark + reorg plan (cpu_pack only)

Status legend: `[x]` done, `[~]` partially done, `[ ]` not started.
Checked against the tree and the numbers in `summary.html`.

Goal: a `cpu_pack_bench` target that measures CPU time and RAM for the parts
that actually cost time (FFT, narrow phase, TrigGrid build, Put), on real
meshes from `/media/desaic/WD/meshes/fruit_hand/`.

Reorg was a prerequisite: the packing driver used to live in `main.cpp` with
hardcoded paths and debug constants, so a benchmark could not call it without
duplicating it. Timing was also scattered as local Stopwatch + cout. Both are
now fixed (phases 0-2).

Confirmed available data:
- `fruits_1/` + `stats.txt`; `load_meshes` loads 95 items (2.09 M verts,
  4.19 M trigs)
- `hands/finger4.8m.stl`, `hands/finger_4.8m_inner.stl`
- `pack_944_0721.txt` (~944 placements) for the partially-filled case

## Phase 0: instrumentation (additive, no behavior change)

- [x] New `Profiler.h/.cpp`:
  - [x] Named accumulating counters: total ms, call count, min/max.
  - [x] RAII `ProfileScope` + `PROFILE_SCOPE("name")` macro.
  - [x] `Profiler::Reset()`, `Profiler::Report(ostream&)`, `Profiler::ReportCsv()`.
  - [x] Single-thread accumulation only (pocketfft/FastSweepPar spawn their own
    threads but never touch the profiler). Documented in the header.
  - [dropped] Compile-time off switch so main.cpp release runs pay nothing.
    Only the runtime `SetEnabled` exists and that is enough: a few ms per packed
    fruit of instrumentation and log spam is acceptable, so the added build
    complexity does not pay for itself.

- [x] New `MemStats.h/.cpp`:
  - [x] `RssBytes()`, `PeakRssBytes()` from `/proc/self/status` (VmRSS/VmHWM) with
    `getrusage` fallback.
  - [x] `MeasureSceneMemory(const PackingScene&)` (named `SceneMemoryReport` in
    the plan): logical byte accounting, since RSS alone will not say which
    structure grew. Sums:
    - [x] `bg.vox`, `bg.fft`
    - [x] `sdf->dist` (short, x2 bytes), `sdf->fineGrid`
    - [x] `containerGrid`, `containerInnerGrid`
    - [x] `instanceMeshCache` (mesh v/t/n bytes)
    - [x] `instanceGrids` (grid data + tLists incl. per-vector header overhead)
    - [x] `instances[].trajectory` (grows with every placement)
    - [x] also added beyond the plan: `item.sdfs`, `item.samples`, `broadPhase`
  - [x] `size_t MemoryBytes() const` on `TrigGrid`, `MeshConvo`, `BroadPhase`
    and `PackValidate`. No parent-folder edits; AdapSDF memory is derived from
    its public `dist`/`fineGrid` members.

### Counters to add

`FindSpot` / `ComputeCollisionGrid` (PackingOps.cpp). Note the existing
`fftTimer` there was dead code: it was `Start()`ed 4 times and never read.
Replaced with real scopes:
- [x] `findspot.fg_voxelize`, `findspot.bg_fft`, `findspot.fg_fft`,
  `findspot.dot`, `findspot.ifft`, `findspot.quantize`, `findspot.score_scan`
  (the triple loop calling `sdf->GetCoarseDist` per free voxel)
- [x] `findspot_subgrid.crop` (the manual voxel copy loop in FindSpotSubgrid),
  plus `findspot.total` and `findspot_subgrid.total`
- [x] dead `fftTimer` deleted

`Nudge` (PackingScene.cpp) -- the local broadTime/gatherTime/pgsTime are gone,
replaced by counters that survive across calls:
- [x] `nudge.sample_gen` (first-time SamplePoints + DownsamplePoints + item SDF)
- [x] `nudge.broadphase` (GetNearby only)
- [x] `nudge.triggrid_build` (split out from broadphase; the expensive,
  cache-populating part, measured separately)
- [x] `nudge.narrowphase` (GatherActiveContacts)
- [ ] `nudge.manifold_reduce` (reduceContactManifold, still inside gather and
  therefore still lumped into `nudge.narrowphase`, which is the single largest
  counter in `nudge_partial` at 76%. Worth splitting out.)
- [x] `nudge.pgs_solve`, plus `nudge.total`

`Put`:
- [x] `put.total`
- [ ] the breakdown `put.transform_mesh`, `put.voxelize`, `put.flood`,
  `put.union`. Deprioritized: `put_scaling` measured 0.54 ms flat per Put and
  no growth with instance count, so the breakdown would not change any decision.

Init:
- [x] `init.load_items` (the plan called it `init.load_meshes`),
  `init.container_sdf`, `init.container_grids`, `init.bg_voxelize`
- [ ] `init.inertia_frame`
- [x] `loadpack.total`, added for the resume path

## Phase 1: config + verbosity

- [x] New `PackingConfig.h/.cpp`. Pulls the hardcoded values out of main.cpp so a
  benchmark can build a scenario without editing production code:
  - [x] `dataDir` (default `/media/desaic/WD/meshes/fruit_hand/`, overridable by
    argv[1] or `FRUIT_HAND_DIR` env var, via `ParseArgs`)
  - [x] `containerFile`, `innerContainerFile`, `fruitSubdir`, `outSubdir`
  - [x] `dx` (0.3), `containerSDFDx` (2.0), `broadPhaseDx` (2.0),
    `subgridCellSize`, plus `gridDx`
  - [x] `maxTrialCount` (was `MAX_TRIAL_COUNT` 10), `startStep` (was
    `DEBUG_STEP` 3), `startItem` (was `DEBUG_ITEM` 0)
  - [x] `resumePackFile` (was the hardcoded `LoadPack` path) + `bool resume`
  - [x] `trajSaveInterval` (10), `packSaveInterval` (20); 0 disables the write
  - [x] added beyond the plan: `computeStats`, and `maxSecondsPerStep` so a
    benchmark can stop mid-step (`end_to_end` relies on it)
  - [x] `toString()`, dumped into the config block of summary.html

- [x] New `Log.h`: `SetLogLevel(int)` + `LOGI`/`LOGD` guards around the
  per-placement cout in `placeItem`, the per-call `Nudge` summary line, and
  `LoadPack`'s per-item lines. Benchmarks run silent unless `--verbose`.

## Phase 2: extract the driver from main.cpp

Move, no logic change:
- [x] `PackStep`, `PackScene`, `PackFruits` -> new `PackingDriver.h/.cpp`, taking
  `const PackingConfig&` instead of reading globals/constants. Also split out
  `BuildScene` and `PrepareBackground` so benchmarks can get a ready scene
  without packing anything.
- [x] `MeshStat`, `ComputeMeshStats`, `GetGroupIndex`, `PlanPackingSteps` ->
  `PackingPlan.cpp`.
- [x] `MakeInnerMesh`, `DebugPointSampling`, `DebugNudge` -> new
  `DebugTools.h/.cpp`.
- [x] `main.cpp` shrinks to: parse config, optionally ComputeMeshStats, plan,
  run. Now 30 lines.

Also fix while moving (cheap, unblocks measurement):
- [x] Delete the dead locals in `PackScene`: `NUM_COPIES`, `outputScale`,
  `ANGLE_TRIALS`, `dxVec`, `voxRes`, `origin`.
- [x] `PlanPackingSteps` no longer does the O(groups * names * stats) linear name
  lookup; it builds one `name -> len` map (`PackingPlan.cpp:192`).

## Phase 3: CMake split

The old `FILE(GLOB EXE_SRC "*.cpp" ...)` globbed every cpp into one executable,
so adding `benchmarks.cpp` would have produced two `main()` symbols.
Restructured:

- [x] `pack_core` static lib: all `*.cpp` in cpu_pack plus `quickhull/*.cpp`,
  minus `main.cpp` and `benchmarks.cpp` (via `list(REMOVE_ITEM ...)` on the
  glob, so nothing else needs listing by hand).
- [x] `cpu_pack` exe: `main.cpp` + `pack_core`.
- [x] `cpu_pack_bench` exe: `benchmarks.cpp` + `pack_core`.
- [x] `pack_core` links `Mesh MathLib ImageIO SparseSDF Util MeshSimplification`
  and propagates them to both exes.
- [x] Kept the existing `-O3 -DNDEBUG` release flags, and `CMAKE_BUILD_TYPE`
  defaults to Release when unset.
- [dropped] Print the build type at startup and warn if not Release. Not worth
  the conditional-compilation plumbing; `CMAKE_BUILD_TYPE` already defaults to
  Release when unset.

## Phase 4: benchmarks.cpp

Shared harness (`Benchmark.h`, `BenchScene`, `BenchReport`, `BenchOutput`):
- [x] each scenario gets a name and a one-line description, registered in
  `BenchRegistry.cpp`.
- [x] reports wall ms, `Profiler` counter breakdown, RSS, peak RSS, and the
  logical scene memory table.
- [x] aligned table to stdout, mirrored to `results.txt`, plus `--csv <file>`
  for diffing across commits.
- [x] `summary.html` generated next to the executable, with per-scenario share
  bars and the config dump.
- [x] fixed seed (`randAngles` seeded 123), so runs are comparable.
- [x] shared scene reuse across scenarios plus a `--budget` wall cap.
- [ ] optional warmup run and N repeats reporting mean/min wall. Each scenario
  runs exactly once; `--items N` scales work per scenario instead. This is the
  main remaining gap for run-to-run noise -- `nudge_big` at 5.2 s and
  `end_to_end` at 90 s are single samples.
- [x] added beyond the plan: `--validate` cross-checks every placement against
  the container and its neighbors (`PackValidate`).

Scenarios, cheapest first so a partial run is still useful:

1. [x] `load_meshes` -- load 95 fruits + both containers. Time + RAM baseline.
   Result: 1.37 s, 129 MB of item meshes.
2. [x] `container_setup` -- merged the plan's `container_sdf` with the TrigGrid
   and bg occupancy grid setup, since they always run together. Result: SDF
   167 ms, grids 22 ms, bg voxelize 199 ms.
3. [ ] `triggrid_build` -- `TrigGrid::Build` per fruit, bucketed by size,
   reporting ms and bytes per grid. Not implemented as its own scenario; the
   `nudge.triggrid_build` counter covers the aggregate (3.0 ms mean in
   `cache_growth`) but not the per-size-class unit cost.
4. [x] `findspot_big` -- `FindSpot` on the largest fruits in an empty container.
   Result: 479 ms mean per call, and `bg_fft` is 79 ms of it on every call,
   recomputed identically each time -- the evidence for caching it.
5. [x] `findspot_small` -- `FindSpotSubgrid` on small fruits, cropped FFT.
   Result: 8.2 ms mean vs 479 ms, so the crop is worth ~60x.
6. [x] `findspot_partial` -- re-run after `LoadPack` of `pack_944_0721.txt`.
   Confirmed the prediction: FFT cost unchanged (1.96 vs 1.80 ms), while
   `score_scan` dropped from 0.58 to 0.35 ms.
7. [x] `nudge_big` / `nudge_small` -- `Nudge` on an empty container, broken into
   broad/triggrid/narrow/pgs. Result: `sample_gen` 41% and `narrowphase` 30%
   dominate; `pgs_solve` is 0.001 ms, i.e. free.
8. [x] `nudge_partial` -- `Nudge` in the 944-item container. Result:
   `narrowphase` rises to 76%; `triggrid_build` fires 13 times, as predicted.
9. [x] `cache_growth` -- nudges in the partially-filled container reporting the
   cache entry counts and bytes. Result: 17 entries = 12.1 MB mesh cache +
   3.1 MB grids, unbounded as summary_doc.md warned.
10. [x] `put_scaling` -- `Put()` cost vs instance count. Result: first-quarter
    mean 0.554 ms vs last-quarter 0.539 ms, i.e. flat. This refutes any concern
    about Put; the "not a bottleneck" comment stands.
11. [x] `end_to_end` -- one plan step through the real `PackStep`, disk saves
    disabled. Result: 90 s spent on 5890 failed spot searches for 0 placements
    at the 944-item fill level.

Added beyond the plan:
- [x] `load_pack` -- replay `pack_944_0721.txt`, one `Put` per line. 4.36 s for
  944 instances.
- [x] `smallfruit_batch` -- bulk placement of the smallest berry, reporting the
  FFT shrink.
- [x] `validate_selftest` -- confirms the validator flags known bad placements,
  so the `--validate` numbers can be trusted.

- [x] Selection by `--only <name>` / `--list`.

## Order of work

- [x] Phase 0 -- instrumentation (all but the compile-time off switch).
- [x] Phase 1 -- config + verbosity.
- [x] Phase 2 -- driver extracted from main.cpp.
- [x] Phase 3 -- CMake split, `cpu_pack_bench` builds and runs.
- [x] Phase 4 -- 14 scenarios, results in `summary.html` (168.5 s total wall,
  646 MB peak RSS).
- [~] Phase 5 -- 5.1, 5.2 and 5.3 done and measured, 9.3x on the berry case.
  5.4 is next and is now the majority of a placement. 5.5-5.6 after that, 5.7
  is closed.

## Target scale (the numbers the design must hit)

Fixed by the use case, not by the code. Everything in phase 5 is prioritized
against these.

- Target output per container: ~200 big and medium-big fruits, plus 6000+
  blueberries to make the container surface look dense.
- Kind count is capped at ~100 by hand and will not grow. So placement counts
  come from repeating a few kinds, not from adding kinds.

Measured kind inventory (`fruits_1/stats.txt`, 95 kinds, grouped by the
`SIZE_THRESH = {20, 6, 3}` cm buckets in `PlanPackingSteps`):

| group | max extent | kinds | supplies |
| --- | --- | --- | --- |
| large | > 20 cm | 17 | full-container `FindSpot` path |
| medium large | 6-20 cm | 57 | the ~200 big/medium-big target |
| medium small | 3-6 cm | 11 | subgrid path |
| small | <= 3 cm | 10 | the 6000+ blueberry target |

The 10 small kinds are grape4/5, raspberry1/2, blueberry1, blueberry1_001,
blueberry2, blueberry2_001, cherry, blackberry -- sizes 0.85 to 3.0 cm. Only 4
are literally blueberries.

Derived geometry, from the measured container:
- container extent 184.8 x 57.6 x 57.6 cm (bg grid 616x192x192 at dx 0.3).
- `subgridCellSize` 20 cm gives `numSubgridCells` 10x3x3 = **90 cells**.
- a blueberry is 3-5 voxels across at dx 0.3, so the FFT collision test barely
  resolves one. Workable but marginal; worth re-checking if placements look
  wrong rather than merely slow.

Per-instance unit costs, measured, for extrapolating to target scale:

| quantity | small item | big item | source |
| --- | --- | --- | --- |
| cached mesh + grid per instance | 215 KB | 2.83 MB | nudge_small / nudge_big |
| trajectory per instance | 1.6 KB | 5.1 KB | nudge_small / nudge_big |
| `Put` | 0.54 ms | 14 ms | put_scaling / nudge_big |
| `FindSpot` per trial | 15.3 ms subgrid | 479 ms full | end_to_end / findspot_big |
| `nudge.narrowphase` per iter | 0.48 ms empty, 3.99 ms at 944 fill | 5.08 ms | nudge_small / nudge_partial |
| `nudge.sample_gen`, first time per kind | 16 ms | 711 ms | nudge_small / nudge_big |

## Phase 5: the fixes the numbers now justify

Measurement is done, so this is where the remaining work is. Reordered against
the target scale above -- the original priority (cache the background FFT) is
now third, because two blockers outrank it.

### 5.1 DONE -- the subgrid cell cursor capped small placements at ~900

Not a performance problem -- the target is unreachable as written.

`item.nextCellIdx` (`MeshInfo.h:26`) is incremented at `PackingDriver.cpp:143`
before the trial loop, so it advances on **success as well as failure**, and
nothing ever resets it or `noMoreFit`. Each kind therefore gets at most one
placement per subgrid cell for the entire run, then `noMoreFit = true`.

- ceiling = 90 cells x 10 small kinds = **900 blueberries**, against a target of
  6000+. Short by ~6.7x.
- adding kinds cannot close the gap: kinds are capped at ~100 total, and only
  ~10 of those are blueberry-sized.
- a 20 cm cell can obviously hold hundreds of 1 cm berries, so the cap is pure
  bookkeeping, not geometry.

- [x] on a successful placement, do not advance the cursor -- retry the same
  cell until it fails all `maxTrialCount` trials, and only then move on.
  `item.nextCellIdx++` moved out of the loop head to the failure path only.
- [x] reset `nextCellIdx` and `noMoreFit` at the start of each plan step, since a
  later step changes occupancy and force direction, so "no more fit" from an
  earlier step is not evidence about this one.
- [x] add a bench scenario that runs the small plan step through `PackStep` and
  reports the count. New `smallfruit_packstep`. The existing `smallfruit_batch`
  bypasses `PackStep` and so would not have caught it.
  - verified: cell cursors advanced 32 for 57 placements, i.e. cells are now
    reused. Before the fix cursorSum would have been >= placement count. The
    scenario reports this directly so the cap cannot regress silently.
- [ ] the blueberries only need to cover the container *surface*, so consider
  restricting the cell walk to boundary cells rather than all 90; interior cells
  are wasted searches. Deferred: not the binding constraint, see below.

Important follow-up from the verification run: **the cap was never what stopped
the run at current speed.** At 618 ms per placement, 60 s of budget buys 107
blueberries, so the 900 ceiling was still far away. Lifting it was necessary but
not sufficient -- 5.2 and 5.3 are what actually decide feasibility.

### 5.2 DONE -- instance-keyed caches reached ~6 GB at target scale

`instanceMeshCache` and `instanceGrids` (`PackingScene.h:108-109`) are keyed by
instance index, held a world-space transformed mesh plus a `TrigGrid` per
instance, and were never evicted. `smallfruit_packstep` measured the growth
directly, and it was **worse than the earlier extrapolation**. Packing
blueberries into the 944-item container:

| cached entries | mesh cache | grids | total | per entry |
| --- | --- | --- | --- | --- |
| 103 | 84.02 MB | 20.48 MB | 104.5 MB | 1.01 MB |
| 174 | 133.58 MB | 31.77 MB | 165.3 MB | 0.95 MB |

~1 MB per cached instance, not the 215 KB a blueberry alone would suggest --
because the *neighbors* of a blueberry are mostly big fruits, and it is the
neighbor that gets cached. So:

- 6200 instances x ~1 MB = **~6 GB**, against the earlier 1.9 GB estimate.
- 165 MB of cache after only 107 placements makes this the fastest-growing
  number in the whole run.
- plus item.meshes 129 MB, bg.vox 21.7 MB, trajectories ~10 MB, broadPhase
  ~15 MB.
- this will exhaust RAM long before the run finishes.

This is entirely redundant: the same ~10 blueberry meshes get stored 6000 times.

- [x] key the grid cache by **kind** instead of instance, storing the `TrigGrid`
  in the item's local frame. New `GridInstance` in `TrigGrid.h` pairs a shared
  grid with the instance transform; the query side transforms the sample point
  into grid-local space, and maps `dist`/`closestPt`/`normal` back out.
  Handles the uniform `RigidTransform::scale` by scaling the query radius and
  the returned distance, so a scaled pack file still works.
- [x] **`instanceMeshCache` deleted outright.** `TrigGrid` holds a non-owning
  mesh pointer, so a kind grid can point straight at `items[i].mesh`, which is
  assigned once at load. The per-instance transformed mesh copy -- 84 of the
  104 MB measured above -- has no reason to exist.
- [x] applied the same fix to `NudgeConstrained`, which had its own
  function-local copy of the pattern.
- [x] `MemStats` / `BenchReport` / `cache_growth` now report one `kindGrids` row.

Measured before and after, same scenarios and same seed:

| scenario | before (mesh + grids) | after (kindGrids) | factor |
| --- | --- | --- | --- |
| nudge_big | 5.65 MB, 2 entries | 1.56 MB, 2 | 3.6x |
| nudge_partial | 9.37 MB, 13 entries | 1.66 MB, 12 | 5.6x |
| cache_growth | 15.17 MB, 17 entries | 2.18 MB, 13 | 7.0x |
| smallfruit_packstep | 104.5 MB, 103 entries | 13.09 MB, 42 | 8.0x |

- entry count is now bounded by the catalogue: worst case ~95 kinds at ~312 KB
  = **~30 MB**, against the ~6 GB projection. The OOM risk is gone, and the
  "bound the caches" item is now moot rather than pending.
- it also **sped up narrowphase**, as hoped, by removing cache pressure:
  `nudge_partial` 3.99 -> 2.97 ms/iter (25% faster), and
  `smallfruit_packstep` 17.46 -> 14.66 ms/iter, i.e. 618 -> 511 ms per
  blueberry placement (17% faster).
- correctness unchanged: `--validate` reports the same worst outside / worst
  overlap as before on nudge_big (0.3/0.3), nudge_small (0.6/0.3) and
  nudge_partial (0.3/0.3), `smallfruit_batch` 0 of 24 failed, and
  `validate_selftest` still 5 of 5.

### 5.3 DONE -- narrowphase was 85% of a berry placement, now 26%

Now measured, and it is the bottleneck by a wide margin. `smallfruit_packstep`
packing blueberries into the 944-item container:

- `nudge.narrowphase` **17.5 ms/iter**, 84.7% of the scenario. The earlier
  estimate guessed 8-16 ms/iter at 6200 instances; it is already 17.5 ms at
  ~1000 instances, so the projection was optimistic.
- ~30 iterations per nudge -> `nudge.total` 497 ms mean.
- **618 ms per blueberry placement** end to end, of which searching is only
  10.5% and `put` 0.3%.
- 6000 blueberries x 0.62 s = **~62 min for the blueberries alone**, and the
  per-iteration cost keeps climbing as the container fills.

Everything else in the small-item path is already cheap: `broadphase` 0.003 ms,
`pgs_solve` 0.002 ms, `put` 1.9 ms, `sample_gen` 31 ms once per kind.

- [x] split `nudge.manifold_reduce` out of `nudge.narrowphase`. It is **0.002
  ms/call, 0.1%** -- not the problem at all. All of narrowphase is the grid
  queries, now counted separately as `nudge.grid_query`.
- [x] add pure tally counters (`Profiler::AddCount` / `PROFILE_COUNT`, no
  timing) so the cost could be divided into its parts. What they showed, per
  `GatherActiveContacts` call: 106 sample points x 11.5 neighbor grids, of
  which 346 pairs pass `InRange` and reach `TrigGrid::NearestTriangle`, at
  **39 us per query**. Per query: 5.6 non-empty cells and **1529 triangle
  tests**, i.e. 28 ns per `closestPointTriangle`. The cost was never the
  algorithm around the query, it was the number of triangles the query reads.
- [x] **exact cell-distance culling in `TrigGrid::NearestTriangle`.** The
  search walked a fixed 3x3x3 window regardless of `maxDist`. For a berry
  `maxDist` is `eps + activeBuffer` = 0.22 cm while `voxelSize` is `gridDx` =
  1.0 cm, so it scanned a 3 cm cube to answer a 0.22 cm question. Now each
  cell's box distance to the query point is compared against the best distance
  so far and the cell is skipped if it cannot win, with the centre cell visited
  first so `minDist` shrinks before the neighbours are tested.
  - exact, not an approximation: a triangle whose nearest point lies in a
    culled cell is farther than `minDist` by construction, and the voxelizer
    registers a triangle in every cell it overlaps, so a triangle reaching into
    a kept cell is still reachable through that cell.
  - result: cells per query 5.6 -> 0.55, triangle tests per query 1529 -> 247,
    `nudge.narrowphase` **14.43 -> 2.40 ms/iter (6.0x)**, and
    **507.9 -> 120.5 ms per blueberry placement (4.2x)**.
- [x] fixed a real bug found while reading that loop: `info.closestPt` was
  assigned for *every* triangle tested, not only when it beat `minDist`, so the
  returned point came from the last triangle scanned while the distance and
  normal came from the nearest one. `GatherActiveContacts` takes the sign of
  the contact from `worldPt - info.closestPt`, so a mismatched pair could flip
  a contact's sign. `nudge_small`'s worst outside depth improved from 0.6 to
  0.3 cm after the fix.
- [ ] `nudge.triggrid_build` fires 73 times for 500 placements, i.e. once per
  kind pair met, no longer per placement. Closed by 5.2.
- [x] **narrow phase cell size cut from 1.0 to 0.5 cm** (`gridDx`). After
  culling, a query still tested 247 triangles in ~0.55 occupied cells, i.e. one
  occupied 1 cm cell holds ~450 triangles, because the fruit meshes carry a few
  hundred triangles per square cm. Triangles per cell falls with the square of
  the cell size, so this is the remaining lever.
  - prerequisite: the fixed 3x3x3 window assumed `maxDist <= voxelSize`. At
    `gridDx` 0.5 a big fruit's query radius is 0.55 cm, so the window is now
    sized `ceil(maxDist / voxelSize)` and walked centre-outward via
    `CenterOutOffset`. Verified a no-op at `gridDx` 1.0 (2.427 -> 2.424
    ms/iter) before relying on it. Without this, 0.5 would have silently
    missed contacts on the big items.
  - measured sweep on the 6k berry case, 40 s budget each:

| `gridDx` | ms per placement | narrowphase ms/iter | trig tests/query | container.grids | kindGrids | peak RSS |
| --- | --- | --- | --- | --- | --- | --- |
| 1.0 | 123.9 | 2.43 | 245 | 7.5 MB | 22.7 MB | 243 MB |
| **0.5** | **66.9** | **0.67** | **58** | 42.2 MB | 42.7 MB | 303 MB |
| 0.25 | 57.3 | 0.32 | 20 | 283.8 MB | 119.0 MB | 644 MB |

  - 0.5 is the knee: 1.85x faster for +60 MB. 0.25 buys a further 14% for 2x
    the memory and pushes the container grids alone to 284 MB, so it is not
    worth it. `gridDx` default changed to 0.5 in `PackingConfig.h`, and it is
    now dumped in the config line and settable with `--griddx` so the sweep can
    be repeated.
- [ ] the iteration count (~30 per nudge) is untouched. Deprioritized: at 0.5
  narrowphase is down to **26.4%** of the scenario, so halving iterations now
  saves ~13% rather than ~40%.

Suite-wide effect of 5.3, full 15-scenario run, all validations passing:

| scenario | before | after |
| --- | --- | --- |
| `nudge_big` | 5.2 s | 4.05 s |
| `nudge_small` | 1.0 s | 0.086 s |
| `nudge_partial` | 0.53 s | 0.12 s |
| `smallfruit_batch` | 1.6 s | 0.69 s |
| peak RSS, whole suite | 646 MB | 684 MB |

`end_to_end` is unchanged at 0 placements from 6060 searches in 90 s, as
expected: that scenario never reaches the narrow phase, which is what makes it
5.4's case rather than 5.3's.

Verification of the culling change (required before believing any speedup):

| check | before | after |
| --- | --- | --- |
| `smallfruit_packstep` new placements validated | not checked | **500, 0 failures** |
| worst overlap / mean overlap | -- | 0.6 cm / **0.32 cm** |
| worst outside container | -- | 0.3 cm (1 voxel) |
| same at `gridDx` 0.5, 905 placements | -- | **0 failures**, worst overlap 0.6, mean **0.318** |
| `nudge_big` worst outside / overlap | 0.3 / 0.3 | 0.3 / 0.3 |
| `nudge_small` worst outside / overlap | 0.6 / 0.3 | **0.3 / 0.3** |
| `nudge_partial` worst outside / overlap | 0.3 / 0.3 | 0.3 / 0.3 |
| `smallfruit_batch` failures | 0 of 24 | 0 of 24 |
| `validate_selftest` | 5 of 5 | 5 of 5 |

`Nudge` deliberately allows `MAX_OVERLAP` 0.2 cm and the validator's measure
stacks voxelization rounding on top, so a 0.32 cm mean is at the intended
tolerance, not above it. `smallfruit_packstep` now re-validates the whole batch
in placement order under `--validate` and writes
`smallfruit_packstep_pack.txt` so the pack can be looked at rather than only
counted.

Spatial plausibility of the 497-berry batch, which the overlap numbers alone do
not establish:
- x spread -255 to -75 cm, i.e. the full 180 cm container length, not a pile.
- radial distance from the fruit-mass axis: berries median 19.3 cm, max 26.6,
  against big fruits median 16.4, max 28.8. The berries sit **outward** of the
  big fruits, which is what "cover the surface" requires.
- all 10 small kinds used, 49-50 placements each, so the kind round-robin is
  even.
- unchanged at `gridDx` 0.5 with 905 berries: same -255..-75 cm x span, mean
  radius 19.0, all 10 kinds. The only number that moved is worst outside depth,
  0.3 -> 0.6 cm, i.e. 2 validation voxels against a 1.2 cm allowance, with 0
  failures. That is a sample-size effect, not a loss of accuracy: the finer
  grid is if anything more exact, and it simply placed 905 berries instead of
  500 in the same 60 s, so it had more chances to produce the outlier.
  `smallfruit_batch` at `gridDx` 0.5 still reports worst outside 0.3 cm.

### 5.4 NOW THE TOP ITEM: the subgrid search pipeline

Promoted from third to first by 5.3. With narrowphase at 26% the spot search is
the majority of a berry placement: `findspot_subgrid.total` is **9.75 ms x 4.7
searches per placement = 46 ms of the 67**, and the searches that fail pay in
full for nothing.

Per search, measured at 500 placements: `ifft` 2.03, `bg_fft` 1.79,
`fg_voxelize` 1.73, `fg_fft` 1.64, `quantize` 0.72, `crop` 0.57,
`score_scan` 0.34.

At 6000 berries and 4.7 searches each that is ~275 s, and the same pipeline
serves the medium items.

Also still the `end_to_end` failure mode: 5890 searches and 90 s for 0
placements in a saturated container.

Measured on a real `./cpu_pack` run of step 3 (the 11 medium-small kinds:
kiwi, plums, strawberries, figs) resuming from the 944-item pack:

- the step takes **187 s and places 35 items**: 5.35 s and **287 searches per
  placement**.
- 9 of the 11 kinds retire having placed nothing at all, each after walking all
  90 cells at 10 trials, i.e. **900 wasted searches per retired kind**.
- the placements all arrive at the end, once the walk reaches the cells that
  still have room, so `end_to_end`'s 90 s window reporting 0 placements was
  measuring the wasted half of the step rather than a truly saturated
  container. Worth widening that scenario's cap.
- this makes the cheap occupancy pre-check below the highest-value item of the
  three: it is the one that kills the 900-search retirement walk.

- [ ] cache `fg_voxelize` + `fg_fft` per (kind, rotation) -- 3.4 of the 9.75 ms,
  and they do not depend on container state at all. With ~10 small kinds x 10
  rotations this is a ~100-entry table with a near-100% hit rate. Same
  kind-keying argument as 5.2, so do this first.
- [ ] cache `bg_fft` per subgrid cell, invalidated only by a `Put` touching that
  cell -- another 1.79 ms. This is the original plan's top fix.
- [ ] cheap occupancy pre-check before any FFT, so a saturated cell is rejected
  without running the pipeline. Promoted to first by the step 3 measurement
  above: 9 of 11 kinds spend 900 searches each proving there is no room.

Progress logging added while diagnosing this (`PackingDriver.cpp`, all at
`LOGI` so benchmarks stay silent): a step banner with the kind list, a 2 s
heartbeat printing elapsed / searches / placed / kinds retired / cell slots
walked / instance count, a line per placement including how far the nudge moved
the item from the found spot, a line per retired kind, and a step summary with
the exit reason and searches per placement. Without these a step that places
nothing for three minutes is indistinguishable from a hang. Also removed a
duplicated `noMoreFit` assignment in the subgrid path that the retirement test
below it already covered.

### 5.5 Periodic saves are O(n^2) and 40x worse at 6200 instances

`trajSaveInterval` 10 and `packSaveInterval` 20 each rewrite the whole
accumulated state (`PackingDriver.cpp`, `SaveTrajectories` / `SaveInstances`).
The plan already flagged this for benchmarks; at target scale it is a production
problem too.

- trajectories reach ~10 MB (6000 x 1.6 KB + 200 x 5.1 KB), rewritten every 10
  placements -> ~620 writes averaging ~5 MB = **~3 GB of I/O**.
- instances: ~310 writes averaging ~3100 lines = ~960 K lines.
- [ ] make both append-incremental, or raise the intervals sharply at high
  instance counts. Do not simply disable them -- a multi-hour run needs restart
  points.

### 5.6 Big and medium-big items: secondary

Only the 17 large kinds (>20 cm) exceed `subgridCellSize`, so only they take the
479 ms full-container `FindSpot` path; the 57 medium-large kinds that supply most
of the ~200 target already use the 15.3 ms subgrid path.

- large: ~10 trials x ~50 placements x 479 ms = ~240 s, of which `bg_fft` 79 ms
  per call is recomputed identically every time (~40 s recoverable by 5.4).
- settle: `nudge_big` 517 ms excluding sample_gen, growing with fill -> 200 x
  ~2 s = ~400 s.
- [ ] `nudge.sample_gen` is 711 ms per big kind but first-time-only per kind, so
  74 large/medium-large kinds = ~53 s one-time. Precompute or cache to disk only
  if the ~10-15 min big-item total becomes the limiting factor. Low priority.

### 5.7 Confirmed non-bottlenecks -- do not spend time here

Measured cheap, and cheap at target scale:

- `put.total` flat 0.54 ms with no growth in instance count: 6200 Puts = ~3.3 s.
- `nudge.pgs_solve` 0.001 ms/call. Free.
- `nudge.broadphase` (GetNearby) 0.001 ms/call. Free.
- one-time setup: `load_meshes` 1.37 s, `container_setup` 2.1 s.
- superseded by 5.2: "bound `instanceGrids` / `instanceMeshCache`". The caches
  are now O(kinds), bounded by construction, and the mesh cache is gone.

### Revised total estimate

With 5.1, 5.2 and 5.3 done, measured at **66.6 ms per blueberry placement** and
905 berries in a 60 s step:

| item | at plan start | after 5.2 | now (5.3 done) | after 5.4 |
| --- | --- | --- | --- | --- |
| blueberry nudge | ~62 min | ~43 min | **~3 min** | ~3 min |
| blueberry searches | ~7 min | ~7 min | **~5 min** | ~2 min |
| big/medium-big total | ~10-15 min | ~10-15 min | ~5-8 min | ~4-6 min |
| peak memory | ~6 GB, would OOM | ~30 MB | ~85 MB of grids, 356 MB RSS | done |

So the run went from **~80 min and certain OOM** to **roughly 10 min and
RAM-safe**. Measured per-placement cost fell 618 -> 511 -> 124 -> 66.6 ms, a
**9.3x** end-to-end improvement on the berry case:

| step | ms per placement | what moved |
| --- | --- | --- |
| plan start | 618 | -- |
| 5.2 kind-keyed grids | 511 | cache pressure |
| 5.3 cell-distance culling | 124 | 1529 -> 247 triangle tests per query |
| 5.3 `gridDx` 1.0 -> 0.5 | 66.6 | 247 -> 58 triangle tests per query |

The 6000-berry target now looks reachable in single-digit minutes rather than
being infeasible, and 5.1's placement ceiling of 900 is finally the binding
constraint again rather than time: a 60 s step already reaches 905. **That
ceiling was lifted in 5.1, but the "restrict the cell walk to boundary cells"
item under 5.1 is now worth revisiting**, since at 905 berries per minute the
question shifts from speed to whether the cells being walked are the useful
ones.

Remaining, in order: 5.4 (the search pipeline, now ~69% of a placement), then
5.5 (saves, untouched and now relatively more expensive since placements arrive
7x faster), then 5.6.

### Code changed in phase 5

5.1, cell cursor:
- `PackingDriver.cpp` -- `item.nextCellIdx++` moved out of the loop head into the
  all-trials-failed path, so a success keeps the cell. Added a reset of
  `noMoreFit` and `nextCellIdx` for every item at the top of `PackStep`.
- `BenchGrowth.cpp` / `BenchRegistry.cpp` -- new `smallfruit_packstep` scenario.
  It prints the cursor sum next to the placement count and states outright
  whether the cap is lifted, so the regression cannot come back silently.

5.2, kind-keyed grids:
- `TrigGrid.h` -- new `GridInstance` (declared after `TrigGrid`, it needs the
  complete type): shared grid pointer plus rot/rotInv/pos/scale/invScale, a
  `worldSpace` flag for the container grids, and `ToLocal` / `ToWorldPoint` /
  `ToWorldDir`. `rotInv` and `invScale` are precomputed so a query costs one
  3x3 multiply and a subtract.
- `PackingScene.h` -- `instanceMeshCache` and `instanceGrids` deleted, replaced
  by `std::unordered_map<unsigned, std::shared_ptr<TrigGrid>> kindGrids`.
- `PackingScene.cpp` -- `Nudge` builds kind grids straight from
  `items[inst.itemId].mesh` with no transformed copy; `GatherActiveContacts`
  takes `const std::vector<GridInstance>&` and maps the query point in and
  `dist`/`closestPt`/`normal` back out; `NudgeConstrained` had its own private
  copy of the whole pattern and now shares `kindGrids` too.
- `MemStats.h/.cpp`, `BenchReport.cpp`, `BenchGrowth.cpp` -- the two cache rows
  and the two growth-table columns collapsed into one `kindGrids`.

Reproducing the numbers: `cpu_pack_bench --only smallfruit_packstep` for the
blueberry feasibility figures, `--only cache_growth` for the memory curve, and
add `--validate` to confirm placements did not change.

5.3, narrow phase:
- `Profiler.h/.cpp` -- `AddCount` / `PROFILE_COUNT`, tally-only counters with no
  timer, so per-query event counts could be divided out of the timings. These
  are what identified the triangle count as the cost.
- `PackingScene.cpp` -- `nudge.grid_query` and `nudge.manifold_reduce` split out
  of `nudge.narrowphase`, plus `count.gather_samples`, `count.gather_grids`,
  `count.nearest_trig`, `count.raw_contacts`.
- `TrigGrid.cpp` -- `AxisGap` + per-cell box-distance culling in
  `NearestTriangle`; search window sized from `maxDist` instead of a fixed
  3x3x3 and walked centre-outward via `CenterOutOffset`; `info.closestPt`
  assignment moved inside the `dist < minDist` test (the bug above);
  `count.grid_cells_scanned` and `count.grid_trig_tests`.
- `PackingConfig.h/.cpp` -- `gridDx` default 1.0 -> 0.5, and added to the config
  dump.
- `benchmarks.cpp` -- `--griddx` so the cell-size sweep does not need a rebuild.
- `BenchGrowth.cpp` -- `smallfruit_packstep` now revalidates the whole batch in
  placement order under `--validate` and writes
  `smallfruit_packstep_pack.txt`, since a speedup in the narrow phase has to be
  paid for in overlap and the 3-placement nudge scenarios are too small to show
  it.

### Run configuration and progress output (out of band, done)

A full run is driven by `pack_fruits.cfg` in the code folder, `./cpu_pack
../cpu_pack/pack_fruits.cfg`. `key value` lines, `#` comments, value is the rest
of the line so paths may hold spaces, unknown keys reported and skipped so an
old file still runs. `argv[1]` is a config when it names a file and `dataDir`
when it names a directory, `--config <file>` also works.

`startStep` and `startItem` are clamped to the nearest valid index instead of
being trusted: `PackingConfig::ClampStartStep` runs in `PackFruits`, the first
point where the plan length is known, and `PackStep` clamps `startItem` itself
because the valid range is the kind count of whichever step is running. An
unclamped `startItem` was silently fatal, not out of range -- the item loop body
never ran, so the step spun through all `step.count` rounds placing nothing.

`PackStep` also had no output except from a successful placement, so a saturated
container looked exactly like a hang. Now: a step banner with the kind count,
target and force, a 2 s heartbeat with searches / placed / kinds retired / cell
slots walked, a per-placement line with the settle distance and ms, a retirement
line per kind, and a step exit reason with ms and searches per placement. Also
removed a duplicate `noMoreFit` block in the subgrid path.

### Next step (5.4)

Cache `fg_voxelize` + `fg_fft` per (kind, rotation) first: 3.4 of the 9.75 ms
per search, no dependence on container state, and ~100 entries at ~100% hit
rate. Then `bg_fft` per subgrid cell with `Put`-based invalidation for another
1.79 ms. Validate the same way 5.3 was validated -- `smallfruit_packstep
--validate` for the overlap and spread numbers, not just the timings, because a
stale cache would show up as bad placements rather than as a crash.
