#pragma once

#include "PackingConfig.h"
#include "PackingPlan.h"

#include <iosfwd>
#include <string>
#include <vector>

/// shared state handed to every scenario. scenarios that mutate a scene
/// build their own, so nothing here is scene state.
struct BenchContext {
    PackingConfig cfg;
    PackingPlan plan;
    // run PackValidator on each placement. off by default since it
    // allocates a second container grid and resamples every item.
    bool validate = false;
    // how many items / placements a scenario should exercise. small on
    // purpose: the container holds a few hundred fruits and the item
    // catalogue stays near 100, so per placement cost and growth trend
    // are what matter, not large-N throughput.
    unsigned itemCount = 3;
    // csv sink, null when not requested.
    std::ostream *csv = nullptr;

    // total wall budget for the whole run, in seconds. scenarios are
    // skipped once it is gone, so a full run stays predictable.
    float budgetSec = 300.0f;
    // seconds already consumed, updated by the driver between scenarios.
    float spentSec = 0.0f;

    float RemainingSec() const {
      float left = budgetSec - spentSec;
      return left > 0.0f ? left : 0.0f;
    }
};

typedef void (*BenchFn)(BenchContext &);

struct Bench {
    std::string name;
    std::string desc;
    BenchFn fn;
};

/// all scenarios in run order.
const std::vector<Bench> &AllBenches();
