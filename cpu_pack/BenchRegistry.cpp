#include "Benchmark.h"

void BenchLoadMeshes(BenchContext &ctx);
void BenchContainerSetup(BenchContext &ctx);
void BenchLoadPack(BenchContext &ctx);
void BenchFindSpotBig(BenchContext &ctx);
void BenchFindSpotSmall(BenchContext &ctx);
void BenchFindSpotPartial(BenchContext &ctx);
void BenchNudgeBig(BenchContext &ctx);
void BenchNudgeSmall(BenchContext &ctx);
void BenchNudgePartial(BenchContext &ctx);
void BenchCacheGrowth(BenchContext &ctx);
void BenchPutScaling(BenchContext &ctx);
void BenchEndToEnd(BenchContext &ctx);
void BenchValidateSelfTest(BenchContext &ctx);
void BenchSmallFruitBatch(BenchContext &ctx);
void BenchSmallFruitPackStep(BenchContext &ctx);
void BenchCoverageBaseline(BenchContext &ctx);
void BenchPocketFill(BenchContext &ctx);

const std::vector<Bench> &AllBenches() {
  static const std::vector<Bench> benches = {
      {"load_meshes", "read every item mesh from disk", BenchLoadMeshes},
      {"container_setup", "container sdf, TrigGrids and bg occupancy grid",
       BenchContainerSetup},
      {"load_pack", "replay a saved pack file, one Put per line",
       BenchLoadPack},
      {"findspot_big", "full container FFT search for the largest items",
       BenchFindSpotBig},
      {"findspot_small", "cropped subgrid FFT search for the smallest items",
       BenchFindSpotSmall},
      {"findspot_partial", "subgrid search against a partly full container",
       BenchFindSpotPartial},
      {"nudge_big", "settle large items in an empty container", BenchNudgeBig},
      {"nudge_small", "settle small items in an empty container",
       BenchNudgeSmall},
      {"nudge_partial", "settle small items with real neighbors present",
       BenchNudgePartial},
      {"smallfruit_batch",
       "bulk placement of the smallest berry, with the FFT shrink reported",
       BenchSmallFruitBatch},
      {"smallfruit_packstep",
       "bulk small placement through the real PackStep control flow",
       BenchSmallFruitPackStep},
      {"cache_growth", "per placement memory growth of the instance caches",
       BenchCacheGrowth},
      {"put_scaling", "Put cost as instance count rises", BenchPutScaling},
      {"end_to_end", "one plan step through the real PackStep control flow",
       BenchEndToEnd},
      {"validate_selftest",
       "confirm the validator flags known bad placements",
       BenchValidateSelfTest},
      {"coverage_baseline",
       "H1: surface coverage metric on a loaded pack, no placement changes",
       BenchCoverageBaseline},
      {"pocket_fill",
       "H2/H3: pocket-driven small-item placement, coverage delta vs. before",
       BenchPocketFill},
  };
  return benches;
}
