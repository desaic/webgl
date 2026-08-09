#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

class PackingScene;

/// process memory, read from /proc/self/status on linux.
/// returns 0 if unavailable.
size_t RssBytes();
/// high water mark (VmHWM), or getrusage ru_maxrss as a fallback.
size_t PeakRssBytes();

std::string FormatBytes(size_t bytes);

/// logical byte accounting per structure. RSS alone does not say which
/// structure grew, and the per-instance caches are the suspected leak.
struct SceneMemory {
    size_t bgVox = 0;
    size_t bgFft = 0;
    size_t containerSdf = 0;
    size_t containerGrids = 0;
    size_t itemMeshes = 0;
    size_t itemSdfs = 0;
    size_t itemSamples = 0;
    // narrow phase TrigGrids, one per item kind, so bounded by the
    // catalogue rather than by the placement count.
    size_t kindGrids = 0;
    unsigned kindGridsCount = 0;
    // grows with every placement.
    size_t trajectories = 0;
    size_t broadPhase = 0;

    size_t Total() const;
    std::string toString() const;
};

SceneMemory MeasureSceneMemory(const PackingScene &scene);

void ReportMemory(std::ostream &out, const std::string &title,
                  const PackingScene &scene);
