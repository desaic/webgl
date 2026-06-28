#include "PackingScene.h"
#include "AdapSDF.h"
#include "FastSweep.h"
#include "FloodOutside.h"
#include "GridUtils.h"
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
  float dir0Weight = 0.5f;
  // assume object is centered at origin in reference space.
  Vec3f sdfDir = sdf->GetCoarseGrad(tran.position);
  Vec3f dir = dir0Weight * dir0 + (1 - dir0Weight) * (sdfFactor * sdfDir);
  dir.normalize();
  return dir;
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
  InitContainerGrids();
  for (size_t i = 0; i < items.size(); i++) {
    items[i].mesh.ComputeVertNormals();
  }
  ComputeContainerSDF();
  InitRigidBodies();
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
    Vec3f localPos;
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

// 1 pt per normal cluster
std::vector<Contact> reduceContacts(const std::vector<Contact> &rawContacts, float angleThresholdDegrees = 5.0f) {
  std::vector<Contact> reducedContacts;

  // Convert angle threshold to a dot product limit (e.g., cos(3°) ≈ 0.9986)
  float dotThreshold = std::cos(angleThresholdDegrees * 3.14159265f / 180.0f);

  for (const auto &newContact : rawContacts) {
    bool matchFound = false;

    for (auto &existingContact : reducedContacts) {
      // If the normals are pointing at the same cube face
      if (newContact.normal.dot(existingContact.normal) > dotThreshold) {
        matchFound = true;

        // Keep the deepest point.
        // Assumes smaller/more negative distance = deeper penetration.
        if (newContact.distance < existingContact.distance) {
          existingContact.worldPos = newContact.worldPos;
          existingContact.distance = newContact.distance;
          existingContact.normal = newContact.normal;
        }
        break;
      }
    }

    // If it's a unique face normal, track it as a new contact row
    if (!matchFound) {
      reducedContacts.push_back(newContact);
    }
  }

  return reducedContacts;
}

