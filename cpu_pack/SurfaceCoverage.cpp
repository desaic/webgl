#include "SurfaceCoverage.h"

#include "PointSample.h"
#include "Profiler.h"
#include "TrigMesh.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace {

struct HashKey3 {
  int x = 0, y = 0, z = 0;
  bool operator==(const HashKey3 &o) const {
    return x == o.x && y == o.y && z == o.z;
  }
};

struct HashKey3Hash {
  size_t operator()(const HashKey3 &k) const {
    return size_t((k.x * 73856093) ^ (k.y * 19349663) ^ (k.z * 83492791));
  }
};

HashKey3 CellKey(const Vec3f &p, float cellSize) {
  return HashKey3{int(std::floor(p[0] / cellSize)), int(std::floor(p[1] / cellSize)),
                  int(std::floor(p[2] / cellSize))};
}

// out of bounds reads as VOX_CONTAINER: it is exterior to even the padded
// grid, i.e. more exterior than the exterior InvertContainer already wrote.
uint8_t ClassAt(const Array3D8u &classGrid, const Vec3f &origin, float dx,
                const Vec3f &p) {
  Vec3f rel = (1.0f / dx) * (p - origin);
  int xi = int(std::floor(rel[0]));
  int yi = int(std::floor(rel[1]));
  int zi = int(std::floor(rel[2]));
  Vec3u size = classGrid.GetSize();
  if (xi < 0 || yi < 0 || zi < 0 || xi >= int(size[0]) || yi >= int(size[1])
      || zi >= int(size[2])) {
    return VOX_CONTAINER;
  }
  return classGrid(unsigned(xi), unsigned(yi), unsigned(zi));
}

struct MarchResult {
  bool valid = false;
  float skipDist = 0.0f; // cm of leading container/inner run.
  float depth = 0.0f;    // cm of free run after the skip.
  uint8_t hitClass = VOX_FREE;
};

// skips a leading run of VOX_CONTAINER/VOX_INNER capped at maxSkipVoxels,
// then counts free voxels up to maxProbeDepth. Invalid if the cap is hit
// while still blocked, i.e. dir does not point toward free space.
MarchResult MarchInward(const Vec3f &start, const Vec3f &dir,
                        const Array3D8u &classGrid, const Vec3f &origin,
                        float dx, unsigned maxSkipVoxels, float maxProbeDepth) {
  MarchResult res;
  auto posAt = [&](unsigned k) { return start + (dx * float(k)) * dir; };
  unsigned k = 0;
  uint8_t cls = ClassAt(classGrid, origin, dx, posAt(k));
  while ((cls == VOX_CONTAINER || cls == VOX_INNER) && k < maxSkipVoxels) {
    k++;
    cls = ClassAt(classGrid, origin, dx, posAt(k));
  }
  if (cls == VOX_CONTAINER || cls == VOX_INNER) {
    return res;
  }
  res.skipDist = dx * float(k);
  unsigned maxProbeVoxels = unsigned(maxProbeDepth / dx) + 1;
  unsigned m = k;
  while (cls == VOX_FREE && (m - k) < maxProbeVoxels) {
    m++;
    cls = ClassAt(classGrid, origin, dx, posAt(m));
  }
  res.valid = true;
  res.depth = dx * float(m - k);
  res.hitClass = cls;
  return res;
}

void FillRayFromMarch(SurfaceRay &ray, const MarchResult &m, const Vec3f &dir,
                      const CoverageParams &params) {
  ray.dir = dir;
  ray.valid = true;
  ray.depth = m.depth;
  ray.hitClass = m.hitClass;
  ray.open = ray.depth > params.holeDepth;
  ray.hitPoint = ray.origin + (m.skipDist + m.depth) * dir;
  ray.mouth = ray.origin + (m.skipDist + params.berryRadius + params.dx) * dir;
}

