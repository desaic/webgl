#include "PocketPlanner.h"

#include <algorithm>
#include <numeric>

namespace {

// smallest stride >= n/3 that is coprime with n, so visiting order_[(i*stride)
// % n] for i in [0,n) is a full permutation and consecutive visits jump far
// in the x-sorted order handed in. Deterministic: no RNG.
unsigned CoprimeStride(unsigned n) {
  if (n <= 2) {
    return 1;
  }
  unsigned start = std::max(1u, n / 3);
  for (unsigned s = start; s < start + n; s++) {
    unsigned stride = 1 + (s % (n - 1));
    if (std::gcd(stride, n) == 1) {
      return stride;
    }
  }
  return 1;
}

// true if any open ray in the patch has a covered neighbor -- i.e. this
// patch sits at the edge of material that already exists, not out in open
// space with nothing nearby yet. Filling a container from empty (or from
// sparse coverage) is an accretion process: a patch with nothing next to it
// cannot succeed no matter how many attempts it gets (Nudge has no nearby
// contact to pull toward), so trying it before its neighbors have anything
// in them just burns its patchMaxFails budget for free. Growing outward
// from whatever already exists, one ring at a time, is what makes those
// attempts land on patches that can actually succeed.
bool HasRimRay(const CoverageField &field, const CoveragePatch &patch) {
  for (unsigned idx : patch.rayIdx) {
    const SurfaceRay &ray = field.rays[idx];
    if (ray.valid && ray.open && ray.rim) {
      return true;
    }
  }
  return false;
}

} // namespace

PocketPlanner::PocketPlanner(const CoverageField &field, const PocketPlannerParams &params,
                             std::vector<PocketKindInfo> smallKinds)
    : field_(field), params_(params), kinds_(std::move(smallKinds)) {
  state_.assign(field_.patches.size(), PocketPatchState());
}

void PocketPlanner::RebuildOrder() {
  order_.clear();
  std::vector<unsigned> eligible;
  for (unsigned p = 0; p < field_.patches.size(); p++) {
    // maxOpenDepth <= deepPocketThreshold covers both "nothing open here"
    // (maxOpenDepth == 0) and "open but only a shallow slope" in one check:
    // neither is a pocket worth spending an attempt on.
    if (field_.patches[p].rayIdx.empty()
        || field_.patches[p].maxOpenDepth <= params_.deepPocketThreshold) {
      continue;
    }
    if (state_[p].consecutiveFails >= params_.patchMaxFails) {
      continue;
    }
    eligible.push_back(p);
  }
  if (eligible.empty()) {
    return;
  }

  // frontier (has a rim ray) always outranks interior (none): a fully
  // isolated patch only gets tried once every frontier patch this round
  // is placed, retired, or exhausted, so growth proceeds outward from
  // whatever already exists instead of scattering attempts randomly
  // across a still-mostly-empty container.
  std::vector<float> priority(eligible.size());
  for (size_t i = 0; i < eligible.size(); i++) {
    const CoveragePatch &patch = field_.patches[eligible[i]];
    float frontierBonus = HasRimRay(field_, patch) ? 1.0e6f : 0.0f;
    priority[i] = frontierBonus + float(patch.openCount) * patch.meanOpenDepth;
  }
  std::vector<unsigned> rank(eligible.size());
  std::iota(rank.begin(), rank.end(), 0u);
  std::sort(rank.begin(), rank.end(),
           [&](unsigned a, unsigned b) { return priority[a] > priority[b]; });

  unsigned tiers = std::max(1u, params_.priorityTiers);
  unsigned tierSize = std::max(1u, unsigned(eligible.size() + tiers - 1) / tiers);
  std::vector<unsigned> topTier;
  for (unsigned i = 0; i < tierSize && i < rank.size(); i++) {
    topTier.push_back(eligible[rank[i]]);
  }

  // strided walk along the container's long x axis: sort the top tier by
  // patch center x, then step through it with a coprime stride so
  // consecutive placements land far apart instead of draining one region.
  std::sort(topTier.begin(), topTier.end(), [&](unsigned a, unsigned b) {
    return field_.patches[a].center[0] < field_.patches[b].center[0];
  });
  unsigned n = unsigned(topTier.size());
  unsigned stride = CoprimeStride(n);
  for (unsigned i = 0; i < n; i++) {
    order_.push_back(topTier[(i * stride) % n]);
  }
}

