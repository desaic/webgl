#include "PackingScene.h"
#include "AdapSDF.h"
#include "FastSweep.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "MarchingCubes.h"
#include "MeshConvo.h"
#include "MeshOps.h"
#include "PointSample.h"
#include "Quat4f.h"
#include "Stopwatch.h"
#include "TrigGrid.h"
#include "cpu_voxelizer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>

namespace fs = std::filesystem;

Vec3i roundf(const Vec3f &fvec) {
  return Vec3i(std::round(fvec[0]), std::round(fvec[1]), std::round(fvec[2]));
}

unsigned PackingScene::Put(unsigned itemIdx, const RigidTransform &tran) {
  TrigMesh inst = MakeTransformedMesh(items[itemIdx].mesh, tran);
  Box3f bbox = ComputeBBox(inst.v);
  // saved for broad phase.
  Box3f meshBox = bbox;
  bbox.vmin = AlignOriginToGrid(bbox.vmin, dx);

  VoxConf conf;
  conf.origin = ToArray(bbox.vmin);
  conf.unit = {dx, dx, dx};
  conf.gridSize = ComputeGridSize(bbox, dx, 1);

  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  VoxelizeMesh(inst, vox, conf);
  FloodOutside8u(vox, 1, 2);

  // e.g. if item origin is same as world origin, then offset is 0.
  Vec3i offset = roundf((1.0f / dx) * (conf.origin - WorldOrigin()));
  Union(bg, offset, vox);

  unsigned instId = instances.size();
  if (placed.size() != items.size()) {
    placed.resize(items.size());
  }
  if (itemIdx < placed.size()) {
    placed[itemIdx].push_back(tran);
  }
  instances.push_back(InstanceInfo(itemIdx, tran));
  broadPhase.Add(meshBox, instId);
  return instances.size() - 1u;
}

Vec3f PackingScene::ForceDirection(unsigned itemIdx,
                                   const Vec3f &gravity,
                                   float sdfFactor,
                                   const RigidTransform &tran) {
  Vec3f dir0 = gravity;
  // (0, 1)
  float dir0Weight = dir0.norm();
  // assume object is centered at origin in reference space.
  Vec3f sdfDir = sdf->GetCoarseGrad(tran.position);
  Vec3f dir = dir0 + (1 - dir0Weight) * (sdfFactor * sdfDir);
  dir.normalize();
  return dir;
}

static void ReverseWinding(TrigMesh &m) {
  for (size_t i = 0; i < m.t.size(); i += 3) {
    std::swap(m.t[i + 1], m.t[i + 2]);
  }
}

void PackingScene::InitDataStructures() {
  randAngles.resize(100);
  unsigned int seed = 123;
  std::mt19937 engine(seed);
  std::uniform_real_distribution<float> dist(0.0f, 6.2831853);
  std::uniform_int_distribution<int> intDistri(0);
  for (size_t i = 0; i < randAngles.size(); i++) {
    randAngles[i][0] = dist(engine);
    randAngles[i][1] = dist(engine);
    randAngles[i][2] = dist(engine);
  }

  for (unsigned i = 0; i < items.size(); i++) {
    nameToIndex[items[i].name] = i;
  }
  // SDF must be computed before reversing the container winding, since
  // AdapSDF bakes its sign convention from the original outward normals.
  ComputeContainerSDF();
  ReverseWinding(container.mesh);
  InitContainerGrids();
  InitRigidBodies();

  for (size_t i = 0; i < items.size(); i++) {
    items[i].mesh.ComputeTrigNormals();
    items[i].mesh.ComputeVertNormals();
  }

  Vec3f containerExtent = container.box.vmax - container.box.vmin;
  numSubgridCells = Vec3u(
      std::max(1u, (unsigned)std::ceil(containerExtent[0] / subgridCellSize)),
      std::max(1u, (unsigned)std::ceil(containerExtent[1] / subgridCellSize)),
      std::max(1u, (unsigned)std::ceil(containerExtent[2] / subgridCellSize)));
  unsigned totalCells = numSubgridCells[0] * numSubgridCells[1] * numSubgridCells[2];
  std::cout << "subgrid " << numSubgridCells[0] << "x" << numSubgridCells[1] << "x"
            << numSubgridCells[2] << " (" << totalCells << " cells)"
            << " cellSize=" << subgridCellSize << "\n";
}

void PackingScene::InitRigidBodies() {
  for (size_t i = 0; i < items.size(); i++) {
    auto &item = items[i];
    item.rb.ToInertiaFrame(item.mesh);
  }
}

void PackingScene::InitContainerGrids() {
  containerGrid.Build(container.mesh, gridDx);
  if (!containerInner.mesh.v.empty()) {
    containerInnerGrid.Build(containerInner.mesh, gridDx);
  }
}

std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh &mesh) {
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  sdf->voxSize = h;
  sdf->band = 16;
  sdf->distUnit = distUnit;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  CloseExterior(sdf->dist, sdf->MAX_DIST);
  Array3D8u frozen;
  // sdf->FastSweepCoarse(frozen);
  int band = 1000;
  FastSweepPar(sdf->dist, sdf->voxSize, distUnit, band, frozen);
  return sdf;
}

void PackingScene::ComputeContainerSDF() {
  float distUnit = 0.001f * containerSDFDx;
  std::cout << "computing container sdf \n";
  sdf = ComputeSDF(distUnit, containerSDFDx, container.mesh);

  // Array3D<Vec3f> gradients = ComputeSDFGradient(*sdf, distUnit, containerSDFDx);

  // std::string gradFile = outputFolder + "sdf_gradients.obj";
  // SaveGradientObj(gradFile, gradients, *sdf, containerSDFDx, 4);
}

Array3D<Vec3f> ComputeSDFGradient(const AdapSDF &sdf, float distUnit, float voxSize) {
  Vec3u gridSize = sdf.dist.GetSize();
  Array3D<Vec3f> gradients(gridSize, Vec3f(0, 0, 0));

  std::cout << "computing sdf gradients on grid " << gridSize[0] << " x " << gridSize[1] << " x " << gridSize[2]
            << "\n";

  for (unsigned z = 1; z < gridSize[2] - 1; z++) {
    for (unsigned y = 1; y < gridSize[1] - 1; y++) {
      for (unsigned x = 1; x < gridSize[0] - 1; x++) {
        Vec3f query = sdf.WorldCoord(Vec3f(x, y, z));
        gradients(x, y, z) = sdf.GetCoarseGrad(query);
      }
    }
  }

  return gradients;
}

