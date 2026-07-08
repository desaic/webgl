#include "PackBones.h"
#include "GridUtils.h"
#include "MeshInfo.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "PackingScene.h"
#include "RigidTransform.h"
#include "Stopwatch.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum class BoneCategory { Center, Left, Right, Other };

static BoneCategory ClassifyBone(const std::string &name) {
  size_t pos = name.find('_');
  std::string prefix = (pos == std::string::npos) ? name : name.substr(0, pos);
  if (prefix == "center") {
    return BoneCategory::Center;
  }
  if (prefix == "left") {
    return BoneCategory::Left;
  }
  if (prefix == "right") {
    return BoneCategory::Right;
  }
  return BoneCategory::Other;
}

struct BoneGroups {
  std::vector<unsigned> center;
  std::vector<unsigned> left;
  std::vector<unsigned> right;
};

static BoneGroups GroupBones(const std::vector<MeshInfo> &items) {
  BoneGroups g;
  for (unsigned i = 0; i < items.size(); i++) {
    BoneCategory cat = ClassifyBone(items[i].name);
    switch (cat) {
      case BoneCategory::Center:
        g.center.push_back(i);
        break;
      case BoneCategory::Left:
        g.left.push_back(i);
        break;
      case BoneCategory::Right:
        g.right.push_back(i);
        break;
      default:
        break;
    }
  }
  return g;
}

static const float DEG15_RAD = 15.0f * 3.14159265f / 180.0f;
static const unsigned CENTER_ROT_TRIALS = 24;

static Vec3f TrialRotation(const std::vector<Vec3f> &randAngles,
                           unsigned &angleIndex,
                           unsigned trial,
                           const PackingConstraints &constraints) {
  if (constraints.lockPosX) {
    return Vec3f(trial * DEG15_RAD, 0.0f, 0.0f);
  }
  Vec3f rot = randAngles[angleIndex];
  angleIndex++;
  if (angleIndex >= randAngles.size()) {
    angleIndex = 0;
  }
  if (constraints.lockRotYZ) {
    rot = Vec3f(rot[0], 0.0f, 0.0f);
  }
  return rot;
}

static void RevertToOriginalPose(MeshInfo &item) {
  item.rb.ToInputFrame(item.mesh);
  item.rb.R0 = Matrix3f::identity();
  item.rb.oldOrigin = Vec3f(0.0f);
  item.box = ComputeBBox(item.mesh.v);
  item.mesh.ComputeTrigNormals();
  item.mesh.ComputeVertNormals();
}

static void SaveProgress(PackingScene &scene, unsigned placed) {
  if (placed > 0 && placed % 10 == 0) {
    std::string trajFile = scene.trajFile + std::to_string((placed / 10) % 10) + ".txt";
    scene.SaveTrajectories(trajFile);
  }
  if (placed > 0 && placed % 20 == 0) {
    std::string packFile = scene.packFile + std::to_string((placed / 20) % 10) + ".txt";
    std::ofstream out(packFile);
    scene.SaveInstances(packFile);
  }
}

static void PackBonesStep(PackingScene &scene,
                          std::vector<unsigned> itemIndices,
                          const PackingConstraints &constraints) {
  std::sort(itemIndices.begin(), itemIndices.end(),
            [&](unsigned a, unsigned b) {
              return scene.items[a].BoxDiagonal() > scene.items[b].BoxDiagonal();
            });

  unsigned placed = 0;
  unsigned maxTrials = constraints.lockPosX ? CENTER_ROT_TRIALS : 10;
  unsigned angleIndex = 0;
  unsigned numItems = itemIndices.size();
  if (numItems == 0) {
    return;
  }
  float sdfFactor = 1.0f;
  Vec3f gravity(0.0f, -1.0f, -1.0f);

  for (unsigned i = 0; i < numItems; i++) {
    unsigned itemIndex = itemIndices[i];
    MeshInfo &item = scene.items[itemIndex];
    bool itemPlaced = false;
    for (unsigned trial = 0; trial < maxTrials; trial++) {
      Vec3f pos;
      Vec3f rot = TrialRotation(scene.randAngles, angleIndex, trial, constraints);
      bool success = FindSpotConstrained(scene.bg, item.mesh, pos, rot,
                                         scene.sdf, sdfFactor, constraints);
      if (success) {
        RigidTransform tran;
        tran.position = pos;
        tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);
        std::vector<RigidTransform> trajectory;
        RigidTransform newTran = scene.NudgeConstrained(itemIndex, tran,
                                                        gravity, constraints,
                                                        trajectory);
        unsigned instanceId = scene.Put(itemIndex, newTran);
        scene.instances[instanceId].trajectory = trajectory;
        itemPlaced = true;
        placed++;
        SaveProgress(scene, placed);
        break;
      }
    }
    if (!itemPlaced) {
      std::cout << "could not place " << item.name << "\n";
    }
  }
  std::cout << "PackBonesStep placed " << placed << " of " << numItems << " items\n";
}

static PackingConstraints MakeCenterConstraints() {
  PackingConstraints c;
  c.lockPosX = true;
  c.fixedPosX = 0.0f;
  c.lockRotYZ = true;
  return c;
}

static PackingConstraints MakeLeftConstraints() {
  PackingConstraints c;
  c.xSign = -1;
  return c;
}

static PackingConstraints MakeRightConstraints() {
  PackingConstraints c;
  c.xSign = 1;
  return c;
}

void PackBones() {
  std::string dataDir = "/media/desaic/WD/meshes/skeleton/";
  std::string meshDir = dataDir + "/bones/";
  std::string containerFile = dataDir + "cube.obj";

  PackingScene scene;
  std::vector<MeshInfo> bones = LoadAllMeshInfo(meshDir);
  scene.items = bones;

  LoadMeshInfo(scene.container, containerFile);

  float broadPhaseDx = 2.0f;
  Box3f containerBox = scene.container.box;
  scene.broadPhase.Init(containerBox, broadPhaseDx);  

  scene.outputFolder = dataDir + "/out/";
  fs::create_directories(scene.outputFolder);
  scene.packFile = scene.outputFolder + "/pack";
  scene.trajFile = scene.outputFolder + "/traj";

  scene.InitDataStructures();

  scene.dx = 1.0f;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);
  InvertContainer(scene.bg.vox, 1);
  scene.placed.resize(scene.items.size());

  BoneGroups groups = GroupBones(scene.items);
  std::cout << "center: " << groups.center.size()
            << " left: " << groups.left.size()
            << " right: " << groups.right.size() << "\n";

  for (unsigned idx : groups.center) {
    RevertToOriginalPose(scene.items[idx]);
  }

  PackingConstraints centerConstraints = MakeCenterConstraints();
  PackingConstraints leftConstraints = MakeLeftConstraints();
  PackingConstraints rightConstraints = MakeRightConstraints();

  PackBonesStep(scene, groups.center, centerConstraints);
  PackBonesStep(scene, groups.left, leftConstraints);
  PackBonesStep(scene, groups.right, rightConstraints);

  std::string packFile = scene.outputFolder + "/pack_final.txt";
  scene.SaveInstances(packFile);

  for (unsigned i = 0; i < scene.items.size(); i++) {
    SavePackedMesh(scene, i);
  }
}
