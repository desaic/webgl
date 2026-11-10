#include "CavityField.h"

#include "MeshOps.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <queue>

namespace {

const float kBig = 1e10f;
const float kSent = 1e30f;

// lower envelope of parabolas, one scan line (Felzenszwalb and Huttenlocher).
void Edt1D(const float *f, float *d, int n, std::vector<int> &v, std::vector<float> &z) {
  int k = 0;
  v[0] = 0;
  z[0] = -kSent;
  z[1] = kSent;
  for (int q = 1; q < n; q++) {
    float s;
    while (true) {
      const int p = v[k];
      s = ((f[q] + float(q) * q) - (f[p] + float(p) * p)) / (2.0f * float(q - p));
      if (s <= z[k] && k > 0) k--;
      else break;
    }
    k++;
    v[k] = q;
    z[k] = s;
    z[k + 1] = kSent;
  }
  k = 0;
  for (int q = 0; q < n; q++) {
    while (z[k + 1] < float(q)) k++;
    const float dd = float(q - v[k]);
    d[q] = std::min(kBig, dd * dd + f[v[k]]);
  }
}

}  // namespace

void DistanceToSeeds(const Array3D8u &seed, float dx, Array3D<float> &dist) {
  const Vec3u s = seed.GetSize();
  dist.Allocate(s, kBig);
  for (size_t i = 0; i < seed.GetData().size(); i++) dist.GetData()[i] = seed.GetData()[i] ? 0.0f : kBig;
  const int n = int(std::max({s[0], s[1], s[2]}));
  std::vector<float> f(n), d(n), z(n + 2);
  std::vector<int> v(n);
  for (unsigned k = 0; k < s[2]; k++)
    for (unsigned j = 0; j < s[1]; j++) {
      for (unsigned i = 0; i < s[0]; i++) f[i] = dist(i, j, k);
      Edt1D(f.data(), d.data(), int(s[0]), v, z);
      for (unsigned i = 0; i < s[0]; i++) dist(i, j, k) = d[i];
    }
  for (unsigned k = 0; k < s[2]; k++)
    for (unsigned i = 0; i < s[0]; i++) {
      for (unsigned j = 0; j < s[1]; j++) f[j] = dist(i, j, k);
      Edt1D(f.data(), d.data(), int(s[1]), v, z);
      for (unsigned j = 0; j < s[1]; j++) dist(i, j, k) = d[j];
    }
  for (unsigned j = 0; j < s[1]; j++)
    for (unsigned i = 0; i < s[0]; i++) {
      for (unsigned k = 0; k < s[2]; k++) f[k] = dist(i, j, k);
      Edt1D(f.data(), d.data(), int(s[2]), v, z);
      for (unsigned k = 0; k < s[2]; k++) dist(i, j, k) = d[k];
    }
  for (float &x : dist.GetData()) x = x >= kBig * 0.5f ? 1e9f : std::sqrt(x) * dx;
}

static void BoxOfPose(const MeshInfo &mi, const RigidTransform &tran, bool exclusion, Box3f &out) {
  if (exclusion) {
    out = mi.box;
    return;
  }
  Vec3f lo(1e30f), hi(-1e30f);
  for (int c = 0; c < 8; c++) {
    const Vec3f p((c & 1) ? mi.box.vmax[0] : mi.box.vmin[0], (c & 2) ? mi.box.vmax[1] : mi.box.vmin[1],
                  (c & 4) ? mi.box.vmax[2] : mi.box.vmin[2]);
    const Vec3f w = tran.rotation * p + tran.position;
    for (int a = 0; a < 3; a++) {
      lo[a] = std::min(lo[a], w[a]);
      hi[a] = std::max(hi[a], w[a]);
    }
  }
  out.vmin = lo;
  out.vmax = hi;
}

