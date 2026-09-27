#pragma once

#include "Array3D.h"
#include "TrigMesh.h"
#include <string>
#include <vector>

class PackingScene;

/// voxelizes every placed instance into one solid occupancy grid at the given
/// voxel size. only the items are filled, so unlike scene.bg.vox the space
/// outside the container reads as empty. the grid is sized to the union of the
/// instance bounding boxes plus 2 voxels of padding, independent of scene.dx
/// and the scene world origin.
/// @param outOrigin min corner of voxel (0,0,0), same convention as
///                  VoxConf::origin: voxel (i,j,k) covers the cube
///                  [outOrigin + idx*voxelSize, outOrigin + (idx+1)*voxelSize].
Array3D8u VoxelizeItems(PackingScene &scene, float voxelSize, Vec3f &outOrigin);

/// computes the shrinkwrap distance field from the placed instances:
/// voxelizes them, flood-fills outside, shrink-wraps with the given
/// radius. positive outside the packed+closed hull, non-positive inside
/// it. dist stores raw values in units of distUnit (physical = dist *
/// distUnit), same convention as AdapDF. Empty scene -> dist left
/// unallocated (GetSize() == (0,0,0)). The standalone primitive --
/// ComputeShrinkWrapMesh/SaveShrinkWrapMesh below are debug helpers that
/// consume an already-computed field/mesh, they do not compute one.
/// rawOut, if non-null, receives a copy of the field as it stood right
/// before closing (plain unsigned distance to nearest actual fruit
/// surface) -- see ComputeVoidField, which needs both fields on one grid.
void ComputeShrinkWrapDistField(PackingScene &scene, float shrinkRadius,
                                float voxelSize, Array3D<short> &dist,
                                Vec3f &origin, float &distUnit,
                                Array3D<short> *rawOut = nullptr);

/// debug helper: marching-cubes an already-computed shrinkwrap field (see
/// ComputeShrinkWrapDistField) at iso 0.
TrigMesh ComputeShrinkWrapMesh(const Array3D<short> &dist, const Vec3f &origin,
                               float voxelSize, float distUnit);

/// debug helper: saves an already-computed shrinkwrap mesh (see
/// ComputeShrinkWrapMesh) to an obj file.
void SaveShrinkWrapMesh(const TrigMesh &mesh, const std::string &filename);

/// "worth filling" field, on the shrinkwrap envelope's own grid:
///   voidField(x) = rawEnvelope(x)   where closedEnvelope(x) <= 0 (x is
///                                   inside the manufactured hull -- solid
///                                   fruit, or a gap small enough that
///                                   closing already fills it in)
///                = blocked         everywhere else (x is outside the
///                                   hull's own silhouette entirely)
/// i.e. filled voxels subtracted from the shrinkwrap field, restricted to
/// the hull's own interior. This deliberately ignores the container: a
/// point can be far from the container wall and still read as blocked here
/// if it is outside the pack's own hull silhouette, which is exactly what
/// is wanted -- this field looks for gaps BETWEEN placed fruit (used by
/// the old, now-removed nominal-size fill pass and by ComputePackQuality's
/// openVolume; Phase 2/PackFillCrevices uses ComputeShrinkwrapField
/// instead, below), not open space between the pack and the container
/// wall (a different, much
/// larger-scale problem; see plan.txt CORE IDEA, revised after visual
/// inspection showed the earlier container-SDF combination pulling in that
/// whole outer shell as "worth filling", which it is not, at this stage).
struct VoidField {
  Array3D<short> dist;
  Vec3f origin = {0.0f, 0.0f, 0.0f};
  float voxelSize = 0.25f;
  float distUnit = 0.01f;
};
VoidField ComputeVoidField(PackingScene &scene, float shrinkRadius = 1.0f,
                          float voxelSize = 0.25f);

