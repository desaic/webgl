# Benchmark + reorg plan (cpu_pack only)

Goal: a `cpu_pack_bench` target that measures CPU time and RAM for the parts
that actually cost time (FFT, narrow phase, TrigGrid build, Put), on real
meshes from `/media/desaic/WD/meshes/fruit_hand/`.

Reorg is a prerequisite: today the packing driver lives in `main.cpp` with
hardcoded paths and debug constants, so a benchmark cannot call it without
duplicating it. Timing is also scattered as local Stopwatch + cout.

Confirmed available data:
- `fruits_1/` 98 meshes + `stats.txt`
- `hands/finger4.8m.stl`, `hands/finger_4.8m_inner.stl`
- `pack_944_0721.txt` (~944 placements) for the partially-filled case

## Phase 0: instrumentation (additive, no behavior change)

New `Profiler.h/.cpp`:
- Named accumulating counters: total ms, call count, min/max.
- RAII `ProfileScope` + `PROFILE_SCOPE("name")` macro.
- `Profiler::Reset()`, `Profiler::Report(ostream&)`, `Profiler::ReportCsv()`.
- Single-thread accumulation only (pocketfft/FastSweepPar spawn their own
  threads but never touch the profiler). Document that.
- Compile-time off switch so main.cpp release runs pay nothing.

New `MemStats.h/.cpp`:
- `RssBytes()`, `PeakRssBytes()` from `/proc/self/status` (VmRSS/VmHWM) with
  `getrusage` fallback.
- `SceneMemoryReport(const PackingScene&)`: logical byte accounting, since RSS
  alone will not say which structure grew. Sums:
  - `bg.vox`, `bg.fft`
  - `sdf->dist` (short, x2 bytes), `sdf->fineGrid`
  - `containerGrid`, `containerInnerGrid`
  - `instanceMeshCache` (mesh v/t/n bytes)
  - `instanceGrids` (grid data + tLists incl. per-vector header overhead)
  - `instances[].trajectory` (grows with every placement)
- Needs `size_t MemoryBytes() const` on `TrigGrid` (members are private) and on
  `MeshConvo`. Both live in cpu_pack, so no parent-folder edits. AdapSDF memory
  is derived from its public `dist`/`fineGrid` members, so `../SparseSDF` stays
  untouched.

### Counters to add

`FindSpot` / `ComputeCollisionGrid` (PackingOps.cpp). Note the existing
`fftTimer` there is dead code: it is `Start()`ed 4 times and never read.
Replace it with real scopes:
- `findspot.fg_voxelize`, `findspot.bg_fft`, `findspot.fg_fft`,
  `findspot.dot`, `findspot.ifft`, `findspot.quantize`, `findspot.score_scan`
  (the triple loop calling `sdf->GetCoarseDist` per free voxel)
- `findspot.subgrid_crop` (the manual voxel copy loop in FindSpotSubgrid)

`Nudge` (PackingScene.cpp) -- replace the local broadTime/gatherTime/pgsTime
with counters so they survive across calls:
- `nudge.sample_gen` (first-time SamplePoints + DownsamplePoints + item SDF)
- `nudge.broadphase` (GetNearby only)
- `nudge.triggrid_build` (split out from broadphase; this is the expensive,
  cache-populating part and must be measured separately)
- `nudge.narrowphase` (GatherActiveContacts)
- `nudge.manifold_reduce` (reduceContactManifold, currently inside gather)
- `nudge.pgs`

`Put`: `put.transform_mesh`, `put.voxelize`, `put.flood`, `put.union`.

Init: `init.load_meshes`, `init.container_sdf`, `init.container_grid`,
`init.inertia_frame`.

## Phase 1: config + verbosity

New `PackingConfig.h/.cpp`. Pulls the hardcoded values out of main.cpp so a
benchmark can build a scenario without editing production code:
- `dataDir` (default `/media/desaic/WD/meshes/fruit_hand/`, overridable by
  argv[1] or `FRUIT_HAND_DIR` env var so the laptop/Windows paths stop being
  commented-out lines)
- `containerFile`, `innerContainerFile`, `fruitSubdir`, `outputFolder`
- `dx` (0.3), `containerSDFDx` (2.0), `broadPhaseDx` (2.0), `subgridCellSize`
- `maxTrialCount` (was `MAX_TRIAL_COUNT` 10), `startStep` (was `DEBUG_STEP` 3),
  `startItem` (was `DEBUG_ITEM` 0)
- `resumePackFile` (was the hardcoded `LoadPack` path) + `bool resume`
- `trajSaveInterval` (10), `packSaveInterval` (20); set to 0 to disable disk
  writes. Required for clean benchmarks -- those periodic saves rewrite the
  whole accumulated state each time, so they are both slow and O(n^2) in I/O.

New `Log.h`: `SetVerbosity(int)` + `LOG_INFO`/`LOG_DEBUG` guards around the
per-placement cout in `placeItem`, the per-call `Nudge` summary line, and
`LoadPack`'s per-item lines. Loading 944 placements currently prints 944 lines;
that I/O alone distorts any measurement of the partially-filled scenario.

