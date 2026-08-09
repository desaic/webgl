#pragma once

#include <chrono>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

/// named accumulating timers. replaces the scattered local Stopwatch
/// variables so per-stage costs survive across calls and can be reported
/// as one table.
///
/// accumulation is NOT thread safe. pocketfft and FastSweepPar spawn their
/// own worker threads but never touch the profiler, so all Add() calls come
/// from the main thread. do not add scopes inside a parallel region.

using ProfileId = int;

struct ProfileCounter {
    std::string name;
    double totalMs = 0.0;
    unsigned long calls = 0;
    double minMs = 0.0;
    double maxMs = 0.0;
};

class Profiler {
  public:
    /// interns a counter name. call once per site via the macro below.
    static ProfileId GetId(const char *name);
    static void Add(ProfileId id, double ms);
    /// pure tally, no time. adds n to the call count and leaves totalMs at 0.
    /// for "how many grid queries did that cost" questions, where wrapping
    /// each event in a timer would cost more than the event.
    static void AddCount(ProfileId id, unsigned long n);

    static void SetEnabled(bool on);
    static bool Enabled();

    /// zeroes all counters but keeps the ids valid.
    static void Reset();

    static const std::vector<ProfileCounter> &Counters();
    /// -1 if the name was never registered.
    static ProfileId Find(const char *name);
    static double TotalMs(const char *name);
    static unsigned long Calls(const char *name);

    /// aligned table, counters with 0 calls omitted.
    /// totalRefMs > 0 adds a percent-of-total column.
    static void Report(std::ostream &out, const std::string &title,
                       double totalRefMs = 0.0);
    /// one row per counter: name,totalMs,calls,meanMs,minMs,maxMs
    static void ReportCsv(std::ostream &out, const std::string &tag);
};

/// RAII timer. does nothing measurable when the profiler is disabled.
class ProfileScope {
  public:
    explicit ProfileScope(ProfileId id) : id_(id) {
      if (Profiler::Enabled()) {
        start_ = std::chrono::steady_clock::now();
        active_ = true;
      }
    }
    ~ProfileScope() {
      if (!active_) {
        return;
      }
      std::chrono::duration<double, std::milli> ms =
          std::chrono::steady_clock::now() - start_;
      Profiler::Add(id_, ms.count());
    }
    ProfileScope(const ProfileScope &) = delete;
    ProfileScope &operator=(const ProfileScope &) = delete;

  private:
    ProfileId id_;
    bool active_ = false;
    std::chrono::steady_clock::time_point start_;
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)

/// times the enclosing scope under 'name'. the id lookup happens once.
#define PROFILE_SCOPE(name)                                             \
  static const ProfileId PROFILE_CONCAT(pfId_, __LINE__) =              \
      Profiler::GetId(name);                                            \
  ProfileScope PROFILE_CONCAT(pfScope_, __LINE__)(                      \
      PROFILE_CONCAT(pfId_, __LINE__))

/// tallies n events under 'name'. no timing.
#define PROFILE_COUNT(name, n)                                          \
  do {                                                                  \
    static const ProfileId PROFILE_CONCAT(pfCntId_, __LINE__) =         \
        Profiler::GetId(name);                                          \
    Profiler::AddCount(PROFILE_CONCAT(pfCntId_, __LINE__),              \
                       (unsigned long)(n));                             \
  } while (0)