int PocketPlanner::NextPatch() {
  if (order_.empty()) {
    RebuildOrder();
  }
  if (order_.empty()) {
    return -1;
  }
  unsigned p = order_.front();
  order_.pop_front();
  return int(p);
}

int PocketPlanner::PickTargetRay(unsigned patchId) const {
  if (patchId >= field_.patches.size()) {
    return -1;
  }
  const CoveragePatch &patch = field_.patches[patchId];
  int bestRim = -1;
  float bestRimDepth = -1.0f;
  int bestInterior = -1;
  float bestInteriorDepth = -1.0f;
  for (unsigned idx : patch.rayIdx) {
    const SurfaceRay &ray = field_.rays[idx];
    if (!ray.valid || !ray.open) {
      continue;
    }
    if (ray.rim) {
      if (ray.depth > bestRimDepth) {
        bestRimDepth = ray.depth;
        bestRim = int(idx);
      }
    } else if (ray.depth > bestInteriorDepth) {
      bestInteriorDepth = ray.depth;
      bestInterior = int(idx);
    }
  }
  return bestRim >= 0 ? bestRim : bestInterior;
}

int PocketPlanner::PickKind(unsigned targetRayIdx) const {
  if (kinds_.empty() || targetRayIdx >= field_.rays.size()) {
    return -1;
  }
  const SurfaceRay &ray = field_.rays[targetRayIdx];
  float nearestCovered = -1.0f;
  if (targetRayIdx < field_.rayNeighbors.size()) {
    for (unsigned j : field_.rayNeighbors[targetRayIdx]) {
      const SurfaceRay &nb = field_.rays[j];
      if (nb.valid && nb.depth <= field_.params.holeDepth) {
        float d = (nb.origin - ray.origin).norm();
        if (nearestCovered < 0.0f || d < nearestCovered) {
          nearestCovered = d;
        }
      }
    }
  }
  // no covered neighbor found (patch wider than a patch, H2's fallback to
  // the deepest interior ray already covers the targeting; here just
  // default to the widest small kind so the search isn't over-constrained).
  float mouthDiameter = nearestCovered >= 0.0f ? 2.0f * nearestCovered : params_.smallMax;
  mouthDiameter = std::max(params_.smallMin, std::min(mouthDiameter, params_.smallMax));

  auto useCountOf = [&](unsigned itemIndex) {
    auto it = kindUseCount_.find(itemIndex);
    return it == kindUseCount_.end() ? 0u : it->second;
  };

  int best = -1;
  float bestExtent = -1.0f;
  unsigned bestUse = ~0u;
  for (const PocketKindInfo &kind : kinds_) {
    if (kind.maxExtent > mouthDiameter) {
      continue;
    }
    unsigned use = useCountOf(kind.itemIndex);
    if (totalPlaced_ > 0 && float(use) / float(totalPlaced_ + 1) > params_.kindShareCap) {
      continue; // over its share cap; a kind under cap should win instead.
    }
    if (kind.maxExtent > bestExtent || (kind.maxExtent == bestExtent && use < bestUse)) {
      bestExtent = kind.maxExtent;
      bestUse = use;
      best = int(kind.itemIndex);
    }
  }
  if (best >= 0) {
    return best;
  }
  // every fitting kind is over its share cap: fall back to least-used fit
  // so a narrow mouth still gets a placement rather than none.
  for (const PocketKindInfo &kind : kinds_) {
    if (kind.maxExtent > mouthDiameter) {
      continue;
    }
    unsigned use = useCountOf(kind.itemIndex);
    if (kind.maxExtent > bestExtent || (kind.maxExtent == bestExtent && use < bestUse)) {
      bestExtent = kind.maxExtent;
      bestUse = use;
      best = int(kind.itemIndex);
    }
  }
  return best;
}

void PocketPlanner::RecordFail(unsigned patchId) {
  if (patchId >= state_.size()) {
    return;
  }
  state_[patchId].consecutiveFails++;
  if (state_[patchId].consecutiveFails < params_.patchMaxFails) {
    order_.push_back(patchId); // back of the current pass, not retried immediately.
  }
}

void PocketPlanner::RecordSuccess(unsigned patchId, unsigned kindItemIndex) {
  if (patchId < state_.size()) {
    state_[patchId].consecutiveFails = 0;
    state_[patchId].berriesPlaced++;
  }
  kindUseCount_[kindItemIndex]++;
  totalPlaced_++;
}
