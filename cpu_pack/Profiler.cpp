#include "Profiler.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <ostream>

namespace {
std::vector<ProfileCounter> &Store() {
  static std::vector<ProfileCounter> counters;
  return counters;
}
std::map<std::string, ProfileId> &Index() {
  static std::map<std::string, ProfileId> index;
  return index;
}
bool g_enabled = true;
} // namespace

ProfileId Profiler::GetId(const char *name) {
  auto &index = Index();
  auto it = index.find(name);
  if (it != index.end()) {
    return it->second;
  }
  ProfileCounter c;
  c.name = name;
  Store().push_back(c);
  ProfileId id = ProfileId(Store().size() - 1);
  index[name] = id;
  return id;
}

void Profiler::Add(ProfileId id, double ms) {
  if (id < 0 || size_t(id) >= Store().size()) {
    return;
  }
  ProfileCounter &c = Store()[size_t(id)];
  if (c.calls == 0 || ms < c.minMs) {
    c.minMs = ms;
  }
  if (ms > c.maxMs) {
    c.maxMs = ms;
  }
  c.totalMs += ms;
  c.calls++;
}

void Profiler::AddCount(ProfileId id, unsigned long n) {
  if (!g_enabled || id < 0 || size_t(id) >= Store().size()) {
    return;
  }
  Store()[size_t(id)].calls += n;
}

void Profiler::SetEnabled(bool on) {
  g_enabled = on;
}

bool Profiler::Enabled() {
  return g_enabled;
}

void Profiler::Reset() {
  for (auto &c : Store()) {
    c.totalMs = 0.0;
    c.calls = 0;
    c.minMs = 0.0;
    c.maxMs = 0.0;
  }
}

const std::vector<ProfileCounter> &Profiler::Counters() {
  return Store();
}

ProfileId Profiler::Find(const char *name) {
  auto &index = Index();
  auto it = index.find(name);
  if (it == index.end()) {
    return -1;
  }
  return it->second;
}

double Profiler::TotalMs(const char *name) {
  ProfileId id = Find(name);
  if (id < 0) {
    return 0.0;
  }
  return Store()[size_t(id)].totalMs;
}

unsigned long Profiler::Calls(const char *name) {
  ProfileId id = Find(name);
  if (id < 0) {
    return 0;
  }
  return Store()[size_t(id)].calls;
}

void Profiler::Report(std::ostream &out, const std::string &title,
                      double totalRefMs) {
  const auto &counters = Store();
  size_t nameWidth = 12;
  bool any = false;
  for (const auto &c : counters) {
    if (c.calls == 0) {
      continue;
    }
    any = true;
    nameWidth = std::max(nameWidth, c.name.size());
  }
  out << "--- " << title << " ---\n";
  if (!any) {
    out << "  no counters recorded\n";
    return;
  }
  out << "  " << std::left << std::setw(int(nameWidth)) << "counter"
      << std::right << std::setw(12) << "total_ms" << std::setw(9) << "calls"
      << std::setw(11) << "mean_ms" << std::setw(11) << "max_ms";
  if (totalRefMs > 0.0) {
    out << std::setw(8) << "pct";
  }
  out << "\n";
  for (const auto &c : counters) {
    if (c.calls == 0) {
      continue;
    }
    double mean = c.totalMs / double(c.calls);
    out << "  " << std::left << std::setw(int(nameWidth)) << c.name
        << std::right << std::fixed << std::setprecision(2) << std::setw(12)
        << c.totalMs << std::setw(9) << c.calls << std::setprecision(3)
        << std::setw(11) << mean << std::setw(11) << c.maxMs;
    if (totalRefMs > 0.0) {
      out << std::setprecision(1) << std::setw(7)
          << (100.0 * c.totalMs / totalRefMs) << "%";
    }
    out << "\n";
  }
  out.unsetf(std::ios::floatfield);
}

void Profiler::ReportCsv(std::ostream &out, const std::string &tag) {
  for (const auto &c : Store()) {
    if (c.calls == 0) {
      continue;
    }
    double mean = c.totalMs / double(c.calls);
    out << tag << "," << c.name << "," << c.totalMs << "," << c.calls << ","
        << mean << "," << c.minMs << "," << c.maxMs << "\n";
  }
}
