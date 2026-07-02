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
  ComputeContainerSDF();
  InitRigidBodies();

  for (size_t i = 0; i < items.size(); i++) {
    items[i].mesh.ComputeTrigNormals();
    items[i].mesh.ComputeVertNormals();
  }
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
    float contactAngleThreshDeg,
    size_t &rawCountOut) {
    
  std::vector<Contact> activeContacts;
  float queryRadius = eps + activeBuffer;
  
  for (size_t s = 0; s < fineSamples.size(); s++) {
    Vec3f worldPt = currentRotMat * fineSamples[s].x + currentT;
    
    for (unsigned m = 0; m < accGrids.size(); m++) {
      ContactInfo info = accGrids[m]->NearestTriangle(worldPt, queryRadius);
      
      // We only care about points within our interaction radius
      if (info.dist >= 0.0f && info.dist < queryRadius) {
        
        // the normal to point FROM the obstacle TO the fruit sample.
        // may lock up the sim if there is already penetration.
        Vec3f surfToSample = worldPt - info.closestPt;
        if (surfToSample.dot(info.normal) < 0.0f) {
          info.normal = -info.normal; 
        }
        
        activeContacts.push_back({worldPt, info.normal, info.dist, int(m)});
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

      // FIX 3: Baumgarte Stabilization (Position correction translated to velocity)
      // If distance < eps, we are penetrating. Create a bias velocity to push it out.
      float penetration = eps - contact.distance;
      float bias = 0.0f;
      if (penetration > 0.0f) {
          bias = (beta / dt) * penetration;
      }

      // Target velocity: we want to stop moving into the surface (-rel_vel) 
      // and add the bias to fix penetration.
      float target_vel = -rel_vel + bias;

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

  const float CONTACT_ANGLE_THRESH_DEG = 5.0f;

  Box3f meshBBox = ComputeBBox(items[itemIdx].mesh.v);
  Vec3f bboxSize = meshBBox.vmax - meshBBox.vmin;
  float maxDim = std::max({bboxSize[0], bboxSize[1], bboxSize[2]});

  float ds = 0.5f;
  float eps = ds * 0.1f;
  float activeBuffer = ds;
                           
  // Simulation parameters
  size_t maxOptimizationSteps = 100; // Might need slightly more steps for settling
  float dt = 1.0f / 60.0f;          // Fixed time step
  float damping = 0.85f;            // Velocity damping to simulate friction/air resistance
  float nudgeAcceleration = 200.0f; // Accelerate at 200 cm/s^2 (which is 2 m/s^2)
  // attraction towards contacting object.
  Vec3f attractionDir(-1,0,0);
  const float MAX_OVERLAP = 0.2;
  auto &meshInfo = items[itemIdx];

  std::vector<SamplePoint> samples;
  if (meshInfo.samples.empty()) {
    std::vector<SamplePoint> allFineSamples;
    SamplePoints(meshInfo.mesh, ds, allFineSamples);
    samples = DownsamplePoints(allFineSamples, ds);
    meshInfo.ComputeSDFCached();
    MovePointsInward(samples, MAX_OVERLAP, meshInfo.sdf);
    meshInfo.samples = samples;
  } else {
    samples = meshInfo.samples;
  }

  Box3f localBox = ComputeBBox(items[itemIdx].mesh.v);
  Vec3f currentT = tran.position;
  Matrix3f rotMat0 = tran.rotation;
  Quat4f currentQ = Quat4f::fromRotationMatrix(rotMat0);

  auto item = items[itemIdx];
  float invMass = 1.0f / item.rb.vol;
  Vec3f invI_local(1.0f/item.rb.inertia(0,0), 1.0f/item.rb.inertia(1,1), 1.0f/item.rb.inertia(2,2));
  
  Vec3f linearVel(0.0f);
  Vec3f angularVel(0.0f);

  std::unordered_map<unsigned, std::shared_ptr<TrigGrid>> persistentNeighborGrids;
  std::vector<std::shared_ptr<TrigMesh> > nbrMeshes;

  Utils::Stopwatch nudgeTotal;
  Utils::Stopwatch stepTimer;
  nudgeTotal.Start();

  for (size_t step = 0; step < maxOptimizationSteps; step++) {
    Matrix3f currentRotMat = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());
    auto Rinv = currentRotMat.transposed();

    // 1. Apply External Forces
    float dir0Weight = 1.0f - step/float(maxOptimizationSteps);
    Vec3f forceDir = dir0Weight *dir0 + (1-dir0Weight) * attractionDir;
    forceDir.normalize();
    linearVel += (forceDir * nudgeAcceleration) * dt;

    // 2. DYNAMIC BROADPHASE STEP
    auto corners = TransformPoints(BoxCorners(localBox), currentRotMat, currentT);
    Box3f instBox = ComputeBBox(corners);
    float stepQueryDist = eps + activeBuffer;
    std::vector<unsigned> intersectingInstances = broadPhase.GetNearby(instBox, stepQueryDist);

    // must be ordered by the intersectingInstances than 1-2 container meshes
    // for active contacts to return correct indices
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

    // 3. Narrow Phase Contact Gathering
    size_t rawCount = 0;
    std::vector<Contact> contacts = GatherActiveContacts(
        samples, accGrids, currentRotMat, currentT, eps, activeBuffer, CONTACT_ANGLE_THRESH_DEG, rawCount);
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
    int pgsPasses = 8;
    SolveContactConstraintsPGS(contacts, currentT, currentRotMat, Rinv, invMass, invI_local, eps, pgsPasses, dt, linearVel, angularVel);

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
    if (linearVel.dot(linearVel) < 1e-4f && angularVel.dot(angularVel) < 1e-4f) {
        break; 
    }
  }

  // Save nudge debug visualization data to a JSON file
  std::string debugFolder = outputFolder.empty() ? "." : outputFolder;
  std::string debugFilename = debugFolder + "/nudge_debug_" + items[itemIdx].name + "_" + std::to_string(instances.size()) + ".json";
  std::string debugMeshFile = debugFolder + "/inertia_" + items[itemIdx].name + ".obj";
  meshInfo.mesh.SaveObj(debugMeshFile);
  fs::create_directories(debugFolder);
  std::ofstream out(debugFilename);
  if (out.good()) {
    out << RigidBodyStateToString(debugSteps);
  }

  tOut.position = currentT;
  tOut.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

  std::cout << "Nudge total " << nudgeTotal.ElapsedMS() << " ms\n";
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
