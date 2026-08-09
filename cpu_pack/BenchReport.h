#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

class PackingScene;

/// wall clock + memory delta around a measured region, printed as one block
/// followed by the profiler table for that region only.
///
/// usage:
///   BenchRegion r("findspot_big");
///   ... work ...
///   r.Finish(scene);
class BenchRegion {
  public:
    /// resets the profiler so the reported table covers only this region.
    explicit BenchRegion(const std::string &name);

    /// stops the clock, prints wall time, rss delta, the profiler table and
    /// the scene memory breakdown. scene may be null.
    void Finish(const PackingScene *scene = nullptr);

    /// wall time so far in ms.
    double ElapsedMS() const;

    /// emitted as extra context lines before the tables.
    void Note(const std::string &line);

    /// csv sink shared by all regions. one row per profiler counter,
    /// tagged with the region name.
    static void SetCsv(std::ostream *out);

    /// description recorded alongside the next region, for the html report.
    static void SetCurrentDesc(const std::string &desc);

  private:
    std::string name_;
    double startMs_ = 0.0;
    size_t startRss_ = 0;
    std::string notes_;
    std::vector<std::string> noteList_;
    bool finished_ = false;
};

/// prints a labelled section header.
void BenchHeader(const std::string &name, const std::string &desc);

/// "12 items, 3 placements" style one liner.
void BenchNote(const std::string &line);

/// records a scenario that never ran, so it shows up in the html report
/// as skipped rather than silently missing.
void BenchSkip(const std::string &name, const std::string &desc,
               const std::string &reason);