void SaveGradientObj(
    const std::string &filename, const Array3D<Vec3f> &gradients, const AdapSDF &sdf, float voxSize, unsigned stride) {
  std::ofstream out(filename);
  if (!out.good()) {
    std::cout << "could not open " << filename << "\n";
    return;
  }

  Vec3u gridSize = gradients.GetSize();
  unsigned vertIdx = 1;

  for (unsigned z = stride; z < gridSize[2] - stride; z += stride) {
    for (unsigned y = stride; y < gridSize[1] - stride; y += stride) {
      for (unsigned x = stride; x < gridSize[0] - stride; x += stride) {
        Vec3f gridCoord(x, y, z);
        Vec3f worldPos = sdf.WorldCoord(gridCoord);
        Vec3f grad = gradients(x, y, z);

        Vec3f endPos = worldPos + grad * voxSize;

        out << "v " << worldPos[0] << " " << worldPos[1] << " " << worldPos[2] << "\n";
        out << "v " << endPos[0] << " " << endPos[1] << " " << endPos[2] << "\n";
        out << "l " << vertIdx << " " << (vertIdx + 1) << "\n";
        vertIdx += 2;
      }
    }
  }

  out.close();
  std::cout << "saved gradient debug to " << filename << "\n";
}

struct Contact {
    Vec3f worldPos;
    Vec3f normal;
    float distance;
    int meshIndex = -1;
};

float distSq(const Vec3f &a, const Vec3f &b) {
  return (a - b).norm2();
}

std::vector<Contact> reduceClusterTo4Points(const std::vector<Contact> &cluster) {
  if (cluster.size() <= 4) {
    return cluster;
  }

  std::vector<Contact> result;
  result.reserve(4);

  // Track which indices from the cluster we've already chosen
  std::vector<bool> chosen(cluster.size(), false);

  // 1. Find the deepest point (critical for resolving penetration)
  size_t idx0 = 0;
  float minDistance = cluster[0].distance;
  for (size_t i = 1; i < cluster.size(); ++i) {
    if (cluster[i].distance < minDistance) { // Assuming negative = deeper
      minDistance = cluster[i].distance;
      idx0 = i;
    }
  }
  result.push_back(cluster[idx0]);
  chosen[idx0] = true;

  // 2. Find the point furthest away from the deepest point
  size_t idx1 = 0;
  float maxDistSq = -1.0f;
  Vec3f p0 = result[0].worldPos;
  for (size_t i = 0; i < cluster.size(); ++i) {
    if (chosen[i])
      continue;
    float dSq = distSq(cluster[i].worldPos, p0);
    if (dSq > maxDistSq) {
      maxDistSq = dSq;
      idx1 = i;
    }
  }
  result.push_back(cluster[idx1]);
  chosen[idx1] = true;

  // 3. Find the point that maximizes the triangle area with p0 and p1
  size_t idx2 = 0;
  float maxAreaSq = -1.0f;
  Vec3f p1 = result[1].worldPos;
  Vec3f edge01 = p1 - p0;
  for (size_t i = 0; i < cluster.size(); ++i) {
    if (chosen[i])
      continue;
    Vec3f edge0i = cluster[i].worldPos - p0;
    // Cross product magnitude squared is proportional to triangle area squared
    float areaSq = edge01.cross(edge0i).dot(edge01.cross(edge0i));
    if (areaSq > maxAreaSq) {
      maxAreaSq = areaSq;
      idx2 = i;
    }
  }
  result.push_back(cluster[idx2]);
  chosen[idx2] = true;

  // 4. Find the point that maximizes the quad space
  // Heuristic: Maximize the minimum distance to any of the 3 existing points
  size_t idx3 = 0;
  float maxMinDistSq = -1.0f;
  Vec3f p2 = result[2].worldPos;
  for (size_t i = 0; i < cluster.size(); ++i) {
    if (chosen[i])
      continue;
    float d0 = distSq(cluster[i].worldPos, p0);
    float d1 = distSq(cluster[i].worldPos, p1);
    float d2 = distSq(cluster[i].worldPos, p2);
    float minDistSq = std::min({d0, d1, d2});

    if (minDistSq > maxMinDistSq) {
      maxMinDistSq = minDistSq;
      idx3 = i;
    }
  }
  result.push_back(cluster[idx3]);

  return result;
}

// 4 points per normal cluster.
std::vector<Contact> reduceContactManifold(const std::vector<Contact> &rawContacts,
                                           float angleThresholdDegrees = 5.0f) {
  if (rawContacts.empty())
    return {};

  float dotThreshold = std::cos(angleThresholdDegrees * 3.14159265f / 180.0f);
  std::vector<std::vector<Contact>> buckets;

  // Bucket contacts sharing roughly the same face normal
  for (const auto &contact : rawContacts) {
    bool bucketFound = false;
    for (auto &bucket : buckets) {
      if (contact.normal.dot(bucket[0].normal) > dotThreshold) {
        bucket.push_back(contact);
        bucketFound = true;
        break;
      }
    }
    if (!bucketFound) {
      buckets.push_back(std::vector<Contact>{contact});
    }
  }

  // Process each bucket and compile the final manifold
  std::vector<Contact> finalManifold;
  for (const auto &bucket : buckets) {
    std::vector<Contact> reducedCluster = reduceClusterTo4Points(bucket);
    finalManifold.insert(finalManifold.end(), reducedCluster.begin(), reducedCluster.end());
  }

  return finalManifold;
}