void RecomputePatchAggregates(const CoverageField &field, CoveragePatch &patch) {
  patch.openCount = 0;
  patch.meanOpenDepth = 0.0f;
  patch.maxOpenDepth = 0.0f;
  for (unsigned &c : patch.hitClassCounts) {
    c = 0;
  }
  float depthSum = 0.0f;
  for (unsigned idx : patch.rayIdx) {
    const SurfaceRay &ray = field.rays[idx];
    if (!ray.valid) {
      continue;
    }
    if (ray.open) {
      patch.openCount++;
      depthSum += ray.depth;
      patch.maxOpenDepth = std::max(patch.maxOpenDepth, ray.depth);
    }
    if (ray.hitClass == VOX_ITEM_BIG || ray.hitClass == VOX_ITEM_SMALL) {
      patch.hitClassCounts[ray.hitClass]++;
    }
  }
  if (patch.openCount > 0) {
    patch.meanOpenDepth = depthSum / float(patch.openCount);
  }
}

void AssignPatches(CoverageField &field, float patchSize) {
  std::unordered_map<HashKey3, unsigned, HashKey3Hash> keyToPatch;
  field.patches.clear();
  for (unsigned i = 0; i < field.rays.size(); i++) {
    SurfaceRay &ray = field.rays[i];
    HashKey3 key = CellKey(ray.origin, patchSize);
    auto it = keyToPatch.find(key);
    unsigned pid;
    if (it == keyToPatch.end()) {
      pid = unsigned(field.patches.size());
      field.patches.push_back(CoveragePatch());
      keyToPatch[key] = pid;
    } else {
      pid = it->second;
    }
    ray.patchId = int(pid);
    field.patches[pid].rayIdx.push_back(i);
    field.patches[pid].center += ray.origin;
  }
  for (CoveragePatch &patch : field.patches) {
    if (!patch.rayIdx.empty()) {
      patch.center = (1.0f / float(patch.rayIdx.size())) * patch.center;
    }
    RecomputePatchAggregates(field, patch);
  }
}

// each valid ray links to the other valid rays within 1.5x raySpacing,
// found via a hash at raySpacing resolution -- finer than patchSize, so a
// ray's neighbors are its immediate geometric neighbors, not its patchmates.
void BuildRayAdjacency(CoverageField &field, float raySpacing) {
  float cellSize = std::max(raySpacing, 1e-3f);
  float neighborRadius = 1.5f * raySpacing;
  float r2 = neighborRadius * neighborRadius;

  std::unordered_map<HashKey3, std::vector<unsigned>, HashKey3Hash> grid;
  for (unsigned i = 0; i < field.rays.size(); i++) {
    if (field.rays[i].valid) {
      grid[CellKey(field.rays[i].origin, cellSize)].push_back(i);
    }
  }

  field.rayNeighbors.assign(field.rays.size(), {});
  for (unsigned i = 0; i < field.rays.size(); i++) {
    if (!field.rays[i].valid) {
      continue;
    }
    HashKey3 base = CellKey(field.rays[i].origin, cellSize);
    for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          auto it = grid.find(HashKey3{base.x + dx, base.y + dy, base.z + dz});
          if (it == grid.end()) {
            continue;
          }
          for (unsigned j : it->second) {
            if (j == i) {
              continue;
            }
            if ((field.rays[j].origin - field.rays[i].origin).norm2() <= r2) {
              field.rayNeighbors[i].push_back(j);
            }
          }
        }
      }
    }
  }
}

void ClassifyRimRay(CoverageField &field, unsigned idx) {
  SurfaceRay &ray = field.rays[idx];
  ray.rim = false;
  if (!ray.valid || !ray.open) {
    return;
  }
  for (unsigned j : field.rayNeighbors[idx]) {
    if (field.rays[j].valid && field.rays[j].depth <= field.params.holeDepth) {
      ray.rim = true;
      return;
    }
  }
}

void ClassifyRim(CoverageField &field) {
  for (unsigned i = 0; i < field.rays.size(); i++) {
    ClassifyRimRay(field, i);
  }
}

std::vector<Vec3f> OrthoDirections26() {
  std::vector<Vec3f> dirs;
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      for (int z = -1; z <= 1; z++) {
        if (x == 0 && y == 0 && z == 0) {
          continue;
        }
        float fx = float(x), fy = float(y), fz = float(z);
        Vec3f d(fx, fy, fz);
        d.normalize();
        dirs.push_back(d);
      }
    }
  }
  return dirs;
}

} // namespace

