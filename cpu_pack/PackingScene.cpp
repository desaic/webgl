#include "PackingScene.h"
#include "cpu_voxelizer.h"
#include "MeshConvo.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "MeshOps.h"
#include "PointSample.h"
#include "TrigGrid.h"
#include "Quat4f.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

Vec3i roundf(const Vec3f & fvec){
  return Vec3i(std::round(fvec[0]),std::round(fvec[1]),std::round(fvec[2]));
}

void PackingScene::Put(unsigned itemIdx, const Transformation &tran){
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
  placed[itemIdx].push_back(tran);  
  instances.push_back(InstanceInfo(itemIdx, tran));
  /// @TODO: add item to broarphase grid
  broadPhase.Add(meshBox , instId);
}

Vec3f PackingScene::ForceDirection(unsigned itemIdx, const Transformation & tran)
{
  Vec3f dir (-1,-1, -1);

  // push away or towards center depending on item size group.
  Vec3f center = bg.GetMeshCenter();

  Vec3f awayFromCenter = tran.position - center;
  awayFromCenter.normalize();
  /// @TODO compute better direction. and use item size.
  dir = dir + awayFromCenter;
  dir.normalize();
  return dir;
}

Transformation PackingScene::Nudge(unsigned itemIdx, const Transformation & tran, const Vec3f & dir) {
    Transformation tOut = tran;
    
    float ds = 0.5f;             // Maximum linear step distance limit
    float eps = ds * 0.1f;       // Target contact clearance boundary (epsilon)
    float activeBuffer = ds * 0.1f;     // Proximity threshold to consider a contact active
    float linearStepSize = 0.1f; // Linear drive push force factor per frame
    
    TrigMesh instMesh = MakeTransformedMesh(items[itemIdx].mesh, tran);
    std::vector<SamplePoint> samples;
    instMesh.ComputeVertNormals();
    SamplePoints(instMesh, 0.5f * ds, samples);
    
    // assuming item is originally centered around 0
    Vec3f com = tran.position; 

    // Store sample points as local vectors relative to the Center of Mass
    std::vector<Vec3f> localSampleOffsets(samples.size());
    for (size_t s = 0; s < samples.size(); ++s) {
        localSampleOffsets[s] = samples[s].x - com;
    }

    // 2. Broad Phase Environment Gathering
    Box3f instBox = ComputeBBox(instMesh.v);
    std::vector<unsigned> intersectingInstances = broadPhase.GetNearby(instBox, ds);
    
    std::vector<TrigGrid> accGrids(intersectingInstances.size() + 1);
    //holds actual mesh for accGrid's mesh pointer.
    std::vector<TrigMesh> neighborMeshes(intersectingInstances.size());
    accGrids[0].Build(container.mesh, dx); // Assuming dx is your global container voxel size
    for (size_t i = 0; i < intersectingInstances.size(); i++) {
        InstanceInfo inst = instances[intersectingInstances[i]];
        neighborMeshes[i] = MakeTransformedMesh(items[inst.itemId].mesh, inst.tran);
        accGrids[i + 1].Build(neighborMeshes[i], ds);
    }

    // 3. Initialize State Variables for the 6-DOF Solver
    Vec3f currentT = tran.position;
    Matrix3f rotMat = tran.rotation;
    Quat4f currentQ = Quat4f::fromRotationMatrix(rotMat); // Convert initial pose rotation

    size_t maxOptimizationSteps = 15;
    struct ActiveContact {
        Vec3f worldPos;
        Vec3f normal;
        float distance;
    };

    // 4. Main Kinematic Optimization Loop
    for (size_t step = 0; step < maxOptimizationSteps; step++) {
        std::vector<ActiveContact> activeContacts;
        Matrix3f currentRotMat = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

        // Gather all active surface contacts across all neighboring distance grids
        for (size_t s = 0; s < localSampleOffsets.size(); s++) {
            Vec3f worldPt = currentRotMat * localSampleOffsets[s] + currentT;

            for (unsigned m = 0; m < accGrids.size(); m++) {
                Vec3f normal;
                float dist = accGrids[m].NearestTriangle(worldPt, ds, normal);

                // If the point is close enough to be an obstacle, track it
                if (dist >= 0.0f && dist < eps + activeBuffer) {
                    activeContacts.push_back({worldPt, normal, dist});
                }
            }
        }

        // Initialize our ideal target movement delta (push down the packing direction)
        Vec3f deltaX = linearStepSize * dir;
        Vec3f deltaTheta(0.0f, 0.0f, 0.0f);

        // Run Sequential Constraint Projection passes (Matrix-free PGS)
        int pgsPasses = 4;
        for (int pass = 0; pass < pgsPasses; pass++) {
            for (const auto& contact : activeContacts) {
                Vec3f r = contact.worldPos - currentT;
                Vec3f r_cross_n = r.cross(contact.normal);

                // Evaluate the current velocity projection row matching this contact
                float j_delta = deltaX.dot(contact.normal) + deltaTheta.dot(r_cross_n);
                float b = eps - contact.distance; // Penetration error feedback depth

                if (j_delta < b) {
                    float j_sq_len = 1.0f + r_cross_n.dot(r_cross_n);
                    if (j_sq_len > 1e-8f) {
                        float delta_lambda = (b - j_delta) / j_sq_len;

                        // Correct the 6-DOF velocity vectors inline
                        deltaX     = deltaX     + delta_lambda * contact.normal;
                        deltaTheta = deltaTheta + r_cross_n * delta_lambda;
                    }
                }
            }
        }

        // 5. Backtracking Line Search Gatekeeper
        float scale = 1.0f;
        bool stepAccepted = false;

        while (scale > 1e-4f) {
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
            for (size_t s = 0; s < localSampleOffsets.size(); s++) {
                Vec3f testPt = testRotMat * localSampleOffsets[s] + testT;

                for (unsigned m = 0; m < accGrids.size(); m++) {
                    Vec3f dummyNormal;
                    float dist = accGrids[m].NearestTriangle(testPt, ds, dummyNormal);
                    
                    // Hard cutoff: if it breaches past epsilon minus a tiny structural tolerance
                    if (dist >= 0.0f && dist < eps - 0.01f) {
                        localCollision = true;
                        break;
                    }
                }
                if (localCollision) break;
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
    }

    // 6. Export the finalized values back into your scene Transformation layout
    tOut.position = currentT;
    tOut.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());

    return tOut;
}

void LoadPack(PackingScene & scene, const std::string & packFile){
  std::ifstream in (packFile);
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
    if (iss >> itemName >> posLabel
        >> trans.position[0] >> trans.position[1] >> trans.position[2]
        >> rotLabel
        >> trans.rotation[0] >> trans.rotation[1] >> trans.rotation[2]
        >> scaleLabel >> trans.scale) {

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
      std::cout << "Loaded " << scene.placed[i].size() << " placements for "
                << scene.items[i].name << "\n";
    }
  }
}

static std::array<float, 3> ToArray(const Vec3f &v) {
  return {v[0], v[1], v[2]};
}

void AddInnerContainer(PackingScene & scene){
  float dx =scene.dx;
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