std::vector<SamplePoint> DownsamplePoints(const std::vector<SamplePoint> &points, float minSpacing) {
  std::vector<SamplePoint> filtered;
  if (points.empty()) {
    return filtered;
  }

  filtered.reserve(points.size() / 4);

  std::unordered_map<int64_t, std::vector<size_t>> spatialGrid;
  auto gridKey = [minSpacing](const Vec3f &p) -> int64_t {
    int32_t ix = static_cast<int32_t>(std::floor(p[0] / minSpacing));
    int32_t iy = static_cast<int32_t>(std::floor(p[1] / minSpacing));
    int32_t iz = static_cast<int32_t>(std::floor(p[2] / minSpacing));
    return (static_cast<int64_t>(ix) << 40) | (static_cast<int64_t>(iy) << 20) | static_cast<int64_t>(iz);
  };

  float spacingSq = minSpacing * minSpacing;

  for (const auto &pt : points) {
    int64_t key = gridKey(pt.x);
    bool tooClose = false;

    for (int dx = -1; dx <= 1 && !tooClose; dx++) {
      for (int dy = -1; dy <= 1 && !tooClose; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          int32_t ix = static_cast<int32_t>(std::floor(pt.x[0] / minSpacing)) + dx;
          int32_t iy = static_cast<int32_t>(std::floor(pt.x[1] / minSpacing)) + dy;
          int32_t iz = static_cast<int32_t>(std::floor(pt.x[2] / minSpacing)) + dz;
          int64_t neighborKey = (static_cast<int64_t>(ix) << 40) | (static_cast<int64_t>(iy) << 20) | static_cast<int64_t>(iz);

          auto it = spatialGrid.find(neighborKey);
          if (it != spatialGrid.end()) {
            for (size_t idx : it->second) {
              if ((pt.x - filtered[idx].x).norm2() < spacingSq) {
                tooClose = true;
                break;
              }
            }
          }
        }
      }
    }

    if (!tooClose) {
      spatialGrid[key].push_back(filtered.size());
      filtered.push_back(pt);
    }
  }

  return filtered;
}

std::vector<Vec3f> BoxCorners(const Box3f & b){
   return {
      b.vmin,
      Vec3f(b.vmax[0], b.vmin[1], b.vmin[2]),
      Vec3f(b.vmin[0], b.vmax[1], b.vmin[2]),
      Vec3f(b.vmax[0], b.vmax[1], b.vmin[2]),
      Vec3f(b.vmin[0], b.vmin[1], b.vmax[2]),
      Vec3f(b.vmax[0], b.vmin[1], b.vmax[2]),
      Vec3f(b.vmin[0], b.vmax[1], b.vmax[2]),
      b.vmax
    };
}

static std::vector<Vec3f> TransformPoints(const std::vector<Vec3f> &points, const Matrix3f &rot, const Vec3f &trans) {
  std::vector<Vec3f> transformed = points;
  for (size_t i = 0; i < transformed.size(); i++) {
    transformed[i] = rot * transformed[i] + trans;
  }
  return transformed;
}
static Quat4f IntegrateAngularVelocity(Vec3f angVel, float dt) {
  Quat4f q_delta = Quat4f::IDENTITY;
  float angle = std::sqrt(angVel.dot(angVel));
  if (angle > 1e-6f) {
    float s = std::sin(angle * dt * 0.5f);
    Vec3f axis = angVel * (1.0f / angle);
    q_delta = Quat4f(std::cos(angle * dt * 0.5f), axis[0] * s, axis[1] * s, axis[2] * s);
  }
  return q_delta;
}

static std::vector<Contact> GatherActiveContacts(
    const std::vector<SamplePoint> &fineSamples,
    const std::vector<const TrigGrid*> &accGrids,
    const Matrix3f &currentRotMat,
    const Vec3f &currentT,
    float eps,
    float activeBuffer,
    float contactAngleThreshDeg) {
    
  std::vector<Contact> activeContacts;
  float queryRadius = eps + activeBuffer;
  
  for (size_t s = 0; s < fineSamples.size(); s++) {
    Vec3f worldPt = currentRotMat * fineSamples[s].x + currentT;
    
    for (unsigned m = 0; m < accGrids.size(); m++) {
      if (!accGrids[m]->InRange(worldPt, queryRadius)) {
        continue;
      }
      ContactInfo info = accGrids[m]->NearestTriangle(worldPt, queryRadius);
      
      // We only care about points within our interaction radius
      if (info.dist >= 0.0f && info.dist < queryRadius) {
        // Compute signed distance using the face normal. All grids now
        // have normals oriented toward where the fruit should be
        // (container winding is reversed at init, neighbors/inner keep
        // outward winding). If the sample is on the wrong side
        // (protruding through the wall), the dot product is negative
        // and we negate the distance so the reduction (which keeps min
        // distance = deepest penetration) works correctly.
        Vec3f surfToSample = worldPt - info.closestPt;
        float signedDist = info.dist;
        if (surfToSample.dot(info.normal) < 0.0f) {
          signedDist = -info.dist;
        }
        
        activeContacts.push_back({worldPt, info.normal, signedDist, int(m)});
      }
    }
  }
  return reduceContactManifold(activeContacts, contactAngleThreshDeg);
}

