#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "MeshOps.h"
#include "PackValidate.h"
#include "PackingOps.h"
#include "Profiler.h"

#include <iostream>
#include <sstream>

namespace {

struct PlaceStats {
    unsigned attempted = 0;
    unsigned placed = 0;
    unsigned validated = 0;
    unsigned validationFailures = 0;
    float worstOutside = 0.0f;
    float worstOverlap = 0.0f;
};

// finds a spot, settles the item with Nudge, then Puts it. mirrors the
// placeItem lambda in PackStep minus the periodic file saves.
// validation runs after Nudge and before Put, outside the profiled path
// it measures, so an unvalidated run behaves identically.
void PlaceSweep(BenchScene &bs, const PackingConfig &cfg,
                const std::vector<unsigned> &picks, bool subgrid,
                PackValidator *validator, PlaceStats &st) {
  PackingScene &scene = *bs.scene;
  unsigned totalCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
  unsigned angleIndex = 0;
  for (size_t i = 0; i < picks.size(); i++) {
    MeshInfo &item = scene.items[picks[i]];
    for (unsigned trial = 0; trial < cfg.maxTrialCount; trial++) {
      Vec3f pos;
      Vec3f rot = scene.randAngles[angleIndex];
      angleIndex = (angleIndex + 1) % unsigned(scene.randAngles.size());
      st.attempted++;
      bool ok;
      if (subgrid) {
        ok = FindSpotSubgrid(scene.bg, item.mesh, pos, rot, scene.sdf, 1.0f,
                             scene.subgridCellSize, trial % totalCells,
                             scene.numSubgridCells, false);
      } else {
        ok = FindSpot(scene.bg, item.mesh, pos, rot, scene.sdf, 1.0f);
      }
      if (!ok) {
        continue;
      }
      RigidTransform tran;
      tran.position = pos;
      tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);
      Vec3f pushDir = scene.ForceDirection(picks[i], Vec3f(-1, 0, 0), 1.0f, tran);
      std::vector<RigidTransform> trajectory;
      RigidTransform newTran = scene.Nudge(picks[i], tran, pushDir, trajectory);

      if (validator != nullptr) {
        ValidationResult vr =
            validator->ValidatePlacement(scene, picks[i], newTran);
        st.validated++;
        if (!vr.Ok()) {
          st.validationFailures++;
          std::cout << "  validate " << item.name << " " << vr.toString()
                    << "\n";
        }
        if (vr.maxOutsideDepth > st.worstOutside) {
          st.worstOutside = vr.maxOutsideDepth;
        }
        if (vr.maxOverlapDepth > st.worstOverlap) {
          st.worstOverlap = vr.maxOverlapDepth;
        }
        validator->AddPlaced(scene, picks[i], newTran);
      }

      unsigned instanceId = scene.Put(picks[i], newTran);
      scene.instances[instanceId].trajectory = trajectory;
      st.placed++;
      break;
    }
  }
}

void NoteStats(BenchRegion &r, const PlaceStats &st) {
  std::ostringstream oss;
  oss << st.placed << " placed of " << st.attempted << " attempts";
  r.Note(oss.str());
  if (st.validated > 0) {
    std::ostringstream v;
    v << "validated " << st.validated << ", failures "
      << st.validationFailures << ", worst outside " << st.worstOutside
      << " cm, worst overlap " << st.worstOverlap << " cm";
    r.Note(v.str());
  }
}

// validation is only initialized when asked for, so its second container
// grid never exists in a normal benchmark run.
std::unique_ptr<PackValidator> MakeValidator(const BenchContext &ctx,
                                             const BenchScene &bs,
                                             bool replayExisting) {
  if (!ctx.validate) {
    return nullptr;
  }
  auto v = std::make_unique<PackValidator>();
  v->Init(*bs.scene, ctx.cfg);
  if (replayExisting) {
    v->AddAllPlaced(*bs.scene);
  }
  return v;
}

} // namespace

// settling large items in an empty container. maxOptimizationSteps is 100
// here and every step gathers contacts against the container TrigGrids.
void BenchNudgeBig(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, false);
  if (!bs.ok) {
    return;
  }
  std::vector<unsigned> picks = PickBigItems(bs, ctx.itemCount);
  auto validator = MakeValidator(ctx, bs, false);
  BenchRegion r("nudge_big");
  PlaceStats st;
  PlaceSweep(bs, ctx.cfg, picks, false, validator.get(), st);
  NoteStats(r, st);
  r.Finish(bs.scene.get());
}

// small items settle in 30 steps with coarser sampling, so nudge.total
// per placement should be far below nudge_big.
void BenchNudgeSmall(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, false);
  if (!bs.ok) {
    return;
  }
  std::vector<unsigned> picks = PickSmallItems(bs, ctx.itemCount);
  auto validator = MakeValidator(ctx, bs, false);
  BenchRegion r("nudge_small");
  PlaceStats st;
  PlaceSweep(bs, ctx.cfg, picks, true, validator.get(), st);
  NoteStats(r, st);
  r.Finish(bs.scene.get());
}

// the realistic case: neighbors exist, so broadphase returns real hits and
// each first-time neighbor pays nudge.triggrid_build. that counter is the
// one to watch, since the grids it builds are cached forever.
void BenchNudgePartial(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  std::vector<unsigned> picks = PickSmallItems(bs, ctx.itemCount);
  auto validator = MakeValidator(ctx, bs, true);
  BenchRegion r("nudge_partial");
  std::ostringstream oss;
  oss << bs.scene->instances.size() << " items already placed";
  r.Note(oss.str());
  PlaceStats st;
  PlaceSweep(bs, ctx.cfg, picks, true, validator.get(), st);
  NoteStats(r, st);
  r.Finish(bs.scene.get());
}
