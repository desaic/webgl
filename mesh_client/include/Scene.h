#pragma once
#include "TrigMesh.h"
#include "Vec3.h"

struct Instance {
	int meshId;
	///position
	Vec3f x;
	///rotation in zyx euler angles
	Vec3f r;
	Instance();
	Instance(int id, const Vec3f & pos, const Vec3f & rot);
};

class Scene{
public:
	Scene() {}
	~Scene() {}

	///\return mesh index. -1 on error
	int AddMesh(const TrigMesh & m);

	///\return instance index. -1 on error.
	int AddInstance(int meshId, Vec3f& position, Vec3f& rotation);

	const std::vector<TrigMesh>& GetMeshes()const {
		return meshes;
	}

	std::vector<TrigMesh>& GetMeshes(){
		return meshes;
	}

private:

	std::vector<TrigMesh> meshes;
	std::vector<Instance> instances;

};