CoverageField BuildCoverageField(const TrigMesh &container,
                                 const Array3D8u &classGrid,
                                 const Vec3f &gridOrigin,
                                 const CoverageParams &params) {
  PROFILE_SCOPE("coverage.build");
  CoverageField field;
  field.params = params;

  std::vector<SamplePoint> samplePts;
  SamplePoints(container, params.raySpacing, samplePts);

  field.rays.reserve(samplePts.size());
  unsigned flipped = 0, invalid = 0;
  {
    PROFILE_SCOPE("coverage.march");
    for (const SamplePoint &sp : samplePts) {
      Vec3f n = sp.n;
      if (n.norm() < 1e-6f) {
        continue;
      }
      n.normalize();

      SurfaceRay ray;
      ray.origin = sp.x;

      Vec3f dirA = -n;
      MarchResult mA = MarchInward(sp.x, dirA, classGrid, gridOrigin, params.dx,
                                   params.wallSkipVoxels, params.maxProbeDepth);
      if (mA.valid) {
        FillRayFromMarch(ray, mA, dirA, params);
      } else {
        MarchResult mB = MarchInward(sp.x, n, classGrid, gridOrigin, params.dx,
                                     params.wallSkipVoxels, params.maxProbeDepth);
        if (mB.valid) {
          FillRayFromMarch(ray, mB, n, params);
          flipped++;
        } else {
          invalid++;
        }
      }
      field.rays.push_back(ray);
    }
  }
  field.flippedRayCount = flipped;
  field.invalidRayCount = invalid;

  AssignPatches(field, params.patchSize);
  BuildRayAdjacency(field, params.raySpacing);
  ClassifyRim(field);
  return field;
}

std::vector<unsigned> FindNeighborPatches(const CoverageField &field,
                                          unsigned patchId, float radius) {
  std::vector<unsigned> out;
  if (patchId >= field.patches.size()) {
    return out;
  }
  out.push_back(patchId);
  Vec3f center = field.patches[patchId].center;
  for (unsigned i = 0; i < field.patches.size(); i++) {
    if (i == patchId) {
      continue;
    }
    if ((field.patches[i].center - center).norm() <= radius) {
      out.push_back(i);
    }
  }
  return out;
}

void ReprobePatch(CoverageField &field, unsigned patchId,
                  const Array3D8u &classGrid, const Vec3f &gridOrigin) {
  PROFILE_SCOPE("coverage.reprobe");
  if (patchId >= field.patches.size()) {
    return;
  }
  CoveragePatch &patch = field.patches[patchId];
  for (unsigned idx : patch.rayIdx) {
    SurfaceRay &ray = field.rays[idx];
    if (!ray.valid) {
      continue;
    }
    MarchResult m = MarchInward(ray.origin, ray.dir, classGrid, gridOrigin,
                                field.params.dx, field.params.wallSkipVoxels,
                                field.params.maxProbeDepth);
    if (!m.valid) {
      ray.valid = false;
      continue;
    }
    FillRayFromMarch(ray, m, ray.dir, field.params);
  }
  for (unsigned idx : patch.rayIdx) {
    ClassifyRimRay(field, idx);
  }
  RecomputePatchAggregates(field, patch);
}

