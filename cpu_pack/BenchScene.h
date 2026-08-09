#pragma once

#include "Benchmark.h"
#include "PackingScene.h"

#include <memory>
#include <vector>

/// a scene built for one scenario, plus the item picks that scenario needs.
struct BenchScene {
    std::unique_ptr<PackingScene> scene;
    // item indices sorted by bounding box diagonal, descending.
    std::vector<int> bySize;
    bool ok = false;
};

/// BuildScene + PrepareBackground with saves disabled and logging silenced.
/// counted outside any BenchRegion, so setup never pollutes a measurement.
/// loadPack replays cfg.resumePackFile so the container starts partly full.
BenchScene MakeBenchScene(const PackingConfig &cfg, bool loadPack);

/// the itemCount largest items, biggest first.
std::vector<unsigned> PickBigItems(const BenchScene &bs, unsigned n);

/// the itemCount smallest items, smallest first.
std::vector<unsigned> PickSmallItems(const BenchScene &bs, unsigned n);
