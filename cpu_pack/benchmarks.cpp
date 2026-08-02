#include "BenchOutput.h"
#include "BenchReport.h"
#include "Benchmark.h"
#include "Log.h"
#include "MemStats.h"
#include "PackingConfig.h"
#include "PackingPlan.h"
#include "Profiler.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

namespace {

/// mirrors everything written to cout into a file, so the run leaves a
/// transcript without the caller having to redirect.
class TeeBuf : public std::streambuf {
  public:
    TeeBuf(std::streambuf *a, std::streambuf *b) : a_(a), b_(b) {}

  protected:
    int overflow(int c) override {
      if (c == EOF) {
        return !EOF;
      }
      int r1 = a_->sputc(char(c));
      int r2 = b_->sputc(char(c));
      return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    int sync() override {
      int r1 = a_->pubsync();
      int r2 = b_->pubsync();
      return (r1 == 0 && r2 == 0) ? 0 : -1;
    }

  private:
    std::streambuf *a_;
    std::streambuf *b_;
};

double NowSec() {
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration<double>(now).count();
}

void PrintUsage() {
  std::cout << "usage: cpu_pack_bench [dataDir] [options]\n";
  std::cout << "  --list             print scenario names and exit\n";
  std::cout << "  --only a,b,c       run only these scenarios\n";
  std::cout << "  --items N          items or placements per scenario"
               " (default 4)\n";
  std::cout << "  --validate         check each placement against the"
               " container and neighbors\n";
  std::cout << "  --csv FILE         write one row per counter to FILE\n";
  std::cout << "  --budget N         total wall budget in seconds"
               " (default 300)\n";
  std::cout << "  --verbose          restore the normal packing log output\n";
  std::cout << "  --help             this message\n";
  std::cout << "writes results.txt and summary.html next to the"
               " executable.\n";
}

void PrintList() {
  const std::vector<Bench> &benches = AllBenches();
  for (size_t i = 0; i < benches.size(); i++) {
    std::cout << "  " << benches[i].name << "  -  " << benches[i].desc << "\n";
  }
}

bool Selected(const std::vector<std::string> &only, const std::string &name) {
  if (only.empty()) {
    return true;
  }
  for (size_t i = 0; i < only.size(); i++) {
    if (only[i] == name) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> SplitCommas(const std::string &s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t comma = s.find(',', start);
    if (comma == std::string::npos) {
      if (start < s.size()) {
        out.push_back(s.substr(start));
      }
      break;
    }
    if (comma > start) {
      out.push_back(s.substr(start, comma - start));
    }
    start = comma + 1;
  }
  return out;
}

} // namespace

int main(int argc, char *argv[]) {
  BenchContext ctx;
  // ParseArgs only looks at argv[1] and the env var, so the flags below
  // are free to reuse the rest of the command line.
  ctx.cfg.ParseArgs(argc, argv);

  std::vector<std::string> only;
  std::string csvPath;
  bool verbose = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      PrintUsage();
      return 0;
    } else if (a == "--list") {
      PrintList();
      return 0;
    } else if (a == "--validate") {
      ctx.validate = true;
    } else if (a == "--verbose") {
      verbose = true;
    } else if (a == "--only" && i + 1 < argc) {
      only = SplitCommas(argv[++i]);
    } else if (a == "--items" && i + 1 < argc) {
      ctx.itemCount = unsigned(std::atoi(argv[++i]));
    } else if (a == "--csv" && i + 1 < argc) {
      csvPath = argv[++i];
    } else if (a == "--budget" && i + 1 < argc) {
      ctx.budgetSec = float(std::atof(argv[++i]));
    } else if (a == "--griddx" && i + 1 < argc) {
      // narrow phase cell size. the cost of a contact query is the number
      // of triangles in the cells it touches, so this is the knob that
      // trades narrow phase time against grid memory.
      ctx.cfg.gridDx = float(std::atof(argv[++i]));
    }
  }
  if (ctx.itemCount == 0) {
    ctx.itemCount = 1;
  }
  if (ctx.budgetSec < 1.0f) {
    ctx.budgetSec = 1.0f;
  }

