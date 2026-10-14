#pragma once

#include "PackingConfig.h"
#include "PackingPlan.h"
#include "PackingScene.h"

#include <string>
#include <vector>

/// reads only "v x y z" lines of an .obj; faces and edges are ignored.
/// @return false if the file cannot be opened.
bool LoadObjVertices(const std::string &path, std::vector<Vec3f> &out);

/// Covers the medium target points (cfg.MediumTargetsPath()) with a small set
/// of spheres grown from the existing pack, using radii derived from the
/// medium fruit kinds in step.names and cfg.sphereScales. Spheres are a proxy
/// only: nothing is placed in the scene. Writes sphere_cover.txt (count, then
/// "x y z r" per line) and sphere_cover_uncovered.obj to the output folder.
void PackSphereCover(PackingScene &scene, const PackingStep &step,
                     const PackingConfig &cfg);

/// discrete sphere radii for the medium kinds in step (sorted ascending).
std::vector<float> BuildSphereRadii(PackingScene &scene, const PackingStep &step,
                                    const PackingConfig &cfg);

/// fast entry for cfg.sphereCover: adds only the large fruit with center
/// x < cfg.sphereMaxX (and the exclusion mesh) to the scene for distance
/// queries, skipping the background voxelization and the full pack load, then
/// runs PackSphereCover.
void RunSphereCoverFast(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg);

/// minimal scene for the sphere cover: only the meshes it needs, the container
/// SDF and the exclusion mesh. use instead of BuildScene when cfg.sphereCover.
bool BuildSphereCoverScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg);
