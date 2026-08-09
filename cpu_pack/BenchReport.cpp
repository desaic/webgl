#include "BenchReport.h"

#include "BenchOutput.h"
#include "MemStats.h"
#include "PackingScene.h"
#include "Profiler.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <ostream>

namespace {
std::ostream *g_csv = nullptr;
std::string g_desc;

double NowMS() {
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration<double, std::milli>(now).count();
}
} // namespace

void BenchRegion::SetCsv(std::ostream *out) {
  g_csv = out;
}

void BenchRegion::SetCurrentDesc(const std::string &desc) {
  g_desc = desc;
}

BenchRegion::BenchRegion(const std::string &name) : name_(name) {
  Profiler::Reset();
  startRss_ = RssBytes();
  startMs_ = NowMS();
}

double BenchRegion::ElapsedMS() const {
  return NowMS() - startMs_;
}

void BenchRegion::Note(const std::string &line) {
  notes_ += "  " + line + "\n";
  noteList_.push_back(line);
}

void BenchRegion::Finish(const PackingScene *scene) {
  if (finished_) {
    return;
  }
  finished_ = true;
  double ms = ElapsedMS();
  size_t rss = RssBytes();

  std::cout << "  wall " << std::fixed << std::setprecision(1) << ms
            << " ms (" << std::setprecision(2) << (ms / 1000.0) << " s)\n";
  std::cout.unsetf(std::ios::floatfield);
  std::cout << "  rss " << FormatBytes(rss) << " (delta "
            << (rss >= startRss_ ? "+" : "-")
            << FormatBytes(rss >= startRss_ ? rss - startRss_
                                            : startRss_ - rss)
            << ", peak " << FormatBytes(PeakRssBytes()) << ")\n";
  if (!notes_.empty()) {
    std::cout << notes_;
  }
  Profiler::Report(std::cout, name_ + " breakdown", ms);
  if (scene != nullptr) {
    ReportMemory(std::cout, name_ + " scene memory", *scene);
  }
  if (g_csv != nullptr) {
    *g_csv << name_ << ",wall_total," << ms << ",1," << ms << "," << ms << ","
           << ms << "\n";
    Profiler::ReportCsv(*g_csv, name_);
  }

  BenchResult res;
  res.name = name_;
  res.desc = g_desc;
  res.wallMs = ms;
  res.rssBytes = rss;
  res.peakRssBytes = PeakRssBytes();
  res.notes = noteList_;
  const std::vector<ProfileCounter> &counters = Profiler::Counters();
  for (size_t i = 0; i < counters.size(); i++) {
    if (counters[i].calls == 0) {
      continue;
    }
    BenchCounter c;
    c.name = counters[i].name;
    c.totalMs = counters[i].totalMs;
    c.calls = counters[i].calls;
    c.meanMs = counters[i].totalMs / double(counters[i].calls);
    c.maxMs = counters[i].maxMs;
    c.pct = ms > 0.0 ? 100.0 * counters[i].totalMs / ms : 0.0;
    res.counters.push_back(c);
  }
  if (scene != nullptr) {
    SceneMemory m = MeasureSceneMemory(*scene);
    res.sceneMemory.push_back({"bg.vox", m.bgVox});
    res.sceneMemory.push_back({"bg.fft", m.bgFft});
    res.sceneMemory.push_back({"container.sdf", m.containerSdf});
    res.sceneMemory.push_back({"container.grids", m.containerGrids});
    res.sceneMemory.push_back({"item.meshes", m.itemMeshes});
    res.sceneMemory.push_back({"item.sdfs", m.itemSdfs});
    res.sceneMemory.push_back({"item.samples", m.itemSamples});
    res.sceneMemory.push_back(
        {"kindGrids (" + std::to_string(m.kindGridsCount) + " entries)",
         m.kindGrids});
    res.sceneMemory.push_back({"trajectories", m.trajectories});
    res.sceneMemory.push_back({"broadPhase", m.broadPhase});
    res.sceneMemory.push_back({"total", m.Total()});
  }
  BenchResults().push_back(res);

  std::cout << "\n";
}

void BenchSkip(const std::string &name, const std::string &desc,
               const std::string &reason) {
  std::cout << "  skipped: " << reason << "\n\n";
  BenchResult res;
  res.name = name;
  res.desc = desc;
  res.skipped = true;
  res.skipReason = reason;
  BenchResults().push_back(res);
}

void BenchHeader(const std::string &name, const std::string &desc) {
  std::cout << "\n========== " << name << " ==========\n";
  if (!desc.empty()) {
    std::cout << desc << "\n";
  }
}

void BenchNote(const std::string &line) {
  std::cout << "  " << line << "\n";
}