static void SolveContactConstraintsPGS(
    const std::vector<Contact> &contacts,
    const Vec3f &currentT,
    const Matrix3f &currentRotMat,
    const Matrix3f &Rinv,
    float invMass,
    const Vec3f &invI_local,
    float eps,
    int pgsPasses,
    float dt,
    Vec3f &linearVel,
    Vec3f &angularVel) {
    
  std::vector<float> lambdas(contacts.size(), 0.0f);
  
  // Baumgarte Stabilization factor (0.1 to 0.3 is standard). 
  // Determines how aggressively we push out of penetrations.
  float beta = 0.2f; 

  for (int pass = 0; pass < pgsPasses; pass++) {
    for (size_t c = 0; c < contacts.size(); c++) {
      const auto &contact = contacts[c];
      Vec3f r = contact.worldPos - currentT;
      Vec3f r_cross_n = r.cross(contact.normal);

      // Current relative velocity at the contact point
      Vec3f v_contact = linearVel + angularVel.cross(r);
      float rel_vel = v_contact.dot(contact.normal);

      float penetration = eps - contact.distance;
      float target_vel;

      if (penetration > 0.0f) {
        // Penetrating: push out with Baumgarte stabilization.
        // target_vel is a velocity delta, so final vel = rel_vel + target_vel = bias.
        float bias = (beta / dt) * penetration;
        target_vel = bias - rel_vel;
      } else {
        // Not penetrating but within detection range.
        // Cap the approach velocity so the sample cannot move farther
        // than the remaining gap in one step. This prevents tunneling
        // through thin walls (e.g. container) while still allowing the
        // sample to approach and settle at dist = eps.
        // rel_vel < 0 means approaching the wall.
        float maxApproach = -(contact.distance - eps) / dt;
        if (rel_vel < maxApproach) {
          // final vel = rel_vel + target_vel = maxApproach.
          target_vel = maxApproach - rel_vel;
        } else {
          // Velocity is safe (slow approach or separating). Skip.
          continue;
        }
      }

      // Effective mass at the contact point
      Vec3f torqueLocal = Rinv * r_cross_n;
      Vec3f angAccLocal(torqueLocal[0] * invI_local[0], torqueLocal[1] * invI_local[1], torqueLocal[2] * invI_local[2]);
      float j_sq_len = invMass + torqueLocal.dot(angAccLocal);

      if (j_sq_len > 1e-8f) {
        float delta_lambda = target_vel / j_sq_len;
        float old_lambda = lambdas[c];

        // Clamp impulse to > 0 (forces can only push objects apart, not pull them together)
        lambdas[c] = std::max(0.0f, old_lambda + delta_lambda);
        float actual_delta_lambda = lambdas[c] - old_lambda;

        // Apply impulse to velocities
        linearVel += (actual_delta_lambda * invMass) * contact.normal;
        angularVel += actual_delta_lambda * (currentRotMat * angAccLocal);
      }
    }
  }
}

void MovePointsInward(std::vector<SamplePoint> &points,float offset, const std::shared_ptr<AdapSDF> &sdf) {
  for (auto& pt : points) {
      pt.x -= pt.n * offset;
  }
}
  
struct RigidBodyState {
  size_t step;
  Vec3f position;
  Quat4f rotation;
  Vec3f linearVel;
  Vec3f angularVel;
  Vec3f externalForce;
  std::vector<Contact> contacts;
};