unsigned StampFruit(PackingScene &scene, unsigned itemId, const RigidTransform &tran, CavityWindow &w,
                    Array3D8u &solid, unsigned *newVox) {
  scene.EnsureItemSamples(itemId);
  const MeshInfo &mi = scene.items[itemId];
  unsigned already = 0, added = 0;
  if (!mi.sdf) return 0;
  Box3f b;
  BoxOfPose(mi, tran, itemId == scene.exclusionItemIndex, b);
  int lo[3], hi[3];
  for (int a = 0; a < 3; a++) {
    lo[a] = std::max(0, int(std::floor((b.vmin[a] - 0.5f - w.origin[a]) / w.dx)));
    hi[a] = std::min(int(w.size[a]) - 1, int(std::floor((b.vmax[a] + 0.5f - w.origin[a]) / w.dx)));
  }
  const Matrix3f inv = tran.rotation.transposed();
  for (int k = lo[2]; k <= hi[2]; k++)
    for (int j = lo[1]; j <= hi[1]; j++)
      for (int i = lo[0]; i <= hi[0]; i++) {
        const Vec3f p = w.Center(i, j, k);
        if (mi.sdf->GetCoarseDist(inv * (p - tran.position)) < 0.0f) {
          if (solid(i, j, k)) already++;
          else {
            solid(i, j, k) = 1;
            added++;
          }
        }
      }
  if (newVox) *newVox = added;
  return already;
}

void BuildCavityWindow(PackingScene &scene, const Box3f &box, const CavityParams &cp, unsigned skipId,
                       CavityWindow &w) {
  w.dx = cp.dx;
  w.origin = box.vmin;
  const Vec3f ext = box.vmax - box.vmin;
  w.size = Vec3u(unsigned(std::ceil(ext[0] / cp.dx)), unsigned(std::ceil(ext[1] / cp.dx)),
                 unsigned(std::ceil(ext[2] / cp.dx)));
  w.solid.Allocate(w.size, 0);
  w.wall.Allocate(w.size, 0);
  w.depth.Allocate(w.size, 0.0f);
  w.sd.Allocate(w.size, 0.0f);
  for (unsigned k = 0; k < w.size[2]; k++)
    for (unsigned j = 0; j < w.size[1]; j++)
      for (unsigned i = 0; i < w.size[0]; i++) {
        const float d = scene.sdf->GetCoarseDist(w.Center(i, j, k));
        w.sd(i, j, k) = d;
        if (d > 0.0f) w.wall(i, j, k) = 1;
        else w.depth(i, j, k) = -d;
      }
  Box3f region;
  region.vmin = w.origin;
  region.vmax = w.origin + w.dx * Vec3f(float(w.size[0]), float(w.size[1]), float(w.size[2]));
  for (unsigned j : scene.broadPhase.GetNearby(region, 0.0f)) {
    if (j == skipId) continue;
    const InstanceInfo &inst = scene.instances[j];
    StampFruit(scene, inst.itemId, inst.tran, w, w.solid);
  }
}