CoverageReport BuildCoverageReport(const CoverageField &field, float gapingHoleDepth) {
  CoverageReport rep;
  rep.totalRays = unsigned(field.rays.size());
  rep.invalidRays = field.invalidRayCount;
  rep.flippedRays = field.flippedRayCount;
  for (const SurfaceRay &ray : field.rays) {
    if (!ray.valid) {
      continue;
    }
    rep.validRays++;
    if (ray.open) {
      rep.openRays++;
    }
  }
  rep.openFraction = rep.validRays > 0 ? float(rep.openRays) / float(rep.validRays) : 0.0f;
  PROFILE_COUNT("count.rays_open", rep.openRays);

  rep.patchCount = unsigned(field.patches.size());
  std::vector<float> openFracs;
  openFracs.reserve(field.patches.size());
  unsigned nonEmptyPatches = 0;
  for (const CoveragePatch &patch : field.patches) {
    if (patch.rayIdx.empty()) {
      continue;
    }
    nonEmptyPatches++;
    float frac = float(patch.openCount) / float(patch.rayIdx.size());
    openFracs.push_back(frac);
    if (patch.openCount == 0) {
      rep.patchesFullyCovered++;
    }
    if (patch.maxOpenDepth > gapingHoleDepth) {
      rep.gapingHolePatches++;
    }
  }
  rep.gapingHoleFraction =
      nonEmptyPatches > 0 ? float(rep.gapingHolePatches) / float(nonEmptyPatches) : 0.0f;
  std::sort(openFracs.begin(), openFracs.end());
  if (!openFracs.empty()) {
    rep.openFractionP50 = openFracs[openFracs.size() / 2];
    size_t p90i = size_t(float(openFracs.size()) * 0.9f);
    rep.openFractionP90 = openFracs[std::min(openFracs.size() - 1, p90i)];
  }

  rep.depthBucketWidth = std::max(0.5f, field.params.holeDepth);
  unsigned numBuckets = unsigned(std::ceil(field.params.maxProbeDepth / rep.depthBucketWidth)) + 1;
  rep.depthHistogram.assign(numBuckets, 0);
  for (const SurfaceRay &ray : field.rays) {
    if (!ray.valid || !ray.open) {
      continue;
    }
    unsigned b = unsigned(ray.depth / rep.depthBucketWidth);
    if (b >= numBuckets) {
      b = numBuckets - 1;
    }
    rep.depthHistogram[b]++;
  }
  return rep;
}

CoverageReport BuildOrthoCoverageReport(const Array3D8u &classGrid,
                                        const Vec3f &gridOrigin,
                                        const OrthoCoverageParams &params) {
  PROFILE_SCOPE("coverage.march");
  Vec3u gsize = classGrid.GetSize();
  Vec3f extent(float(gsize[0]) * params.dx, float(gsize[1]) * params.dx,
              float(gsize[2]) * params.dx);
  Vec3f center = gridOrigin + 0.5f * extent;
  float boxRadius = 0.5f * extent.norm();
  // generous: must cross the full exterior plus the wall from a start
  // point outside the bounding sphere, not just the wall itself.
  unsigned maxSkipVoxels = unsigned(3.0f * boxRadius / params.dx) + 4;

  CoverageField field;
  field.params.holeDepth = params.holeDepth;
  field.params.maxProbeDepth = params.maxProbeDepth;
  field.params.dx = params.dx;

  unsigned side = std::max(1u, unsigned(std::sqrt(float(params.maxRaysPerDirection))));
  std::vector<Vec3f> dirs = OrthoDirections26();
  for (const Vec3f &d : dirs) {
    Vec3f up = std::fabs(d[1]) < 0.9f ? Vec3f(0, 1, 0) : Vec3f(1, 0, 0);
    Vec3f u = d.cross(up);
    u.normalize();
    Vec3f v = d.cross(u);
    v.normalize();

    float step = params.spacing > 0.0f ? params.spacing : (2.0f * boxRadius / float(side));
    unsigned n = unsigned(2.0f * boxRadius / step) + 1;
    n = std::min(n, side);
    for (unsigned i = 0; i < n; i++) {
      for (unsigned j = 0; j < n; j++) {
        float su = -boxRadius + step * (float(i) + 0.5f);
        float sv = -boxRadius + step * (float(j) + 0.5f);
        Vec3f start = center + su * u + sv * v - (boxRadius * 1.5f) * d;

        SurfaceRay ray;
        ray.origin = start;
        MarchResult m = MarchInward(start, d, classGrid, gridOrigin, params.dx,
                                    maxSkipVoxels, params.maxProbeDepth);
        if (m.valid) {
          FillRayFromMarch(ray, m, d, field.params);
        }
        field.rays.push_back(ray);
      }
    }
  }
  // no patches for the independence check: only the overall open fraction
  // matters here, never per-patch targeting.
  CoverageReport rep = BuildCoverageReport(field);
  rep.patchCount = 0;
  rep.patchesFullyCovered = 0;
  rep.openFractionP50 = 0.0f;
  rep.openFractionP90 = 0.0f;
  return rep;
}

