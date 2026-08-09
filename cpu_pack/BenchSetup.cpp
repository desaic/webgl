#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "MemStats.h"
#include "MeshInfo.h"
#include "PackingDriver.h"
#include "Profiler.h"

#include <iostream>
#include <sstream>

// how long it takes to read every item mesh off disk, and what they cost
// in RAM once resident. this is paid once but sets the RSS floor.
void BenchLoadMeshes(BenchContext &ctx) {
  BenchRegion r("load_meshes");
  std::vector<MeshInfo> items = LoadAllMeshInfo(ctx.cfg.MeshDir());
  size_t verts = 0, trigs = 0;
  for (size_t i = 0; i < items.size(); i++) {
    verts += items[i].mesh.v.size() / 3;
    trigs += items[i].mesh.t.size() / 3;
  }
  std::ostringstream oss;
  oss << items.size() << " items, " << verts << " verts, " << trigs
      << " trigs";
  r.Note(oss.str());
  r.Finish();
}

// the one time container setup: sdf via FastSweepPar, the two narrow phase
// TrigGrids, and the dx=0.3 occupancy grid. init.* counters split it.
void BenchContainerSetup(BenchContext &ctx) {
  BenchRegion r("container_setup");
  BenchScene bs = MakeBenchScene(ctx.cfg, false);
  if (!bs.ok) {
    return;
  }
  Vec3u g = bs.scene->bg.GridSize();
  std::ostringstream oss;
  oss << "bg grid " << g[0] << "x" << g[1] << "x" << g[2] << " at dx "
      << ctx.cfg.dx;
  r.Note(oss.str());
  r.Finish(bs.scene.get());
}

// resuming a run: parse the pack file and rebuild every instance, which
// calls Put once per line and so re-voxelizes the whole pack.
void BenchLoadPack(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, false);
  if (!bs.ok) {
    return;
  }
  BenchRegion r("load_pack");
  LoadPack(*bs.scene, ctx.cfg.ResumePackPath());
  std::ostringstream oss;
  oss << bs.scene->instances.size() << " instances from "
      << ctx.cfg.ResumePackPath();
  r.Note(oss.str());
  r.Finish(bs.scene.get());
}