std::string RigidBodyStateToString(const std::vector<RigidBodyState> & debugSteps) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(6);
  out << "{\n";
  out << "  \"steps\": [\n";
  for (size_t i = 0; i < debugSteps.size(); i++) {
    const auto &ds = debugSteps[i];
    out << "    {\n";
    out << "      \"step\": " << ds.step << ",\n";
    out << "      \"position\": [" << ds.position[0] << ", " << ds.position[1] << ", " << ds.position[2] << "],\n";
    out << "      \"rotation\": [" << ds.rotation.w() << ", " << ds.rotation.x() << ", " << ds.rotation.y() << ", "
        << ds.rotation.z() << "],\n";
    out << "      \"linearVelocity\": [" << ds.linearVel[0] << ", " << ds.linearVel[1] << ", " << ds.linearVel[2]
        << "],\n";
    out << "      \"angularVelocity\": [" << ds.angularVel[0] << ", " << ds.angularVel[1] << ", " << ds.angularVel[2]
        << "],\n";
    out << "      \"externalForce\": [" << ds.externalForce[0] << ", " << ds.externalForce[1] << ", "
        << ds.externalForce[2] << "],\n";
    out << "      \"contacts\": [\n";
    for (size_t c = 0; c < ds.contacts.size(); c++) {
      const auto &contact = ds.contacts[c];
      out << "        {\n";
      out << "          \"worldPos\": [" << contact.worldPos[0] << ", " << contact.worldPos[1] << ", "
          << contact.worldPos[2] << "],\n";
      out << "          \"normal\": [" << contact.normal[0] << ", " << contact.normal[1] << ", " << contact.normal[2]
          << "],\n";
      out << "          \"distance\": " << contact.distance << ",\n";
      out << "          \"meshIndex\": " << contact.meshIndex << "\n";
      out << "        }";
      if (c + 1 < ds.contacts.size()) {
        out << ",";
      }
      out << "\n";
    }
    out << "      ]\n";
    out << "    }";
    if (i + 1 < debugSteps.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

RigidTransform PackingScene::Nudge(unsigned itemIdx,
                                   const RigidTransform &tran,
                                   const Vec3f &dir0,
                                   std::vector<RigidTransform> &trajectory) {
  RigidTransform tOut = tran;

  std::vector<RigidBodyState> debugSteps;

  const float CONTACT_ANGLE_THRESH_DEG = 20.0f;

  auto &meshInfo = items[itemIdx];
  // Scale sample spacing by fruit size so small fruits still get enough
  // samples to detect overlap. ds is capped at 0.5 for large fruits and
  // floored so tiny meshes don't blow up sample count.
  Box3f fruitBox = ComputeBBox(meshInfo.mesh.v);
  Vec3f fruitExtent = fruitBox.vmax - fruitBox.vmin;
  float minExtent = std::min({fruitExtent[0], fruitExtent[1], fruitExtent[2]});
  float ds = 0.5f;
  if (minExtent < 3.0f) {
    ds = 0.2f;
  } else if (minExtent < 5.0f) {
    ds = 0.3f;
  }
  float eps = ds * 0.1f;
  float activeBuffer = ds;

  // Simulation parameters -- scaled by fruit size.
  // Small fruits need fewer steps to settle and coarser approximation is fine.
  size_t maxOptimizationSteps;
  float damping;
  float pgsPasses;
  float earlyExitVelSq;
  unsigned contactGatherInterval;
  if (minExtent < 3.0f) {
    maxOptimizationSteps = 30;
    damping = 0.82f;
    pgsPasses = 5;
    earlyExitVelSq = 1e-3f;
    contactGatherInterval = 1;
  } else if (minExtent < 5.0f) {
    maxOptimizationSteps = 50;
    damping = 0.84f;
    pgsPasses = 6;
    earlyExitVelSq = 1e-3f;
    contactGatherInterval = 2;
  } else if (minExtent < 10.0f) {
    maxOptimizationSteps = 80;
    damping = 0.85f;
    pgsPasses = 7;
    earlyExitVelSq = 1e-3f;
    contactGatherInterval = 1;
  } else {
    maxOptimizationSteps = 100;
    damping = 0.85f;
    pgsPasses = 8;
    earlyExitVelSq = 1e-4f;
    contactGatherInterval = 1;
  }
  float dt = 1.0f / 60.0f;          // Fixed time step
  float nudgeAcceleration = 200.0f; // Accelerate at 200 cm/s^2 (which is 2 m/s^2)
  // attraction towards contacting object.
  Vec3f attractionDir(-1,0,0);
  const float MAX_OVERLAP = 0.2;

  std::vector<SamplePoint> samples;
  if (meshInfo.samples.empty()) {
    std::vector<SamplePoint> allFineSamples;
    float sampleSpacing;
    float maxOverlap;
    if (minExtent < 3.0f) {
      sampleSpacing = std::max(0.2f, minExtent * 0.15f);
      maxOverlap = 0.0f;
    } else if (minExtent < 5.0f) {
      sampleSpacing = std::max(0.15f, minExtent * 0.12f);
      maxOverlap = 0.1f;
    } else {
      sampleSpacing = std::max(0.1f, std::min(ds, minExtent * 0.1f));
      maxOverlap = MAX_OVERLAP;
    }
    SamplePoints(meshInfo.mesh, sampleSpacing, allFineSamples);
    samples = DownsamplePoints(allFineSamples, sampleSpacing);
    std::cout<<"sample spacing " << sampleSpacing <<" num samples " << samples.size()<<"\n";
    meshInfo.ComputeSDFCached();
    MovePointsInward(samples, maxOverlap, meshInfo.sdf);
    meshInfo.samples = samples;
  } else {
    samples = meshInfo.samples;
  }

  Box3f localBox = fruitBox;
  Vec3f currentT = tran.position;
  Quat4f currentQ = Quat4f::fromRotationMatrix(tran.rotation);

  const auto &item = items[itemIdx];
  float invMass = 1.0f / item.rb.vol;
  Vec3f invI_local(1.0f/item.rb.inertia(0,0), 1.0f/item.rb.inertia(1,1), 1.0f/item.rb.inertia(2,2));

  Vec3f linearVel(0.0f);
  Vec3f angularVel(0.0f);
  std::vector<Contact> prevContacts;

  Utils::Stopwatch nudgeTotal;
  nudgeTotal.Start();
  double gatherTime = 0, pgsTime = 0, broadTime = 0;
  unsigned maxGrids = 0;

  for (size_t step = 0; step < maxOptimizationSteps; step++) {
    Utils::Stopwatch swStep;
    swStep.Start();
    Matrix3f currentRotMat = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());
    auto Rinv = currentRotMat.transposed();

    // 1. Apply External Forces
    float dir0Weight = 1.0f - step/float(maxOptimizationSteps);
    Vec3f forceDir = dir0Weight *dir0 + (1-dir0Weight) * attractionDir;
    forceDir.normalize();
    linearVel += (forceDir * nudgeAcceleration) * dt;

    // 2. DYNAMIC BROADPHASE STEP
    Utils::Stopwatch swBroad;
    swBroad.Start();
    auto corners = TransformPoints(BoxCorners(localBox), currentRotMat, currentT);
    Box3f instBox = ComputeBBox(corners);
    float stepQueryDist = eps + activeBuffer;
    std::vector<unsigned> intersectingInstances = broadPhase.GetNearby(instBox, stepQueryDist);

    // must be ordered by the intersectingInstances than 1-2 container meshes
    // for active contacts to return correct indices
    std::vector<const TrigGrid*> accGrids;

    for (size_t i = 0; i < intersectingInstances.size(); i++) {
      unsigned instIdx = intersectingInstances[i];
      auto it = instanceGrids.find(instIdx);
      if (it == instanceGrids.end()) {
        InstanceInfo inst = instances[instIdx];
        auto nbrMesh = std::make_shared<TrigMesh>();
        *nbrMesh = MakeTransformedMesh(items[inst.itemId].mesh, inst.tran);
        instanceMeshCache[instIdx] = nbrMesh;
        
        auto newGrid = std::make_shared<TrigGrid>();
        newGrid->Build(*nbrMesh, gridDx);
        instanceGrids[instIdx] = newGrid;
        accGrids.push_back(newGrid.get());
      } else {
        accGrids.push_back(it->second.get());
      }
    }

    accGrids.push_back(&containerGrid);
    if (innerContainerEnabled) {
      accGrids.push_back(&containerInnerGrid);
    }
    broadTime += swBroad.ElapsedMS();
    maxGrids = std::max(maxGrids, (unsigned)accGrids.size());

    // 3. Narrow Phase Contact Gathering (throttled for small fruits)
    std::vector<Contact> contacts;
    if (step % contactGatherInterval == 0) {
      Utils::Stopwatch swGather;
      swGather.Start();
      contacts = GatherActiveContacts(
          samples, accGrids, currentRotMat, currentT, eps, activeBuffer, CONTACT_ANGLE_THRESH_DEG);
      gatherTime += swGather.ElapsedMS();
    } else {
      contacts = prevContacts;
    }
    prevContacts = contacts;
    float minX = 1e30f;
    int attractItem = -1;
    // pick contacting object with min x coord.
    for (size_t i = 0; i < contacts.size(); i++){
      int gridIdx = contacts[i].meshIndex;
      if(gridIdx>=0 && gridIdx < intersectingInstances.size()){
        unsigned instIdx = intersectingInstances[unsigned(gridIdx)];
        float x = instances[instIdx].tran.position[0];
        if(x<minX || attractItem < 0){
          minX = x;
          attractItem = instIdx;
        }
      }
    }
    if(attractItem>= 0){
      attractionDir = instances[unsigned(attractItem)].tran.position - currentT;
      attractionDir.normalize();
    }
    // 4. PGS Solver Passes (Modifies Velocities)
    Utils::Stopwatch swPGS;
    swPGS.Start();
    SolveContactConstraintsPGS(contacts, currentT, currentRotMat, Rinv, invMass, invI_local,
                                eps, (int)pgsPasses, dt, linearVel, angularVel);
    pgsTime += swPGS.ElapsedMS();

    // 5. Integrate Velocities to Positions
    currentT += linearVel * dt;
    
    Quat4f q_delta = IntegrateAngularVelocity(angularVel, dt);
    currentQ = q_delta * currentQ;
    currentQ.normalize();

    // Apply damping so the fruit settles down and energy bleeds out
    linearVel *= damping;
    angularVel *= damping;

    // Record debug step data
    RigidBodyState dStep;
    dStep.step = step;
    dStep.position = currentT;
    dStep.rotation = currentQ;
    dStep.linearVel = linearVel;
    dStep.angularVel = angularVel;
    dStep.externalForce = forceDir * nudgeAcceleration;
    dStep.contacts = contacts;
    debugSteps.push_back(dStep);

    trajectory.push_back(RigidTransform(currentT, Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w())));
    
    // Early exit if the fruit has completely settled into a snug spot
    if (linearVel.dot(linearVel) < earlyExitVelSq && angularVel.dot(angularVel) < earlyExitVelSq) {
        break; 
    }
  }

  // Save nudge debug visualization data to a JSON file
  // std::string debugFolder = outputFolder.empty() ? "." : outputFolder;
  // std::string debugFilename = debugFolder + "/nudge_debug_" + items[itemIdx].name + "_" + std::to_string(instances.size()) + ".json";
  // std::string debugMeshFile = debugFolder + "/inertia_" + items[itemIdx].name + ".obj";
  // meshInfo.mesh.SaveObj(debugMeshFile);
  // fs::create_directories(debugFolder);
  // std::ofstream out(debugFilename);
  // if (out.good()) {
  //   out << RigidBodyStateToString(debugSteps);
  // }

  tOut.position = currentT;
  tOut.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

  double total = nudgeTotal.ElapsedMS();
  std::cout << "Nudge " << items[itemIdx].name
            << " total=" << total << "ms"
            << " steps=" << trajectory.size()
            << " samples=" << samples.size()
            << " extent=" << minExtent
             << " broad=" << broadTime << "ms"
            << " gather=" << gatherTime << "ms"
            << " pgs=" << pgsTime << "ms"
            << " grids=" << maxGrids
            << " pos=(" << currentT[0] << "," << currentT[1] << "," << currentT[2] << ")"
            << " start=(" << tran.position[0] << "," << tran.position[1] << "," << tran.position[2] << ")"
            << "\n";
  return tOut;
}