float ComputeVisibleBerryFraction(const CoverageField &field,
                                  const BerryVisibilityInput &input) {
  if (input.smallInstanceCenters.empty()) {
    return -1.0f;
  }
  float cellSize = std::max(0.1f, input.matchRadius);
  std::unordered_map<HashKey3, std::vector<unsigned>, HashKey3Hash> grid;
  for (unsigned i = 0; i < input.smallInstanceCenters.size(); i++) {
    grid[CellKey(input.smallInstanceCenters[i], cellSize)].push_back(i);
  }

  std::vector<bool> seen(input.smallInstanceCenters.size(), false);
  float matchRadius2 = input.matchRadius * input.matchRadius;
  for (const SurfaceRay &ray : field.rays) {
    if (!ray.valid || ray.hitClass != VOX_ITEM_SMALL) {
      continue;
    }
    HashKey3 base = CellKey(ray.hitPoint, cellSize);
    for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          auto it = grid.find(HashKey3{base.x + dx, base.y + dy, base.z + dz});
          if (it == grid.end()) {
            continue;
          }
          for (unsigned idx : it->second) {
            if (seen[idx]) {
              continue;
            }
            if ((input.smallInstanceCenters[idx] - ray.hitPoint).norm2() <= matchRadius2) {
              seen[idx] = true;
            }
          }
        }
      }
    }
  }
  unsigned count = 0;
  for (bool s : seen) {
    if (s) {
      count++;
    }
  }
  return float(count) / float(seen.size());
}

unsigned CountRaysBlocked(const CoverageField &field, const std::vector<unsigned> &rayIdx,
                         const VoxFootprint &footprint, const Vec3f &candidatePos, float dx) {
  unsigned count = 0;
  Vec3u fSize = footprint.vox.GetSize();
  for (unsigned idx : rayIdx) {
    const SurfaceRay &ray = field.rays[idx];
    if (!ray.valid || !ray.open) {
      continue;
    }
    Vec3f seg = ray.hitPoint - ray.mouth;
    float segLen = seg.norm();
    unsigned steps = unsigned(segLen / dx) + 1;
    bool blocked = false;
    for (unsigned s = 0; s <= steps && !blocked; s++) {
      float t = steps > 0 ? float(s) / float(steps) : 0.0f;
      Vec3f wp = ray.mouth + t * seg;
      Vec3f local = wp - candidatePos - footprint.localOrigin;
      int fx = int(std::floor(local[0] / dx));
      int fy = int(std::floor(local[1] / dx));
      int fz = int(std::floor(local[2] / dx));
      if (fx < 0 || fy < 0 || fz < 0 || fx >= int(fSize[0]) || fy >= int(fSize[1])
          || fz >= int(fSize[2])) {
        continue;
      }
      if (footprint.vox(unsigned(fx), unsigned(fy), unsigned(fz)) != 0) {
        blocked = true;
      }
    }
    if (blocked) {
      count++;
    }
  }
  return count;
}

void SaveOpenMouthsObj(const std::string &filename, const CoverageField &field) {
  std::vector<Vec3f> pts;
  for (const SurfaceRay &ray : field.rays) {
    if (ray.valid && ray.open) {
      pts.push_back(ray.mouth);
    }
  }
  SaveVec3fObj(filename, pts);
}

std::string CoverageReport::toString() const {
  std::ostringstream oss;
  oss << "rays " << totalRays << " (valid " << validRays << ", invalid "
      << invalidRays << ", flipped " << flippedRays << ")\n";
  oss << "open ray fraction " << openFraction << " (" << openRays << "/"
      << validRays << ")\n";
  if (patchCount > 0) {
    oss << "patches " << patchCount << ", fully covered " << patchesFullyCovered
        << ", open fraction p50 " << openFractionP50 << " p90 " << openFractionP90
        << "\n";
    oss << "gaping holes (bigger than gapingHoleDepth): " << gapingHolePatches
        << " patches (" << gapingHoleFraction << ")\n";
  }
  oss << "open ray depth histogram (bucket " << depthBucketWidth << " cm):";
  for (size_t i = 0; i < depthHistogram.size(); i++) {
    oss << " " << depthHistogram[i];
  }
  oss << "\n";
  if (visibleBerryFraction >= 0.0f) {
    oss << "visible berry fraction " << visibleBerryFraction << "\n";
  }
  return oss.str();
}
