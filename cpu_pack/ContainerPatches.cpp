#include "ContainerPatches.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <map>
#include <utility>

std::vector<ShellPatch> BuildShellPatches(const TrigMesh &shell, float radius, float maxAngleDeg) {
  const size_t nt = shell.t.size() / 3;
  std::vector<Vec3f> cen(nt), nrm(nt);
  std::vector<float> area(nt);
  auto vert = [&](unsigned i) { return Vec3f(shell.v[3 * i], shell.v[3 * i + 1], shell.v[3 * i + 2]); };
  for (size_t i = 0; i < nt; ++i) {
    const Vec3f a = vert(shell.t[3 * i]), b = vert(shell.t[3 * i + 1]), c = vert(shell.t[3 * i + 2]);
    cen[i] = (a + b + c) * (1.0f / 3.0f);
    Vec3f n = (b - a).cross(c - a);
    area[i] = 0.5f * n.norm();
    if (n.norm2() > 1e-12f) n.normalize();
    nrm[i] = n;
  }
  // edge adjacency.
  std::map<std::pair<unsigned, unsigned>, std::vector<unsigned>> edgeTrigs;
  for (size_t i = 0; i < nt; ++i)
    for (unsigned e = 0; e < 3; ++e) {
      const unsigned a = shell.t[3 * i + e], b = shell.t[3 * i + (e + 1) % 3];
      edgeTrigs[{std::min(a, b), std::max(a, b)}].push_back(unsigned(i));
    }
  std::vector<std::vector<unsigned>> adj(nt);
  for (const auto &kv : edgeTrigs)
    for (unsigned a : kv.second)
      for (unsigned b : kv.second)
        if (a != b) adj[a].push_back(b);

  const float cosMax = std::cos(maxAngleDeg * 3.14159265f / 180.0f);
  const float r2 = radius * radius, core2 = 0.25f * radius * radius;
  std::vector<char> covered(nt, 0);
  std::vector<ShellPatch> patches;
  std::vector<unsigned> stamp(nt, ~0u);
  for (size_t seed = 0; seed < nt; ++seed) {
    if (covered[seed]) continue;
    ShellPatch p;
    p.center = cen[seed];
    p.normal = nrm[seed];
    const unsigned pid = unsigned(patches.size());
    std::deque<unsigned> q(1, unsigned(seed));
    stamp[seed] = pid;
    while (!q.empty()) {
      const unsigned t = q.front();
      q.pop_front();
      p.trigs.push_back(t);
      p.area += area[t];
      if ((cen[t] - p.center).norm2() <= core2 && nrm[t].dot(p.normal) >= cosMax) covered[t] = 1;
      for (unsigned u : adj[t]) {
        if (stamp[u] == pid) continue;
        if ((cen[u] - p.center).norm2() > r2 || nrm[u].dot(p.normal) < cosMax) continue;
        stamp[u] = pid;
        q.push_back(u);
      }
    }
    covered[seed] = 1;
    patches.push_back(std::move(p));
  }
  return patches;
}

void SavePatchesObj(const std::string &path, const TrigMesh &shell,
                    const std::vector<ShellPatch> &patches) {
  std::ofstream out(path);
  size_t base = 0;
  for (size_t pi = 0; pi < patches.size(); ++pi) {
    out << "g patch_" << pi << "\n";
    for (unsigned t : patches[pi].trigs)
      for (unsigned k = 0; k < 3; ++k) {
        const unsigned v = shell.t[3 * t + k];
        out << "v " << shell.v[3 * v] << ' ' << shell.v[3 * v + 1] << ' ' << shell.v[3 * v + 2] << "\n";
      }
    for (size_t i = 0; i < patches[pi].trigs.size(); ++i)
      out << "f " << base + 3 * i + 1 << ' ' << base + 3 * i + 2 << ' ' << base + 3 * i + 3 << "\n";
    base += 3 * patches[pi].trigs.size();
  }
}
