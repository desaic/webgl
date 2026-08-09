#include "BenchOutput.h"

#include "MemStats.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef __linux__
#include <unistd.h>
#endif

std::string ExeDir() {
#ifdef __linux__
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    std::string p(buf);
    size_t slash = p.find_last_of('/');
    if (slash != std::string::npos) {
      return p.substr(0, slash + 1);
    }
  }
#endif
  return "./";
}

std::vector<BenchResult> &BenchResults() {
  static std::vector<BenchResult> results;
  return results;
}

namespace {

std::string Escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '&') {
      out += "&amp;";
    } else if (c == '<') {
      out += "&lt;";
    } else if (c == '>') {
      out += "&gt;";
    } else {
      out += c;
    }
  }
  return out;
}

std::string Fixed(double v, int prec) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(prec) << v;
  return oss.str();
}

// a bar cell whose width shows the counter's share of its scenario.
std::string BarCell(double pct) {
  std::ostringstream oss;
  double w = pct;
  if (w < 0.0) {
    w = 0.0;
  }
  if (w > 100.0) {
    w = 100.0;
  }
  // the bar is a fixed-width track with the label after it, so a 0.1%
  // bar does not end up with its label sitting on top of the bar.
  oss << "<td class=\"bar\"><div class=\"track\"><div class=\"fill\""
         " style=\"width:"
      << Fixed(w, 1) << "%\"></div></div><span>" << Fixed(pct, 1)
      << "%</span></td>";
  return oss.str();
}

const char *kStyle =
    "body{font:14px/1.5 system-ui,sans-serif;margin:24px;color:#222;"
    "max-width:1100px}"
    "h1{font-size:22px;margin-bottom:4px}"
    "h2{font-size:17px;margin:28px 0 6px;border-bottom:2px solid #ddd;"
    "padding-bottom:3px}"
    "p.desc{color:#666;margin:2px 0 10px}"
    "table{border-collapse:collapse;width:100%;margin:8px 0 4px}"
    "th,td{padding:4px 8px;text-align:right;border-bottom:1px solid #eee}"
    "th:first-child,td:first-child{text-align:left;font-family:ui-monospace,"
    "monospace}"
    "th{background:#f5f5f5;font-weight:600}"
    "td.bar{width:200px;text-align:left;display:flex;align-items:center;"
    "gap:8px}"
    "td.bar .track{flex:1;background:#eef1f5;height:12px;border-radius:2px;"
    "overflow:hidden}"
    "td.bar .fill{background:#8ab4f8;height:12px;border-radius:2px;"
    "min-width:1px}"
    "td.bar span{font-size:11px;color:#555;width:44px;text-align:right;"
    "flex:none}"
    "ul.notes{margin:4px 0 10px;padding-left:20px;color:#444}"
    "code{background:#f2f2f2;padding:1px 4px;border-radius:3px}"
    "pre{background:#f7f7f7;padding:10px;border-radius:4px;overflow-x:auto;"
    "font-size:12px}"
    ".skip{color:#a00}"
    ".sum td:first-child{font-family:inherit}"
    ".hdr{color:#666;font-size:12px}";

} // namespace

std::string WriteBenchHtml(const std::string &title,
                           const std::string &configText, double totalWallMs) {
  const std::vector<BenchResult> &results = BenchResults();
  std::string path = ExeDir() + "summary.html";
  std::ofstream out(path);
  if (!out.good()) {
    return std::string();
  }

  out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n";
  out << "<title>" << Escape(title) << "</title>\n";
  out << "<style>" << kStyle << "</style>\n</head><body>\n";
  out << "<h1>" << Escape(title) << "</h1>\n";
  out << "<p class=\"hdr\">total wall " << Fixed(totalWallMs / 1000.0, 1)
      << " s, " << results.size() << " scenarios, peak rss "
      << Escape(FormatBytes(PeakRssBytes())) << "</p>\n";

  // overview first, so the expensive scenarios are obvious without
  // scrolling through every breakdown.
  out << "<h2>overview</h2>\n<table class=\"sum\"><tr><th>scenario</th>"
         "<th>wall s</th><th>share</th><th>rss</th></tr>\n";
  for (size_t i = 0; i < results.size(); i++) {
    const BenchResult &r = results[i];
    out << "<tr><td><a href=\"#" << Escape(r.name) << "\">"
        << Escape(r.name) << "</a></td>";
    if (r.skipped) {
      out << "<td colspan=\"3\" class=\"skip\">skipped: "
          << Escape(r.skipReason) << "</td></tr>\n";
      continue;
    }
    out << "<td>" << Fixed(r.wallMs / 1000.0, 2) << "</td>";
    double share = totalWallMs > 0.0 ? 100.0 * r.wallMs / totalWallMs : 0.0;
    out << BarCell(share);
    out << "<td>" << Escape(FormatBytes(r.rssBytes)) << "</td></tr>\n";
  }
  out << "</table>\n";

  for (size_t i = 0; i < results.size(); i++) {
    const BenchResult &r = results[i];
    out << "<h2 id=\"" << Escape(r.name) << "\">" << Escape(r.name)
        << "</h2>\n";
    out << "<p class=\"desc\">" << Escape(r.desc) << "</p>\n";
    if (r.skipped) {
      out << "<p class=\"skip\">skipped: " << Escape(r.skipReason)
          << "</p>\n";
      continue;
    }
    out << "<p class=\"hdr\">wall " << Fixed(r.wallMs, 1) << " ms, rss "
        << Escape(FormatBytes(r.rssBytes)) << ", peak "
        << Escape(FormatBytes(r.peakRssBytes)) << "</p>\n";
    if (!r.notes.empty()) {
      out << "<ul class=\"notes\">\n";
      for (size_t j = 0; j < r.notes.size(); j++) {
        out << "<li>" << Escape(r.notes[j]) << "</li>\n";
      }
      out << "</ul>\n";
    }
    if (!r.counters.empty()) {
      out << "<table><tr><th>counter</th><th>total ms</th><th>calls</th>"
             "<th>mean ms</th><th>max ms</th><th>share</th></tr>\n";
      for (size_t j = 0; j < r.counters.size(); j++) {
        const BenchCounter &c = r.counters[j];
        out << "<tr><td>" << Escape(c.name) << "</td><td>"
            << Fixed(c.totalMs, 2) << "</td><td>" << c.calls << "</td><td>"
            << Fixed(c.meanMs, 3) << "</td><td>" << Fixed(c.maxMs, 3)
            << "</td>" << BarCell(c.pct) << "</tr>\n";
      }
      out << "</table>\n";
    }
    if (!r.sceneMemory.empty()) {
      out << "<table><tr><th>structure</th><th>bytes</th></tr>\n";
      for (size_t j = 0; j < r.sceneMemory.size(); j++) {
        out << "<tr><td>" << Escape(r.sceneMemory[j].first) << "</td><td>"
            << Escape(FormatBytes(r.sceneMemory[j].second)) << "</td></tr>\n";
      }
      out << "</table>\n";
    }
  }

  out << "<h2>config</h2>\n<pre>" << Escape(configText) << "</pre>\n";
  out << "</body></html>\n";
  return path;
}
