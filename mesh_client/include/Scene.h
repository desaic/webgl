#pragma once
#include "TrigMesh.h"
#include "Eigen/Dense"

typedef Eigen::Vector3f Vec3;

struct Instance {
	int meshId;
	///position
	Vec3 x;
	///rotation in zyx euler angles
	Vec3 r;
	Instance();
	Instance(int id, const Vec3 & pos, const Vec3 & rot);
};

class Scene{
public:
	Scene() {}
	~Scene() {}

	///\return mesh index. -1 on error
	int AddMesh(const TrigMesh & m);

	///\return instance index. -1 on error.
	int AddInstance(int meshId, Vec3& position, Vec3& rotation);

private:

	std::vector<TrigMesh> meshes;
	std::vector<Instance> instances;

};