static Quat4f ExtractTwistX(const Quat4f &q) {
  Vec3f axis(1, 0, 0);
  Vec3f v(q.x(), q.y(), q.z());
  float projLen = v.dot(axis);
  Quat4f twist(q.w(), projLen, 0, 0);
  twist.normalize();
  return twist;
}

static void ApplyPositionConstraints(Vec3f &pos, Vec3f &vel,
                                     const PackingConstraints &c) {
  if (c.lockPosX) {
    pos[0] = c.fixedPosX;
    vel[0] = 0.0f;
  }
}

static void ApplyRotationConstraints(Quat4f &q, Vec3f &angVel,
                                     const PackingConstraints &c) {
  if (c.lockRotYZ) {
    q = ExtractTwistX(q);
    angVel[1] = 0.0f;
    angVel[2] = 0.0f;
  }
}

static void EnforceXSignWall(Vec3f &pos, Vec3f &vel, int xSign) {
  if (xSign == 0) {
    return;
  }
  const float EPS = 1e-3f;
  if (xSign < 0 && pos[0] > -EPS) {
    pos[0] = -EPS;
    if (vel[0] > 0.0f) {
      vel[0] = 0.0f;
    }
  } else if (xSign > 0 && pos[0] < EPS) {
    pos[0] = EPS;
    if (vel[0] < 0.0f) {
      vel[0] = 0.0f;
    }
  }
}

static Vec3f CenterAttractionDir(const Vec3f &pos,
                                 const PackingConstraints &c) {
  if (c.xSign != 0) {
    Vec3f dir(-pos[0], 0, 0);
    dir.normalize();
    return dir;
  }
  return Vec3f(-1, 0, 0);
}

static void SolveContactPBD(const std::vector<Contact> &contacts,
                            Vec3f &pos,
                            Quat4f &q,
                            float invMass,
                            const Vec3f &invI_local,
                            float targetDist,
                            int passes,
                            float linStiffness,
                            float angStiffness) {
  float perPassLin = 1.0f - std::pow(1.0f - linStiffness, 1.0f / passes);
  float perPassAng = 1.0f - std::pow(1.0f - angStiffness, 1.0f / passes);
  for (int pass = 0; pass < passes; pass++) {
    for (const auto &contact : contacts) {
      float penetration = targetDist - contact.distance;
      if (penetration <= 0.0f) {
        continue;
      }
      Vec3f r = contact.worldPos - pos;
      Vec3f r_cross_n = r.cross(contact.normal);
      Matrix3f R = Matrix3f::rotation(q.x(), q.y(), q.z(), q.w());
      Vec3f rcn_local = R.transposed() * r_cross_n;
      Vec3f angCorrLocal(rcn_local[0] * invI_local[0],
                         rcn_local[1] * invI_local[1],
                         rcn_local[2] * invI_local[2]);
      Vec3f angCorrWorld = R * angCorrLocal;
      float w_total = invMass + r_cross_n.dot(angCorrWorld);
      if (w_total < 1e-8f) {
        continue;
      }
      float lambdaLin = penetration / w_total * perPassLin;
      pos += (lambdaLin * invMass) * contact.normal;
      float lambdaAng = penetration / w_total * perPassAng;
      Vec3f rotCorrection = angCorrWorld * lambdaAng;
      float angle = std::sqrt(rotCorrection.dot(rotCorrection));
      if (angle > 1e-6f) {
        Vec3f axis = rotCorrection * (1.0f / angle);
        Quat4f dq(std::cos(angle * 0.5f),
                  axis[0] * std::sin(angle * 0.5f),
                  axis[1] * std::sin(angle * 0.5f),
                  axis[2] * std::sin(angle * 0.5f));
        q = dq * q;
        q.normalize();
      }
    }
  }
}

