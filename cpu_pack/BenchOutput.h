#pragma once

#include <string>
#include <vector>

/// directory holding the running executable, with a trailing slash.
/// falls back to "./" if it cannot be resolved.
std::string ExeDir();

/// one profiler counter as recorded for a finished scenario.
struct BenchCounter {
    std::string name;
    double totalMs = 0.0;
    unsigned long calls = 0;
    double meanMs = 0.0;
    double maxMs = 0.0;
    double pct = 0.0;
};

/// everything the html summary needs about one scenario.
struct BenchResult {
    std::string name;
    std::string desc;
    double wallMs = 0.0;
    size_t rssBytes = 0;
    size_t peakRssBytes = 0;
    // scenario specific lines already formatted for display.
    std::vector<std::string> notes;
    std::vector<BenchCounter> counters;
    // logical scene memory, empty when the scenario had no scene.
    std::vector<std::pair<std::string, size_t>> sceneMemory;
    bool skipped = false;
    std::string skipReason;
};

/// accumulated across the run, in run order.
std::vector<BenchResult> &BenchResults();

/// writes summary.html next to the executable. returns the path written,
/// or an empty string on failure.
std::string WriteBenchHtml(const std::string &title,
                           const std::string &configText,
                           double totalWallMs);
