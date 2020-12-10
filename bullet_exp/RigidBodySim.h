#pragma once
#include <array>
#include <vector>
#include "LinearMath/btAlignedObjectArray.h"

class TrigMesh;
class btBroadphaseInterface;
class btCollisionShape;
class btOverlappingPairCache;
class btCollisionDispatcher;
class btConstraintSolver;
struct btCollisionAlgorithmCreateFunc;
class btDefaultCollisionConfiguration;
class btTriangleMesh;
class btDiscreteDynamicsWorld;

struct RigidState {
  std::array<float,3> pos;
  //rotation in euler angles
  std::array<float,3> rot;
};

class RigidBodySim {
public:

  RigidBodySim();
  void Load();
  void Step();
  ///returns states of all objects with positive indices
  ///which are user added objects.
  void GetStates(std::vector<RigidState> * states);
  ~RigidBodySim();

  void setupEmptyDynamicsWorld();
  void addBounds();
  void addMeshToBullet(TrigMesh * mesh);
  
  std::vector<TrigMesh* > meshes;

  btAlignedObjectArray<btCollisionShape*>	collisionShapes;
  btAlignedObjectArray<btTriangleMesh*> trimeshes;
  btBroadphaseInterface* broadphase;
  btCollisionDispatcher* dispatcher;
  btConstraintSolver* solver;
  btDefaultCollisionConfiguration* collisionConfiguration;
  btDiscreteDynamicsWorld* world;

  std::array<float, 3> worldSize;

};