static Vec3f AngVelFromQuatDelta(const Quat4f &qNew, const Quat4f &qOld, float dt) {
  Quat4f dq = qNew * qOld.inverse();
  dq.normalize();
  if (dq.w() < 0.0f) {
    dq = Quat4f(-dq.w(), -dq.x(), -dq.y(), -dq.z());
  }
  float rotAngle;
  Vec3f rotAxis = dq.getAxisAngle(&rotAngle);
  if (rotAngle < 1e-6f) {
    return Vec3f(0.0f);
  }
  return rotAxis * (rotAngle / dt);
}

RigidTransform PackingScene::NudgeConstrained(unsigned itemIdx,
                                               const RigidTransform &tran,
                                               const Vec3f &dir0,
                                               const PackingConstraints &constraints,
                                               std::vector<RigidTransform> &trajectory) {
  RigidTransform tOut = tran;

  const float CONTACT_ANGLE_THRESH_DEG = 20.0f;
  auto &meshInfo = items[itemIdx];
  Box3f fruitBox = ComputeBBox(meshInfo.mesh.v);
  Vec3f fruitExtent = fruitBox.vmax - fruitBox.vmin;
  float minExtent = std::min({fruitExtent[0], fruitExtent[1], fruitExtent[2]});
  float ds = 0.5f;
  float eps = ds * 0.1f;
  float activeBuffer = 1.0f;

  size_t maxOptimizationSteps = 40;
  float dt = 1.0f / 60.0f;
  float damping = 0.85f;
  float nudgeAcceleration = 200.0f;
  const float MAX_OVERLAP = 0.2f;

  std::vector<SamplePoint> samples;
  if (meshInfo.samples.empty()) {
    std::vector<SamplePoint> allFineSamples;
    SamplePoints(meshInfo.mesh, ds, allFineSamples);
    samples = DownsamplePoints(allFineSamples, ds);
    meshInfo.ComputeSDFCached();
    MovePointsInward(samples, MAX_OVERLAP, meshInfo.sdf);
    std::vector<SamplePoint> hullSamples;
    AddConvexHullSamples(meshInfo.mesh, ds, 100, hullSamples);
    MovePointsInward(hullSamples, 0.02f, meshInfo.sdf);
    samples.insert(samples.end(), hullSamples.begin(), hullSamples.end());
    meshInfo.samples = samples;
  } else {
    samples = meshInfo.samples;
  }

  Box3f localBox = fruitBox;
  Vec3f currentT = tran.position;
  Quat4f currentQ = Quat4f::fromRotationMatrix(tran.rotation);

  Vec3f zeroVel(0.0f);
  ApplyPositionConstraints(currentT, zeroVel, constraints);
  ApplyRotationConstraints(currentQ, zeroVel, constraints);

  const auto &item = items[itemIdx];
  float invMass = 1.0f / item.rb.vol;
  Vec3f invI_local(1.0f/item.rb.inertia(0,0), 1.0f/item.rb.inertia(1,1), 1.0f/item.rb.inertia(2,2));

  Vec3f linearVel(0.0f);
  Vec3f angularVel(0.0f);

  std::unordered_map<unsigned, std::shared_ptr<TrigGrid>> persistentNeighborGrids;
  std::vector<std::shared_ptr<TrigMesh>> nbrMeshes;

  Utils::Stopwatch nudgeTotal;
  nudgeTotal.Start();

  for (size_t step = 0; step < maxOptimizationSteps; step++) {
    Matrix3f currentRotMat = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

    float dir0Weight = 1.0f - step/float(maxOptimizationSteps);
    Vec3f attractionDir = CenterAttractionDir(currentT, constraints);
    Vec3f forceDir = dir0Weight * dir0 + (1-dir0Weight) * attractionDir;
    if (constraints.lockPosX) {
      forceDir[0] = 0.0f;
    }
    forceDir.normalize();
    linearVel += (forceDir * nudgeAcceleration) * dt;

    Vec3f predT = currentT + linearVel * dt;
    Quat4f predQ = IntegrateAngularVelocity(angularVel, dt) * currentQ;
    predQ.normalize();
    Matrix3f predRotMat = Matrix3f::rotation(predQ.x(), predQ.y(), predQ.z(), predQ.w());

    auto corners = TransformPoints(BoxCorners(localBox), predRotMat, predT);
    Box3f instBox = ComputeBBox(corners);
    float stepQueryDist = eps + activeBuffer;
    std::vector<unsigned> intersectingInstances = broadPhase.GetNearby(instBox, stepQueryDist);

    std::vector<const TrigGrid*> accGrids;
    for (size_t i = 0; i < intersectingInstances.size(); i++) {
      unsigned instIdx = intersectingInstances[i];
      auto it = persistentNeighborGrids.find(instIdx);
      if (it == persistentNeighborGrids.end()) {
        InstanceInfo inst = instances[instIdx];
        auto nbrMesh = std::make_shared<TrigMesh>();
        *nbrMesh = MakeTransformedMesh(items[inst.itemId].mesh, inst.tran);
        nbrMeshes.push_back(nbrMesh);
        auto newGrid = std::make_shared<TrigGrid>();
        newGrid->Build(*nbrMesh, gridDx);
        persistentNeighborGrids[instIdx] = newGrid;
        accGrids.push_back(newGrid.get());
      } else {
        accGrids.push_back(it->second.get());
      }
    }
    accGrids.push_back(&containerGrid);
    if (innerContainerEnabled) {
      accGrids.push_back(&containerInnerGrid);
    }

    std::vector<Contact> contacts = GatherActiveContacts(
        samples, accGrids, predRotMat, predT, eps, activeBuffer, CONTACT_ANGLE_THRESH_DEG);

    int pbdPasses = 8;
    float pbdTarget = 0.5f;
    SolveContactPBD(contacts, predT, predQ, invMass, invI_local,
                    pbdTarget, pbdPasses, 1.0f, 0.1f);

    Vec3f correction = predT - currentT;
    float maxCorrection = 5.0f * dt;
    float corrLen = std::sqrt(correction.dot(correction));
    if (corrLen > maxCorrection) {
      correction *= maxCorrection / corrLen;
      predT = currentT + correction;
    }

    linearVel = (predT - currentT) / dt;
    angularVel = AngVelFromQuatDelta(predQ, currentQ, dt);

    const float MAX_SPEED = 50.0f;
    float linSpeed = std::sqrt(linearVel.dot(linearVel));
    if (linSpeed > MAX_SPEED) {
      linearVel *= MAX_SPEED / linSpeed;
    }
    float angSpeed = std::sqrt(angularVel.dot(angularVel));
    if (angSpeed > MAX_SPEED) {
      angularVel *= MAX_SPEED / angSpeed;
    }

    ApplyPositionConstraints(predT, linearVel, constraints);
    ApplyRotationConstraints(predQ, angularVel, constraints);
    EnforceXSignWall(predT, linearVel, constraints.xSign);

    currentT = predT;
    currentQ = predQ;

    linearVel *= damping;
    angularVel *= damping;

    trajectory.push_back(RigidTransform(currentT, Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w())));

    if (linearVel.dot(linearVel) < 1e-4f && angularVel.dot(angularVel) < 1e-4f) {
      break;
    }
  }

  tOut.position = currentT;
  tOut.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

  std::cout << "NudgeConstrained total " << nudgeTotal.ElapsedMS() << " ms\n";
  return tOut;
}