void BuildCavMask(const CavityWindow &w, const Array3D8u &solid, const CavityParams &cp, Array3D<float> &D,
                  Array3D8u &cav, Array3D8u &reachFree) {
  const Vec3u s = w.size;
  const size_t n = solid.GetData().size();
  // tool model: a cylinder of radius R with a ball tip, axis within coneDeg of the outward
  // direction, comes in from the open air (everything outside the container, the wall is not a
  // thing). a free voxel is machined if it lies within R of a reachable tool center; the rest are
  // pockets and undercuts the tool cannot make.
  DistanceToSeeds(solid, w.dx, D);
  Array3D8u ok(s, 0);
  for (size_t i = 0; i < n; i++) ok.GetData()[i] = (!solid.GetData()[i] && D.GetData()[i] >= cp.R) ? 1 : 0;
  // allowed directions per voxel from the outward normal (container sdf gradient).
  std::vector<std::array<float, 3>> dirs;
  for (int a = -1; a <= 1; a++)
    for (int b = -1; b <= 1; b++)
      for (int c2 = -1; c2 <= 1; c2++) {
        if (!a && !b && !c2) continue;
        const float l = std::sqrt(float(a * a + b * b + c2 * c2));
        dirs.push_back({a / l, b / l, c2 / l});
      }
  const float cosCone = std::cos(cp.coneDeg * 3.14159265f / 180.0f);
  std::vector<unsigned> allowed(n, 0u);
  for (unsigned k = 1; k + 1 < s[2]; k++)
    for (unsigned j = 1; j + 1 < s[1]; j++)
      for (unsigned i = 1; i + 1 < s[0]; i++) {
        if (!ok(i, j, k)) continue;
        Vec3f g(w.sd(i + 1, j, k) - w.sd(i - 1, j, k), w.sd(i, j + 1, k) - w.sd(i, j - 1, k),
                w.sd(i, j, k + 1) - w.sd(i, j, k - 1));
        const float gl = g.norm();
        if (gl < 1e-6f) continue;
        g = (1.0f / gl) * g;
        unsigned m = 0;
        for (size_t d = 0; d < dirs.size(); d++)
          if (dirs[d][0] * g[0] + dirs[d][1] * g[1] + dirs[d][2] * g[2] >= cosCone) m |= 1u << d;
        allowed[i + size_t(s[0]) * (j + size_t(s[1]) * k)] = m;
      }
  Array3D8u centers(s, 0);
  Array3D8u run(s, 0);
  size_t di = 0;
  for (int a = -1; a <= 1; a++)
    for (int b = -1; b <= 1; b++)
      for (int c2 = -1; c2 <= 1; c2++) {
        if (!a && !b && !c2) continue;
        const size_t d = di++;
        // run(p): the cylinder from p along (a,b,c2) to the open air is free.
        const int x0 = a > 0 ? int(s[0]) - 1 : 0, x1 = a > 0 ? -1 : int(s[0]), xs = a > 0 ? -1 : 1;
        const int y0 = b > 0 ? int(s[1]) - 1 : 0, y1 = b > 0 ? -1 : int(s[1]), ys = b > 0 ? -1 : 1;
        const int z0 = c2 > 0 ? int(s[2]) - 1 : 0, z1 = c2 > 0 ? -1 : int(s[2]), zs = c2 > 0 ? -1 : 1;
        for (int k = z0; k != z1; k += zs)
          for (int j = y0; j != y1; j += ys)
            for (int i = x0; i != x1; i += xs) {
              if (!ok(i, j, k)) {
                run(i, j, k) = 0;
                continue;
              }
              const int x = i + a, y = j + b, z = k + c2;
              if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) {
                run(i, j, k) = w.sd(i, j, k) > 0.0f ? 1 : 0;
              } else {
                run(i, j, k) = w.sd(i, j, k) > 0.0f ? 1 : run(x, y, z);
              }
              if (run(i, j, k) && (allowed[i + size_t(s[0]) * (j + size_t(s[1]) * k)] >> d & 1u)) centers(i, j, k) = 1;
            }
      }
  // open air voxels with room for the tool are centers too.
  for (size_t i = 0; i < n; i++)
    if (ok.GetData()[i]) {
      // sd > 0 at the voxel: outside the container.
    }
  Array3D<float> reach;
  DistanceToSeeds(centers, w.dx, reach);
  cav.Allocate(s, 0);
  reachFree.Allocate(s, 0);
  for (size_t i = 0; i < n; i++) {
    reachFree.GetData()[i] = (!solid.GetData()[i] && reach.GetData()[i] <= cp.R) ? 1 : 0;
    cav.GetData()[i] = (!solid.GetData()[i] && !w.wall.GetData()[i] && reach.GetData()[i] > cp.R &&
                        w.depth.GetData()[i] <= cp.band)
                           ? 1
                           : 0;
  }
  // sealed voids are invisible and need no milling: keep only unreachable voxels connected to
  // the open air through free voxels with a passage wider than sealClear.
  Array3D8u seen(s, 0);
  std::vector<std::array<int, 3>> stack;
  for (unsigned k = 0; k < s[2]; k++)
    for (unsigned j = 0; j < s[1]; j++)
      for (unsigned i = 0; i < s[0]; i++)
        if (w.sd(i, j, k) > 0.0f && !solid(i, j, k) && D(i, j, k) >= cp.sealClear) {
          seen(i, j, k) = 1;
          stack.push_back({int(i), int(j), int(k)});
        }
  const int nb[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  while (!stack.empty()) {
    const auto p = stack.back();
    stack.pop_back();
    for (const auto &o : nb) {
      const int x = p[0] + o[0], y = p[1] + o[1], z = p[2] + o[2];
      if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
      if (seen(x, y, z) || solid(x, y, z) || D(x, y, z) < cp.sealClear) continue;
      seen(x, y, z) = 1;
      stack.push_back({x, y, z});
    }
  }
  for (unsigned k = 0; k < s[2]; k++)
    for (unsigned j = 0; j < s[1]; j++)
      for (unsigned i = 0; i < s[0]; i++) {
        if (!cav(i, j, k)) continue;
        bool open = seen(i, j, k);
        for (int d = 0; d < 6 && !open; d++) {
          const int x = int(i) + nb[d][0], y = int(j) + nb[d][1], z = int(k) + nb[d][2];
          if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
          open = seen(x, y, z);
        }
        if (!open) cav(i, j, k) = 0;
      }
}