## Phase 2: extract the driver from main.cpp

Move, no logic change:
- `PackStep`, `PackScene`, `PackFruits` -> new `PackingDriver.h/.cpp`, taking
  `const PackingConfig&` instead of reading globals/constants.
- `MeshStat`, `ComputeMeshStats`, `GetGroupIndex`, `PlanPackingSteps` ->
  `PackingPlan.cpp` (already the natural home).
- `MakeInnerMesh`, `DebugPointSampling`, `DebugNudge` -> new `DebugTools.h/.cpp`.
- `main.cpp` shrinks to: parse config, optionally ComputeMeshStats, plan, run.

Also fix while moving (cheap, unblocks measurement):
- Delete the dead locals in `PackScene`: `NUM_COPIES`, `outputScale`,
  `ANGLE_TRIALS`, `dxVec`, `voxRes`, `origin`.
- `PlanPackingSteps` currently does an O(groups * names * stats) linear name
  lookup to re-find each mesh's extent it already computed; build one
  `name -> len` map instead. Not hot, but it is noise in `init.*`.

## Phase 3: CMake split

Current `FILE(GLOB EXE_SRC "*.cpp" ...)` globs every cpp into one executable,
so adding `benchmarks.cpp` would produce two `main()` symbols. Restructure:

- `pack_core` static lib: all `*.cpp` in cpu_pack plus `quickhull/*.cpp`,
  minus `main.cpp` and `benchmarks.cpp` (use `list(REMOVE_ITEM ...)` on the
  glob so nothing else needs listing by hand).
- `cpu_pack` exe: `main.cpp` + `pack_core`.
- `cpu_pack_bench` exe: `benchmarks.cpp` + `pack_core`.
- Both link `Mesh MathLib ImageIO SparseSDF Util MeshSimplification`.
- Keep the existing `-O3 -DNDEBUG` release flags; benchmarks are meaningless in
  a Debug build, so print the build type at startup and warn if not Release.

## Phase 4: benchmarks.cpp

Shared harness: each scenario gets a name, an optional warmup run, N repeats,
and reports wall ms (mean/min), `Profiler` counter breakdown, RSS delta, peak
RSS, and the logical `SceneMemoryReport`. Output as an aligned table to stdout
plus `--csv <file>` so numbers can be diffed across commits. Fixed seed is
already in place (`randAngles` seeded 123), so runs are comparable.

Scenarios, cheapest first so a partial run is still useful:

1. `load_meshes` -- load 98 fruits + both containers. Time + RAM baseline.
2. `container_sdf` -- `ComputeSDF` on `finger4.8m.stl` at dx 2.0. One-time cost,
   but it gates every other scenario's setup time.
3. `triggrid_build` -- `TrigGrid::Build` per fruit, bucketed by size. Reports ms
   and bytes per grid. This is the unit cost behind the `instanceGrids` growth.
4. `findspot_big` -- `FindSpot` on the largest fruits in an empty container.
   Worst case for the full-container FFT; expect `findspot.bg_fft` to dominate
   and to be identical across trials, which is the evidence for caching it.
5. `findspot_small` -- `FindSpotSubgrid` on small fruits, cropped FFT. Compare
   `findspot.bg_fft` here vs scenario 4 to quantify what the subgrid crop buys.
6. `findspot_partial` -- scenarios 4 and 5 re-run after `LoadPack` of
   `pack_944_0721.txt`. Tests whether occupancy changes FFT cost (it should not)
   vs `score_scan` cost (it should drop, fewer free voxels).
7. `nudge_big` / `nudge_small` -- `Nudge` on an empty container, broken into
   broad/triggrid/narrow/pgs. Establishes per-item settle cost by size class.
8. `nudge_partial` -- `Nudge` in the 944-item container, where broadphase
   returns many neighbors. Expect `nudge.triggrid_build` to spike on first
   contact with each new neighbor, then fall to zero as the cache fills.
9. `cache_growth` -- run K nudges in the partially-filled container, printing
   `instanceGrids.size()`, its bytes, and RSS every 10 nudges. This is the
   scenario that quantifies the unbounded-cache problem from summary_doc.md.
10. `put_scaling` -- `Put()` cost as a function of instance count, to confirm or
    refute the "revoxelizes because it's not a bottleneck" comment.
11. `end_to_end` -- pack N items (N configurable, default small) from empty with
    disk saves disabled. Totals + peak RSS + full counter breakdown.

Selection by `--only <name>` / `--list`, since `container_sdf` plus a 944-item
`LoadPack` makes the full suite slow to start.

## Order of work

Phase 0 and 1 are additive and independently verifiable (main.cpp output must
be unchanged). Phase 2 is a pure move. Phase 3 and 4 land the target. After
that the numbers justify the fixes in summary_doc.md, in this order:
cache the background FFT, then bound `instanceGrids`/`instanceMeshCache`.
