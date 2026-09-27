#pragma once

#include "MeshInfo.h"
#include "PackingConfig.h"

#include <string>

/// one-off helpers kept out of main.cpp. not part of the packing pipeline.

/// marching cubes an inset surface of the container and saves it.
void MakeInnerMesh(const PackingConfig &cfg, float insetVoxels = 4.0f);

/// dumps sample points before and after moving them inward, plus the
/// item sdf isosurface, for inspecting contact sample coverage.
void DebugPointSampling(MeshInfo &meshInfo, const std::string &outputFolder);

/// drops a single item into the container and saves its trajectory.
void DebugNudge(const PackingConfig &cfg);

/// loads the scene and its resume pack, voxelizes the placed items at
/// voxelSize, and writes the occupancy grid as a cube mesh to
/// <outputFolder>/item_voxels.obj. does not pack anything, so it is the
/// quickest way to check that the voxelization lines up with the item meshes.
void DebugItemVoxels(const PackingConfig &cfg, float voxelSize = 0.25f);

/// loads the scene and its resume pack, shrink-wraps the placed items, and
/// writes <outputFolder>/shrinkwrap_debug.obj alongside item_meshes.obj, so the
/// hull can be checked against the geometry it wraps without running a pack.
void DebugShrinkWrap(const PackingConfig &cfg, float shrinkRadius = 1.0f,
                     float voxelSize = 0.25f);

/// loads the scene and its resume pack, computes VoidField (see
/// PackShrinkWrap.h), and writes its iso-0 surface to
/// <outputFolder>/voidfield_debug.obj alongside item_meshes.obj -- the
/// boundary of "worth filling" space (gaps between placed fruit inside the
/// shrinkwrap hull's own silhouette; container-independent), for
/// eyeballing before any threshold/local-maxima code consumes the field.
/// plan.txt CANDIDATE SPOT MANAGEMENT, step 5.
void DebugVoidField(const PackingConfig &cfg, float shrinkRadius = 1.0f,
                    float voxelSize = 0.25f);

/// loads the scene and its resume pack, computes VoidField, then runs
/// ExtractVoidSpots (threshold + morphological open + local maxima, see
/// PackShrinkWrap.h and plan.txt CANDIDATE SPOT MANAGEMENT) and writes:
///   <outputFolder>/voidspots_blobs_debug.obj  -- the cleaned mask's own
///     surface, one isolated chunk per real crevice/void, outer sculpture
///     surface absent by construction (thresholded away).
///   <outputFolder>/voidspots_points_debug.obj -- the accepted local maxima
///     as a point cloud, the actual target positions a later TryFillSpot
///     would use. Each spot's radius is also printed to the console so it
///     can be cross-checked against the blob it sits in.
void DebugVoidSpots(const PackingConfig &cfg, float shrinkRadius = 1.0f,
                    float voxelSize = 0.25f, unsigned openRadiusVoxels = 1);

/// NOTE: there used to be a separate DebugCreviceField here, for a
/// container-relative "crevice field" (min(containerDepth, envelopeDist)).
/// That field, and the wall-relative version of Phase 2 it was built for,
/// were both wrong -- corrected per the user: Phase 2 has nothing to do
/// with the container wall, it is small fruit resting on fruit-fruit
/// crevices only, which is exactly what VoidField already describes (see
/// PackShrinkWrap.h). DebugVoidSpots above IS now Phase 2's own debug
/// view too, since PackFillCrevices consumes the identical VoidField/
/// ExtractVoidSpots output PackFillVoids does (see PackingDriver.cpp).

/// step 8: assembles ClaimGrid + TryFillSpot into the old nominal-size
/// fill loop (PackFillVoids, pre-rename Phase 2, removed from the real
/// pipeline but kept here for debugging), run once with the nominal fruit
/// size only (kindsBySize is just every item in
/// the scene, since no second scale tier exists yet -- see plan.txt
/// MULTI-SCALE FILL, the M1 histogram already said it is not worth building
/// yet). Loads the resume pack, extracts spots widest-first (steps 4-5),
/// and calls TryFillSpot on each unclaimed one in that order. Saves the
/// updated pack to <outputFolder>/pack_after_fill.txt and the merged item
/// meshes to item_meshes.obj (overwriting the pre-fill dump) so the result
/// can be diffed against voidspots_points_debug.obj from the same round.
void DebugFillVoids(const PackingConfig &cfg, float shrinkRadius = 1.0f,
                    float voxelSize = 0.25f, unsigned openRadiusVoxels = 1,
                    float clearance = 0.1f);

/// loads packFile (relative to cfg.dataDir, same convention as
/// cfg.resumePackFile) and independently checks instances [rangeStart,
/// rangeEnd) for container/overlap violations, using PackValidator -- a
/// separate, coarser (cfg.dx) occupancy grid built by real voxelization,
/// not the fine EnvelopeField/VoidField TryFillSpot itself checked against.
/// Each instance is validated against a grid built from every OTHER
/// instance (itself excluded, same pattern as BenchValidateSelfTest's
/// "real placement from pack file" case), so a false self-overlap cannot
/// hide a real one. Prints one line per instance plus an aggregate summary.
void DebugValidatePlacementRange(const PackingConfig &cfg,
                                 const std::string &packFile,
                                 size_t rangeStart, size_t rangeEnd);

/// loads cfg.resumePackFile, builds the crevice baseline from its first
/// numBaseInstances instances (the Phase 1 pack), and scores every later
/// instance with ComputeCreviceCoverage. Writes
/// <outputFolder>/<outPrefix>_coverage.txt and <outPrefix>_unnecessary.obj.
void DebugCreviceCoverage(const PackingConfig &cfg, size_t numBaseInstances,
                          const std::string &outPrefix);