std::vector<CavityComp> FindCavities(const CavityWindow &w, const Array3D8u &solid, const CavityParams &cp,
                                     const Box3f *core, float *totalScore, Array3D<unsigned> *labels) {
  const Vec3u s = w.size;
  Array3D<float> D;
  Array3D8u cav, reachFree;
  BuildCavMask(w, solid, cp, D, cav, reachFree);
  (void)labels;
  std::vector<CavityComp> out;
  const float vox = w.dx * w.dx * w.dx;
  auto weight = [&](unsigned i, unsigned j, unsigned k) {
    (void)i; (void)j; (void)k;
    return 1.0f;
  };
  // the cavity network is one connected maze, so split it by clearance: anchors are the voxels of
  // largest clearance, each suppressing its neighborhood.
  struct Cand {
    float d;
    unsigned i, j, k;
  };
  std::vector<Cand> cands;
  float total = 0.0f;
  for (unsigned k = 0; k < s[2]; k++)
    for (unsigned j = 0; j < s[1]; j++)
      for (unsigned i = 0; i < s[0]; i++) {
        if (!cav(i, j, k) || D(i, j, k) < cp.objClear) continue;
        const Vec3f c0 = w.Center(i, j, k);
        const bool in0 = !core || (c0[0] >= core->vmin[0] && c0[0] < core->vmax[0] && c0[1] >= core->vmin[1] &&
                                   c0[1] < core->vmax[1] && c0[2] >= core->vmin[2] && c0[2] < core->vmax[2]);
        if (in0) total += weight(i, j, k) * vox;
        if (D(i, j, k) >= cp.minClear) cands.push_back({D(i, j, k), i, j, k});
      }
  if (totalScore) *totalScore = total;
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) { return a.d > b.d; });
  Array3D8u taken(s, 0);
  for (const Cand &cd : cands) {
    if (taken(cd.i, cd.j, cd.k)) continue;
    const Vec3f at = w.Center(cd.i, cd.j, cd.k);
    const bool inCore = !core || (at[0] >= core->vmin[0] && at[0] < core->vmax[0] && at[1] >= core->vmin[1] &&
                                  at[1] < core->vmax[1] && at[2] >= core->vmin[2] && at[2] < core->vmax[2]);
    // suppress and gather the cavity voxels around the anchor.
    const float supp = std::max(1.5f, 1.0f * cd.d);
    const float gather = 1.5f * cd.d + 1.5f;
    const int rg = int(std::ceil(gather / w.dx));
    Vec3f sum(0, 0, 0);
    float wsum = 0.0f;
    unsigned cnt = 0;
    std::vector<Vec3f> pts;
    for (int dk = -rg; dk <= rg; dk++)
      for (int dj = -rg; dj <= rg; dj++)
        for (int di = -rg; di <= rg; di++) {
          const int x = int(cd.i) + di, y = int(cd.j) + dj, z = int(cd.k) + dk;
          if (x < 0 || y < 0 || z < 0 || x >= int(s[0]) || y >= int(s[1]) || z >= int(s[2])) continue;
          const float r = w.dx * std::sqrt(float(di * di + dj * dj + dk * dk));
          if (r > gather) continue;
          if (r <= supp) taken(x, y, z) = 1;
          if (!cav(x, y, z) || D(x, y, z) < cp.objClear) continue;
          const float wt = weight(x, y, z);
          const Vec3f p = w.Center(x, y, z);
          sum += wt * p;
          wsum += wt;
          cnt++;
          pts.push_back(p);
        }
    if (!inCore || cnt == 0) continue;
    CavityComp cc;
    cc.voxels = cnt;
    cc.volume = float(cnt) * vox;
    cc.score = wsum * vox;
    cc.clearance = cd.d;
    cc.deepest = at;
    cc.center = sum / wsum;
    float cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (const Vec3f &p : pts) {
      const Vec3f d = p - cc.center;
      for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) cov[a][b] += d[a] * d[b];
    }
    Vec3f ax(0.577f, 0.5f, 0.64f);
    for (int it = 0; it < 12; it++) {
      Vec3f nx(cov[0][0] * ax[0] + cov[0][1] * ax[1] + cov[0][2] * ax[2],
               cov[1][0] * ax[0] + cov[1][1] * ax[1] + cov[1][2] * ax[2],
               cov[2][0] * ax[0] + cov[2][1] * ax[1] + cov[2][2] * ax[2]);
      const float l = nx.norm();
      if (l < 1e-9f) break;
      ax = nx / l;
    }
    cc.axis = ax;
    out.push_back(cc);
  }
  return out;
}