  // transcript and html both land next to the binary so a run is
  // self-contained regardless of the working directory.
  std::string exeDir = ExeDir();
  std::string txtPath = exeDir + "results.txt";
  std::ofstream txt(txtPath);
  std::streambuf *originalCout = std::cout.rdbuf();
  TeeBuf tee(originalCout, txt.rdbuf());
  if (txt.good()) {
    std::cout.rdbuf(&tee);
  } else {
    std::cout << "could not open " << txtPath << ", console only\n";
  }
  // a scenario can run for minutes; keep the transcript current so a
  // killed run still leaves usable output.
  std::cout.setf(std::ios::unitbuf);

  // per placement logging would otherwise dominate the wall clock and drown
  // the tables. --verbose puts it back when debugging a scenario.
  SetLogLevel(verbose ? LOG_INFO : LOG_SILENT);
  Profiler::SetEnabled(true);

  std::ofstream csv;
  if (!csvPath.empty()) {
    csv.open(csvPath);
    if (csv.good()) {
      csv << "scenario,counter,total_ms,calls,mean_ms,min_ms,max_ms\n";
      ctx.csv = &csv;
      BenchRegion::SetCsv(&csv);
    } else {
      std::cout << "could not open csv " << csvPath << "\n";
    }
  }

  // benchmarks never write pack or trajectory files.
  ctx.cfg.trajSaveInterval = 0;
  ctx.cfg.packSaveInterval = 0;
  ctx.cfg.computeStats = false;

  std::string configText = ctx.cfg.toString();
  std::cout << configText;
  std::cout << "items per scenario " << ctx.itemCount << "\n";
  std::cout << "validation " << (ctx.validate ? "on" : "off") << "\n";
  std::cout << "wall budget " << ctx.budgetSec << " s\n";
  std::cout << "transcript " << txtPath << "\n";

  ctx.plan = PlanPackingSteps(ctx.cfg.MeshDir());
  if (ctx.plan.steps.empty()) {
    std::cout << "warning: empty packing plan, end_to_end will be skipped\n";
  }

  const std::vector<Bench> &benches = AllBenches();
  unsigned ran = 0, skipped = 0;
  double t0 = NowSec();
  for (size_t i = 0; i < benches.size(); i++) {
    if (!Selected(only, benches[i].name)) {
      continue;
    }
    ctx.spentSec = float(NowSec() - t0);
    BenchHeader(benches[i].name, benches[i].desc);
    BenchRegion::SetCurrentDesc(benches[i].desc);
    // scenarios that manage their own budget (end_to_end) are allowed to
    // start with very little left; they shrink instead of skipping.
    if (ctx.RemainingSec() <= 0.0f) {
      BenchSkip(benches[i].name, benches[i].desc, "wall budget exhausted");
      skipped++;
      continue;
    }
    benches[i].fn(ctx);
    ran++;
  }
  double totalWallMs = 1000.0 * (NowSec() - t0);

  if (ran == 0) {
    std::cout << "no scenario matched. --list shows the names.\n";
    return 1;
  }
  std::cout << "\nran " << ran << " scenarios";
  if (skipped > 0) {
    std::cout << ", skipped " << skipped << " for budget";
  }
  std::cout << ". total " << std::fixed << std::setprecision(1)
            << (totalWallMs / 1000.0) << " s of " << ctx.budgetSec
            << " s budget, peak rss " << FormatBytes(PeakRssBytes()) << "\n";
  std::cout.unsetf(std::ios::floatfield);

  std::string html =
      WriteBenchHtml("cpu_pack benchmarks", configText, totalWallMs);
  if (html.empty()) {
    std::cout << "could not write summary.html to " << exeDir << "\n";
  } else {
    std::cout << "summary " << html << "\n";
  }

  std::cout.flush();
  std::cout.rdbuf(originalCout);
  return 0;
}