/// hull interior minus actual fruit, eroded, then restricted to a thin
/// shell around the shrinkwrap surface's own zero crossing -- true
/// crevices only, no single-fruit inflation, no deep bulk interior:
///   1. voidMask(x) = closedField(x) <= 0 AND rawField(x) > 0 -- inside the
///      manufactured hull, but not inside any actual placed fruit.
///   2. erode voidMask by erodeVoxels. A single fruit's own concave
///      surface feature (a dimple) gets bridged by closing the same as a
///      real multi-fruit crevice mouth is, so voidMask alone cannot tell
///      them apart; erosion removes thin/shallow slivers like a dimple
///      while a genuinely wide multi-fruit crevice survives. NOT derived
///      from shrinkRadius -- closing nets close to zero growth over a
///      smooth convex surface (see plan.txt M2), so there is no uniform
///      shrinkRadius-thick shell to size this against; tune it directly.
///   3. keep only voxels within skinBandVoxels + erodeVoxels of
///      closedField(x)'s own zero crossing (the erosion in step 2 already
///      pulled every survivor at least erodeVoxels away from that
///      boundary, so the band is widened by the same amount to compensate)
///      -- attraction points belong on the manufactured surface, not
///      floating in whatever bulk void the erosion left behind deeper in
///      the interior.
/// Kept voxels hold rawField(x), the unsigned distance to the nearest
/// ACTUAL fruit surface (same value ExtractVoidSpots' local-maxima search
/// treats as the local crevice width); everywhere else is blocked
/// (-MAX_DIST sentinel), same shape VoidField uses, so ExtractVoidSpots
/// runs on this completely unchanged -- no marching cubes, no point
/// sampling, still a plain dense-grid operation.
VoidField ComputeShrinkwrapField(PackingScene &scene, float shrinkRadius = 1.0f,
                                 float voxelSize = 0.25f, unsigned erodeVoxels = 2,
                                 unsigned skinBandVoxels = 2);

/// plain unsigned distance to the nearest placed fruit surface, no closing
/// applied (unlike VoidField, nothing here is masked to a hull interior or
/// otherwise altered) -- for validating a NEW candidate pose against
/// EXISTING placed fruit at the same fine sub-voxel resolution spots were
/// found at. Deliberately not scene.bg.vox: that grid is built at the much
/// coarser cfg.dx via crude voxelize+corner-zero per instance (the same
/// kind of conservative dilation M2 fixed for the shrinkwrap field), so a
/// gap this field calls wide open can read as fully occupied in bg.vox at
/// 3x the voxel size -- exactly the mismatch that made every fill attempt
/// fail OccupiedFraction in TryFillSpot despite settling cleanly.
struct EnvelopeField {
  Array3D<short> dist;
  Vec3f origin = {0.0f, 0.0f, 0.0f};
  float voxelSize = 0.25f;
  float distUnit = 0.01f;
};
EnvelopeField ComputeEnvelopeField(PackingScene &scene, float voxelSize = 0.25f);


/// nearest-cell sample of a plain distance field (VoidField or
/// EnvelopeField) at an arbitrary world point, in physical units (already
/// multiplied by distUnit). Out-of-bounds returns a large positive value
/// (far/free), since being outside the grid this field was built on means
/// far from whatever the field tracks, not blocked by it.
float SampleDistField(const Array3D<short> &dist, const Vec3f &origin, float voxelSize,
                      float distUnit, const Vec3f &worldPos);

/// a candidate fill spot: pos is a local maximum of the cleaned void
/// distance field, radius is the field's value there (the inscribed-disk
/// radius at that point, i.e. half the local void width). See plan.txt
/// CANDIDATE SPOT MANAGEMENT.
struct VoidSpot {
  Vec3f pos = {0.0f, 0.0f, 0.0f};
  float radius = 0.0f;
};

/// result of ExtractVoidSpots: the accepted spots plus the intermediate
/// grids, kept around for debug visualization (blob mesh, etc).
struct VoidSpotResult {
  std::vector<VoidSpot> spots;
  // binary mask after threshold + morphological open (1 = kept as a real
  // gap, not crust/noise). Same grid as the VoidField this was built from.
  Array3D8u cleanedMask;
  // distance-to-cleanedMask's-own-boundary, valid (and used) only where
  // cleanedMask is 1. This, not the raw VoidField, is what spots are local
  // maxima of.
  Array3D<short> cleanDist;
};

/// Phase 2b/2c/CANDIDATE SPOT MANAGEMENT pipeline on an already-computed
/// VoidField:
///   1. threshold vf.dist > threshold into a binary mask (voxelSize+epsilon
///      per Phase 2b, NOT > 0 -- see plan.txt for why).
///   2. morphological open (erode then dilate by openRadiusVoxels) to kill
///      jagged crust/thin webs that survived the threshold.
///   3. distance transform to the cleaned mask's own boundary.
///   4. local maxima of that field (each voxel compared within a window
///      sized to its own value), then non-maximum suppression: sort by
///      value descending, drop a candidate into an already-accepted larger
///      one only if their inscribed disks already overlap (exact test, not
///      a tuned constant).
/// threshold and openRadiusVoxels are in the same physical units as
/// vf.voxelSize (cm); openRadiusVoxels is rounded to whole voxels.
VoidSpotResult ExtractVoidSpots(const VoidField &vf, float threshold,
                                unsigned openRadiusVoxels = 1);
