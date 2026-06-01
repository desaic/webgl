#include "PackingScene.h"
#include "AdapSDF.h"
#include "FastSweep.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "MeshConvo.h"
#include "MeshOps.h"
#include "PointSample.h"
#include "Quat4f.h"
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

unsigned PackingScene::Put(unsigned itemIdx, const Transformation &tran) {
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
  if(placed.size() != items.size()){
    placed.resize(items.size());
  }
  if(itemIdx < placed.size()){
    placed[itemIdx].push_back(tran);
  }
  instances.push_back(InstanceInfo(itemIdx, tran));
  broadPhase.Add(meshBox, instId);
  return instances.size() - 1u;
}

Vec3f PackingScene::ForceDirection(unsigned itemIdx, 
  const Vec3f & gravity, float sdfFactor, const Transformation &tran) {
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


  for(unsigned i =0;i<items.size();i++){
    nameToIndex[items[i].name] = i;
  }
  InitContainerGrids();
  for (size_t i = 0; i < items.size(); i++) {
    items[i].mesh.ComputeVertNormals();
  }
  ComputeContainerSDF();
}

void PackingScene::InitContainerGrids() {
  containerGrid.Build(container.mesh, gridDx);
  if(!containerInner.mesh.v.empty()){
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

  Array3D<Vec3f> gradients = ComputeSDFGradient(*sdf, distUnit, containerSDFDx);

  std::string gradFile = outputFolder + "sdf_gradients.obj";
  SaveGradientObj(gradFile, gradients, *sdf, containerSDFDx, 4);
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

Transformation PackingScene::Nudge(unsigned itemIdx,
                                   const Transformation &tran,
                                   const Vec3f &dir0,
                                   std::vector<Transformation> &trajectory) {
  Transformation tOut = tran;

  // from inertia frame to current transform.
  //RigidBodyInfo rb = items[itemIdx].rb;
  
  const float CONTACT_ANGLE_THRESH_DEG = 5.0f;                                    
  float ds = 0.5f;                // Maximum linear step distance limit
  float minDist = ds * 0.1f;      // Target contact clearance boundary
  float eps = minDist;            // clearance threshold
  float activeBuffer = ds * 0.1f; // buffer zone to gather near-contacts
  size_t maxOptimizationSteps = 10;

  float broadPhaseDist = ds * maxOptimizationSteps;

  float MIN_LineSearchStep = 1e-2f;
  TrigMesh instMesh = MakeTransformedMesh(items[itemIdx].mesh, tran);  
  // samples are in object frame, so that it move with pos and rot.
  // If I need their normals, I need to rotate the normals first.
  std::vector<SamplePoint> samples;
  SamplePoints(items[itemIdx].mesh, 0.5f * ds, samples);

  // 2. Broad Phase Environment Gathering
  Box3f instBox = ComputeBBox(instMesh.v);
  std::vector<unsigned> intersectingInstances = broadPhase.GetNearby(instBox, broadPhaseDist);

  std::vector<TrigGrid> accGrids(intersectingInstances.size() + 1);
  std::vector<TrigMesh> neighborMeshes(intersectingInstances.size());
  accGrids[0] = containerGrid;
  for (size_t i = 0; i < intersectingInstances.size(); i++) {
    InstanceInfo inst = instances[intersectingInstances[i]];
    neighborMeshes[i] = MakeTransformedMesh(items[inst.itemId].mesh, inst.tran);
    accGrids[i + 1].Build(neighborMeshes[i], gridDx);
  }

  if(innerContainerEnabled){
    accGrids.push_back(containerInnerGrid);
  }

  // 3. Initialize State Variables for the 6-DOF Solver
  Vec3f currentT = tran.position;
  Matrix3f rotMat0 = tran.rotation;
  Quat4f currentQ = Quat4f::fromRotationMatrix(rotMat0);

  // Matrix3f invI_local = Matrix3f::Zero();  
  // for(unsigned i = 0;i<3;i++){    
  //   if(rb.inertia(i,i) > 0){
  //     invI_local(i,i) = 1.0f/rb.inertia(i,i);
  //   }else{
  //     invI_local(i,i) = 1.0f;
  //   }
  // }
  // 4. Main Kinematic Optimization Loop
  for (size_t step = 0; step < maxOptimizationSteps; step++) {
    std::vector<Contact> activeContacts;
    Matrix3f currentRotMat = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());
    Vec3f dir = dir0;
    dir.normalize();

    // Gather all active surface contacts across all neighboring distance grids
    for (size_t s = 0; s < samples.size(); s++) {
      Vec3f worldPt = currentRotMat * samples[s].x + currentT;
      Vec3f worldPt0 = rotMat0 * samples[s].x + tran.position;
      for (unsigned m = 0; m < accGrids.size(); m++) {
        ContactInfo info = accGrids[m].NearestTriangle(worldPt, ds);
        // "fix" inverted normal. only works if initially not in contact.
        Vec3f sampleToSurf = info.closestPt - worldPt0;
        // If the point is close enough to be an obstacle, track it
        if (info.dist >= 0.0f && info.dist < eps + activeBuffer) {
          if (sampleToSurf.dot(info.normal) > 0) {
            info.normal = -info.normal;
          }
          activeContacts.push_back({worldPt, info.normal, info.dist});
        }
      }
    }

    std::vector<Contact> contacts = reduceContactManifold(activeContacts, CONTACT_ANGLE_THRESH_DEG);
    // Initialize our ideal target movement delta (push down the packing direction)
    Vec3f deltaX = ds * dir;

    // 2. Generate a random torque/angular velocity vector
    Vec3f deltaTheta(0.0f);
    // Track accumulated impulses (lambdas) for proper unilateral LCP PGS
    std::vector<float> lambdas(contacts.size(), 0.0f);

    // Run Sequential Constraint Projection passes (Matrix-free PGS)
    int pgsPasses = 8;
    for (int pass = 0; pass < pgsPasses; pass++) {
      for (size_t c = 0; c < contacts.size(); c++) {
        const auto &contact = contacts[c];
        Vec3f r = contact.worldPos - (currentT );
        Vec3f r_cross_n = r.cross(contact.normal);

        // Evaluate the current velocity projection row matching this contact
        float j_delta = deltaX.dot(contact.normal) + deltaTheta.dot(r_cross_n);
        float b = eps - contact.distance; // Penetration error feedback depth

        float j_sq_len = 1.0f + r_cross_n.dot(r_cross_n); // Equivalent to J * M^-1 * J^T
        if (j_sq_len > 1e-8f) {
          float delta_lambda = (b - j_delta) / j_sq_len;

          // Clamp total accumulated lambda to be non-negative (unilateral constraint)
          float old_lambda = lambdas[c];
          lambdas[c] = std::max(0.0f, old_lambda + delta_lambda);
          float actual_delta_lambda = lambdas[c] - old_lambda;

          // Correct the 6-DOF vectors inline using the clamped lambda delta
          deltaX = deltaX + actual_delta_lambda * contact.normal;
          deltaTheta = deltaTheta + r_cross_n * actual_delta_lambda;
        }
      }
    }

    // 5. Backtracking Line Search Gatekeeper
    float scale = 1.0f;
    bool stepAccepted = false;

    while (scale > MIN_LineSearchStep) {
      Vec3f testT = currentT + deltaX * scale;

      // Build the proposed incremental rotation quaternion from axis-angle deltaTheta
      Quat4f q_delta = Quat4f::IDENTITY;
      float angle = std::sqrt(deltaTheta.dot(deltaTheta));
      if (angle > 1e-6f) {
        float s = std::sin(angle * scale * 0.5f);
        Vec3f axis = deltaTheta * (1.0f / angle);
        q_delta = Quat4f(std::cos(angle * scale * 0.5f), axis[0] * s, axis[1] * s, axis[2] * s);
      }

      Quat4f testQ = q_delta * currentQ;
      testQ.normalize();
      Matrix3f testRotMat = Matrix3f::rotation(testQ.x(), testQ.y(), testQ.z(), testQ.w());

      // Verify if this test step triggers hard penetrations
      bool localCollision = false;
      for (size_t s = 0; s < samples.size(); s++) {
        Vec3f testPt = testRotMat * samples[s].x + testT;

        for (unsigned m = 0; m < accGrids.size(); m++) {
          Vec3f dummyNormal;
          float dist = accGrids[m].NearestTriangle(testPt, ds, dummyNormal);

          // Hard cutoff: if it breaches past epsilon minus a tiny structural tolerance
          if (dist >= 0.0f && dist < eps - 0.01f) {
            localCollision = true;
            break;
          }
        }
        if (localCollision)
          break;
      }

      if (!localCollision) {
        // Step is safe! Commit updates and progress to the next optimization frame
        currentT = testT;
        currentQ = testQ;
        stepAccepted = true;
        break;
      }

      scale *= 0.5f; // Step failed, reduce scale and try again
    }

    // If the line search was forced to shrink to zero, the item is completely jammed
    if (!stepAccepted || (deltaX.dot(deltaX) + deltaTheta.dot(deltaTheta)) < 1e-7f) {
      break;
    }
    // for debug visualization.
    trajectory.push_back(
        Transformation(currentT, Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w())));
  }

  // 6. Export the finalized values back into your scene Transformation layout
  tOut.position = currentT;
  tOut.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

  return tOut;
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
    Transformation trans;

    // Parse: ItemName pos x y z rot x y z scale s
    if (iss >> itemName >> posLabel >> trans.position[0] >> trans.position[1] >> trans.position[2] >> rotLabel >>
        trans.rotation[0] >> trans.rotation[1] >> trans.rotation[2] >> scaleLabel >> trans.scale) {

      // Find which item index this corresponds to
      int itemIndex = -1;
      for (size_t i = 0; i < scene.items.size(); i++) {
        if (scene.items[i].name == itemName) {
          itemIndex = i;
          break;
        }
      }

      if (itemIndex >= 0) {
        scene.placed[itemIndex].push_back(trans);
      } else {
        std::cout << "Warning: item " << itemName << " not found in scene\n";
      }
    }
  }

  // Print summary
  for (size_t i = 0; i < scene.items.size(); i++) {
    if (!scene.placed[i].empty()) {
      std::cout << "Loaded " << scene.placed[i].size() << " placements for " << scene.items[i].name << "\n";
    }
  }
}

static std::array<float, 3> ToArray(const Vec3f &v) {
  return {v[0], v[1], v[2]};
}

void PackingScene::SaveTrajectories(const std::string &filename) const {
  std::ofstream trajOut(filename);
  for (size_t i = 0; i < instances.size(); i++) {
    const InstanceInfo &inst = instances[i];
    std::string meshFile = items[inst.itemId].filePath;
    trajOut << "Instance " << i << " Mesh " << meshFile << "\n";
    trajOut << "Final " << inst.tran.toString() << "\n";
    for (size_t j = 0; j < inst.trajectory.size(); j++) {
      trajOut << "Step " << j << " " << inst.trajectory[j].toString() << "\n";
    }
    trajOut << "\n";
  }
  trajOut.close();
}

void PackingScene::SaveInstances(const std::string & packFile)const{
  std::ofstream out(packFile);
  out << instances.size() <<"\n";
  for(size_t i = 0;i<instances.size();i++){
    std::string name = items[instances[i].itemId].name;
    out << name <<" "<<instances[i].tran.toString()<<"\n";
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