static bool ReadVec3f(std::istream &in, Vec3f &v) {
  return bool(in >> v[0] >> v[1] >> v[2]);
}

static bool ReadMatrix3f(std::istream &in, Matrix3f &m) {
  return bool(in >> m(0,0) >> m(0,1) >> m(0,2)
                 >> m(1,0) >> m(1,1) >> m(1,2)
                 >> m(2,0) >> m(2,1) >> m(2,2));
}

void LoadPack(PackingScene &scene, const std::string &packFile) {
  std::ifstream in(packFile);
  if (!in.good()) {
    return;
  }

  // Initialize placed vector with empty vectors for each item
  scene.placed.resize(scene.items.size());

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string itemName, posLabel, rotLabel, scaleLabel;
    RigidTransform trans;
    iss >> itemName >> posLabel;
    // Parse: ItemName pos x y z rot r00..r22 scale s
    if (!ReadVec3f(iss, trans.position)) continue;
    if (!(iss >> rotLabel) || !ReadMatrix3f(iss, trans.rotation)) continue;
    if (!(iss >> scaleLabel >> trans.scale)) continue;

    int itemIndex = -1;
    for (size_t i = 0; i < scene.items.size(); i++) {
      if (scene.items[i].name == itemName) {
        itemIndex = i;
        break;
      }
    }
    // convert from model coordinate to inertia coordiante
    const auto & item = scene.items[itemIndex];
    const auto & rb = item.rb;
    trans = rb.GetInertiaTran(trans);
    if (itemIndex >= 0) {
      scene.Put(itemIndex, trans);
    } else {
      std::cout << "Warning: item " << itemName << " not found in scene\n";
    }
  }

  // Print summary
  for (size_t i = 0; i < scene.items.size(); i++) {
    if (!scene.placed[i].empty()) {
      std::cout << "Loaded " << scene.placed[i].size() << " placements for " << scene.items[i].name << "\n";
    }
  }
}

void PackingScene::SaveTrajectories(const std::string &filename) const {
  std::ofstream trajOut(filename);
  for (size_t i = 0; i < instances.size(); i++) {
    const InstanceInfo &inst = instances[i];
    const auto &item = items[inst.itemId];
    std::string meshFile = item.filePath;
    trajOut << "Instance " << i << " Mesh " << meshFile << "\n";

    std::vector<RigidTransform> trajToSave;
    if (inst.trajectory.size() > 50) {
      size_t targetCount = 10;
      size_t stride = inst.trajectory.size() / targetCount;
      size_t lastIdx = 0;
      for (size_t j = 0; j < inst.trajectory.size(); j += stride) {
        trajToSave.push_back(inst.trajectory[j]);
        lastIdx = j;
      }
      if (lastIdx != inst.trajectory.size() - 1) {
        trajToSave.push_back(inst.trajectory.back());
      }
    } else {
      trajToSave = inst.trajectory;
    }

    for (size_t j = 0; j < trajToSave.size(); j++) {
      auto tran = trajToSave[j];
      tran = item.rb.GetInputTran(tran);
      trajOut << "Step " << j << " " << tran.toString() << "\n";
    }
    auto tran = inst.tran;
    tran = item.rb.GetInputTran(tran);
    trajOut << "Final " << tran.toString() << "\n";
    trajOut << "\n";
  }
  trajOut.close();
}

void PackingScene::SaveInstances(const std::string &packFile) const {
  std::ofstream out(packFile);
  out << instances.size() << "\n";
  for (size_t i = 0; i < instances.size(); i++) {
    const auto &item = items[instances[i].itemId];
    std::string name = item.name;
    RigidTransform tran = instances[i].tran;
    tran = item.rb.GetInputTran(tran);
    out << name << " " << tran.toString() << "\n";
  }
}

void AddInnerContainer(PackingScene &scene) {
  float dx = scene.dx;
  Box3f box = scene.container.box;
  VoxConf conf;
  conf.origin = ToArray(box.vmin);
  conf.unit = {dx, dx, dx};

  conf.gridSize = ComputeGridSize(box, dx, 1);
  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &scene.containerInner.mesh, vox);
  FloodOutside8u(vox, 1, 2);
  ThreshInPlace(vox, 1);
  Union(scene.bg, Vec3i(0), vox);
  scene.innerContainerEnabled = true;
}

/// @brief
/// @param scene
/// @param i item type index
void SavePackedMesh(const PackingScene &scene, unsigned i) {
  std::string name = scene.items[i].name;
  TrigMesh m = MakeMergedMesh(scene.items[i].mesh, scene.placed[i]);
  if (!m.v.empty()) {
    std::string outMeshName = scene.outputFolder + name + "_merge.obj";
    m.SaveObj(outMeshName);
  }
}
