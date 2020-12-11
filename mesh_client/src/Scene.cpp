#include "Scene.h"

Instance::Instance() : meshId(0), x(0, 0, 0), r(0, 0, 0) 
{
}

Instance::Instance(int id, const Vec3& pos, const Vec3& rot):
  meshId(id), x(pos), r(rot)
{
}

int Scene::AddMesh(const TrigMesh& m)
{
  meshes.push_back(m);
  return int(meshes.size() - 1);
}

int Scene::AddInstance(int meshId, Vec3& position, Vec3& rotation)
{
  if ( meshId >= int(meshes.size()) ) {
    return -1;
  }
  Instance inst(meshId, position, rotation);
  instances.push_back(inst);
  return int(instances.size() - 1);
}