static std::vector<SamplePoint> DownsamplePoints(const std::vector<SamplePoint> &points, float minSpacing) {
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

static std::vector<SamplePoint> CoarseSampleMesh(const TrigMesh &mesh, float maxSpacing) {
  std::vector<SamplePoint> samples;
  if (mesh.v.empty()) {
    return samples;
  }

  const auto &verts = mesh.v;
  const auto &normals = mesh.nv;

  bool hasNormals = !normals.empty() && normals.size() * 3 == verts.size();

  samples.reserve(verts.size() / 3);

  std::unordered_map<int64_t, std::vector<size_t>> spatialGrid;
  auto gridKey = [maxSpacing](const Vec3f &p) -> int64_t {
    int32_t ix = static_cast<int32_t>(std::floor(p[0] / maxSpacing));
    int32_t iy = static_cast<int32_t>(std::floor(p[1] / maxSpacing));
    int32_t iz = static_cast<int32_t>(std::floor(p[2] / maxSpacing));
    return (static_cast<int64_t>(ix) << 40) | (static_cast<int64_t>(iy) << 20) | static_cast<int64_t>(iz);
  };

  float spacingSq = maxSpacing * maxSpacing;

  for (size_t i = 0; i < verts.size(); i += 3) {
    Vec3f pos(verts[i], verts[i + 1], verts[i + 2]);
    Vec3f normal(0, 0, 1);

    if (hasNormals) {
      normal = normals[i / 3];
    }

    int64_t key = gridKey(pos);
    bool tooClose = false;

    for (int dx = -1; dx <= 1 && !tooClose; dx++) {
      for (int dy = -1; dy <= 1 && !tooClose; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          int32_t ix = static_cast<int32_t>(std::floor(pos[0] / maxSpacing)) + dx;
          int32_t iy = static_cast<int32_t>(std::floor(pos[1] / maxSpacing)) + dy;
          int32_t iz = static_cast<int32_t>(std::floor(pos[2] / maxSpacing)) + dz;
          int64_t neighborKey = (static_cast<int64_t>(ix) << 40) | (static_cast<int64_t>(iy) << 20) | static_cast<int64_t>(iz);

          auto it = spatialGrid.find(neighborKey);
          if (it != spatialGrid.end()) {
            for (size_t idx : it->second) {
              if ((pos - samples[idx].x).norm2() < spacingSq) {
                tooClose = true;
                break;
              }
            }
          }
        }
      }
    }

    if (!tooClose) {
      spatialGrid[key].push_back(samples.size());
      samples.push_back({pos, normal});
    }
  }

  return samples;
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

static Quat4f IntegrateAngularVelocity(Vec3f deltaTheta, float scale) {
  Quat4f q_delta = Quat4f::IDENTITY;
  float angle = std::sqrt(deltaTheta.dot(deltaTheta));
  if (angle > 1e-6f) {
    float s = std::sin(angle * scale * 0.5f);
    Vec3f axis = deltaTheta * (1.0f / angle);
    q_delta = Quat4f(std::cos(angle * scale * 0.5f), axis[0] * s, axis[1] * s, axis[2] * s);
  }
  return q_delta;
}

static std::vector<Contact> GatherActiveContacts(
    const std::vector<SamplePoint> &fineSamples,
    const std::vector<const TrigGrid*> &accGrids,
    const Matrix3f &currentRotMat,
    const Vec3f &currentT,
    const Matrix3f &rotMat0,
    const Vec3f &tranPosition,
    float eps,
    float activeBuffer,
    float contactAngleThreshDeg,
    size_t &rawCountOut) {
  std::vector<Contact> activeContacts;
  float queryRadius = eps + activeBuffer;
  for (size_t s = 0; s < fineSamples.size(); s++) {
    Vec3f worldPt = currentRotMat * fineSamples[s].x + currentT;
    Vec3f worldPt0 = rotMat0 * fineSamples[s].x + tranPosition;
    
    for (unsigned m = 0; m < accGrids.size(); m++) {
      ContactInfo info = accGrids[m]->NearestTriangle(worldPt, queryRadius);
      
      if (info.dist >= 0.0f && info.dist < eps + activeBuffer) {
        Vec3f sampleToSurf = info.closestPt - worldPt0;
        if (sampleToSurf.dot(info.normal) > 0) {
          info.normal = -info.normal;
        }
        activeContacts.push_back({worldPt, info.normal, info.dist});
      }
    }
  }
  rawCountOut = activeContacts.size();
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
    Vec3f &deltaX,
    Vec3f &deltaTheta) {
  std::vector<float> lambdas(contacts.size(), 0.0f);
  for (int pass = 0; pass < pgsPasses; pass++) {
    for (size_t c = 0; c < contacts.size(); c++) {
      const auto &contact = contacts[c];
      Vec3f r = contact.worldPos - currentT;
      Vec3f r_cross_n = r.cross(contact.normal);

      float j_delta = deltaX.dot(contact.normal) + deltaTheta.dot(r_cross_n);
      float b = eps - contact.distance; 
      
      Vec3f torqueLocal = Rinv * r_cross_n;
      Vec3f angAccLocal(torqueLocal[0] * invI_local[0], torqueLocal[1] * invI_local[1], torqueLocal[2] * invI_local[2]);
      float j_sq_len = invMass + torqueLocal.dot(angAccLocal);

      if (j_sq_len > 1e-8f) {
        float delta_lambda = (b - j_delta) / j_sq_len;
        float old_lambda = lambdas[c];
        lambdas[c] = std::max(0.0f, old_lambda + delta_lambda);
        float actual_delta_lambda = lambdas[c] - old_lambda;

        deltaX += (actual_delta_lambda * invMass) * contact.normal;
        deltaTheta += actual_delta_lambda * (currentRotMat * angAccLocal);
      }
    }
  }
}

static bool CheckCollision(
    const std::vector<SamplePoint> &coarseSamples,
    const std::vector<const TrigGrid*> &accGrids,
    const Matrix3f &testRotMat,
    const Vec3f &testT,
    float ds,
    float eps) {
  for (size_t s = 0; s < coarseSamples.size(); s++) {
    Vec3f testPt = testRotMat * coarseSamples[s].x + testT;

    for (unsigned m = 0; m < accGrids.size(); m++) {
      Vec3f dummyNormal;
      float dist = accGrids[m]->NearestTriangle(testPt, ds, dummyNormal);

      if (dist >= 0.0f && dist < eps - 0.01f) {
        return true;
      }
    }
  }
  return false;
}

RigidTransform PackingScene::Nudge(unsigned itemIdx,
                                   const RigidTransform &tran,
                                   const Vec3f &dir0,
                                   std::vector<RigidTransform> &trajectory) {
  RigidTransform tOut = tran;

  const float CONTACT_ANGLE_THRESH_DEG = 5.0f;

  Box3f meshBBox = ComputeBBox(items[itemIdx].mesh.v);
  Vec3f bboxSize = meshBBox.vmax - meshBBox.vmin;
  float maxDim = std::max({bboxSize[0], bboxSize[1], bboxSize[2]});

  // surface sample points average distance (cm)
  // scale based on object size: 0.5cm for small fruits, up to 2cm for large ones
  float ds = 0.5f;
  // parts cannot get closer than this.
  float eps = ds * 0.1f;
  // contact is active when distance is within eps + activeBuffer.
  float activeBuffer = ds; 

  size_t maxOptimizationSteps = 20;
  float MIN_LineSearchStep = 1e-2f;

  // 1. Fine & Coarse Samples Initialization
  std::vector<SamplePoint> allFineSamples;
  SamplePoints(items[itemIdx].mesh, ds, allFineSamples);
  std::vector<SamplePoint> fineSamples = DownsamplePoints(allFineSamples, ds);
  SavePointsObj(outputFolder + "/debug_banana_points.obj", fineSamples);
  std::vector<SamplePoint> coarseSamples = CoarseSampleMesh(items[itemIdx].mesh, ds);

  // OPTIMIZATION: Compute local bounding box once outside the loop
  Box3f localBox = ComputeBBox(items[itemIdx].mesh.v);

  // 2. Initialize State Variables
  Vec3f currentT = tran.position;
  Matrix3f rotMat0 = tran.rotation;
  Quat4f currentQ = Quat4f::fromRotationMatrix(rotMat0);

  auto item = items[itemIdx];
  float invMass = 1.0f/item.rb.vol;
  Vec3f invI_local(1.0f/item.rb.inertia(0,0), 1.0f/item.rb.inertia(1,1), 1.0f/item.rb.inertia(2,2));
  Vec3f velocityX(0.0f);
  Vec3f velocityTheta(0.0f);
  float damping = 0.7f; 

  // OPTIMIZATION: Persistent cache to prevent rebuilding grids for the same neighbor
  std::unordered_map<unsigned, std::shared_ptr<TrigGrid>> persistentNeighborGrids;
  std::vector<std::shared_ptr<TrigMesh> > nbrMeshes;

  Utils::Stopwatch nudgeTotal;
  Utils::Stopwatch stepTimer;
  float broadphaseMs = 0.0f;
  float narrowphaseMs = 0.0f;
  float pgsMs = 0.0f;
  float lineSearchMs = 0.0f;
  nudgeTotal.Start();

  // 3. Main Kinematic Optimization Loop
  for (size_t step = 0; step < maxOptimizationSteps; step++) {
    Matrix3f currentRotMat = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

    // DYNAMIC BROADPHASE STEP: Compute current world bounding box from local corners
    stepTimer.Start();
    auto corners = TransformPoints(BoxCorners(localBox), currentRotMat, currentT);
    Box3f instBox = ComputeBBox(corners);
    // Query broadphase with a tight radius specific only to this step's reach
    float stepQueryDist = eps + activeBuffer;
    std::vector<unsigned> intersectingInstances = broadPhase.GetNearby(instBox, stepQueryDist);

    // Assemble acceleration grids dynamically for this step
    // Store non-owning const pointers to eliminate vector deep copies
    std::vector<const TrigGrid*> accGrids;
    accGrids.push_back(&containerGrid);

    for (size_t i = 0; i < intersectingInstances.size(); i++) {
      unsigned instIdx = intersectingInstances[i];
      
      auto it = persistentNeighborGrids.find(instIdx);
      if (it == persistentNeighborGrids.end()) {
        InstanceInfo inst = instances[instIdx];
        auto nbrMesh = std::make_shared<TrigMesh>();
        *nbrMesh = MakeTransformedMesh(items[inst.itemId].mesh, inst.tran);
        
        nbrMeshes.push_back(nbrMesh);
        // Allocate heap-backed grid and build directly on it
        auto newGrid = std::make_shared<TrigGrid>();
        newGrid->Build(*nbrMesh, gridDx);
        
        persistentNeighborGrids[instIdx] = newGrid;
        accGrids.push_back(newGrid.get());
      } else {
        // Safe pointer collection without copy constructors firing
        accGrids.push_back(it->second.get());
      }
    }

    if (innerContainerEnabled) {
      accGrids.push_back(&containerInnerGrid);
    }
    broadphaseMs += stepTimer.ElapsedMS();

    // 4. Narrow Phase Contact Gathering
    stepTimer.Start();
    size_t rawCount = 0;
    std::vector<Contact> contacts = GatherActiveContacts(
        fineSamples, accGrids, currentRotMat, currentT, rotMat0, tran.position,
        eps, activeBuffer, CONTACT_ANGLE_THRESH_DEG, rawCount);
    std::cout << "# contact before after reduction " << rawCount << " " << contacts.size() << "\n";
    narrowphaseMs += stepTimer.ElapsedMS();

    auto Rinv = currentRotMat.transposed();
    
    Vec3f dir = dir0;
    dir.normalize();

    Vec3f deltaX = (ds * dir) + velocityX * damping;
    Vec3f deltaTheta = velocityTheta * damping;

    // 5. PGS Solver Passes
    stepTimer.Start();
    int pgsPasses = 8;
    SolveContactConstraintsPGS(contacts, currentT, currentRotMat, Rinv, invMass, invI_local, eps, pgsPasses, deltaX, deltaTheta);
    pgsMs += stepTimer.ElapsedMS();

    // 6. Backtracking Line Search Gatekeeper
    stepTimer.Start();
    float scale = 1.0f;
    bool stepAccepted = false;

    while (scale > MIN_LineSearchStep) {
      Vec3f testT = currentT + deltaX * scale;

      Quat4f q_delta = IntegrateAngularVelocity(deltaTheta, scale);

      Quat4f testQ = q_delta * currentQ;
      testQ.normalize();
      Matrix3f testRotMat = Matrix3f::rotation(testQ.x(), testQ.y(), testQ.z(), testQ.w());

      if (!CheckCollision(coarseSamples, accGrids, testRotMat, testT, ds, eps)) {
        currentT = testT;
        currentQ = testQ;
        stepAccepted = true;
        break;
      }

      scale *= 0.5f; 
    }
    lineSearchMs += stepTimer.ElapsedMS();

    if (!stepAccepted || (deltaX.dot(deltaX) + deltaTheta.dot(deltaTheta)) < 1e-7f) {
      break;
    }
    velocityX = deltaX * scale;
    velocityTheta = deltaTheta * scale;
    
    trajectory.push_back(RigidTransform(currentT, Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w())));
  }

  tOut.position = currentT;
  tOut.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

  std::cout << "Nudge total " << nudgeTotal.ElapsedMS() << " ms\n";
  std::cout << "  broadphase " << broadphaseMs << " ms\n";
  std::cout << "  narrowphase " << narrowphaseMs << " ms\n";
  std::cout << "  PGS solver " << pgsMs << " ms\n";
  std::cout << "  line search " << lineSearchMs << " ms\n";

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
    for (size_t j = 0; j < inst.trajectory.size(); j++) {
      auto tran = inst.trajectory[j];
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
