#define USE_DOUBLE 1
#include "Common/Common.h"
#include "Simulation/TimeManager.h"
#include "Simulation/SimulationModel.h"
#include "Simulation/TimeStepController.h"
#include <array>
#include <iostream>
#include "Simulation/Simulation.h"
#include "Simulation/DistanceFieldCollisionDetection.h"
#include "Utils/Logger.h"
#include "Utils/Timing.h"
#include "Discregrid/mesh/triangle_mesh.hpp"
#include "Discregrid/geometry/TriangleMeshDistance.h"
#include "Discregrid/cubic_lagrange_discrete_grid.hpp"
#include "Simulation/CubicSDFCollisionDetection.h"
#define _USE_MATH_DEFINES
#include "math.h"

#include "TrigMesh.h"
#include "Vec4.h"
#include "Quat4f.h"
#include "Array2D.h"
#include "ImageIO.h"
using namespace PBD;
using namespace std;
using namespace Utilities;

void timeStep ();
void buildModel ();
void createBodyModel();

const int numberOfBodies = 10;
const Real width = static_cast<Real>(1.0);
const Real height = static_cast<Real>(0.1);
const Real depth = static_cast<Real>(0.1);
INIT_LOGGING
INIT_TIMING

struct MotionState {
	Vec3f pos;
	Quat4f rot;
};

typedef std::vector<RigidBody*> RigidVec;
typedef std::vector<MotionState> RigidState;
DistanceFieldCollisionDetection* cd;

void SaveMeshes(const RigidVec&bodies, const std::string & outFile) {
	std::ofstream out(outFile);	
	if (!out.good()) {
		std::cout << "can't open " << outFile << "\n";
		return;
	}
	size_t globalVidx = 1;
	for (size_t i = 0; i < bodies.size(); i++) {
		std::string objectName = "o" + std::to_string(i);
		auto& geom = bodies[i]->getGeometry();
		auto verts = geom.getVertexDataLocal();
		auto& mesh = geom.getMesh();
		out << "o " << objectName << "\n";
		for (size_t j = 0; j < verts.size(); j++) {
			auto v = verts.getPosition(j);
			out <<"v " << v[0] << " " << v[1] << " " << v[2] << "\n";
		}
		const auto& faces = mesh.getFaces();
		for (size_t j = 0; j < faces.size(); j+=3) {
			out << "f " << (globalVidx +faces[j]) << " " << (globalVidx +faces[j + 1] )<< " " << (globalVidx +faces[j + 2]) << "\n";
		}
		globalVidx += verts.size();
	}
}

void SaveSimState(const std::vector<RigidState> & states,
	const std::string & outFile, const std::string & bodiesFile) {
	std::ofstream out(outFile);
	out << "bodies " << bodiesFile << "\n";
	out << "num_steps " << states.size() << "\n";
	for (size_t i = 0; i < states.size(); i++) {
		out << "step " << i << " num_bodies " <<states[i].size() << "\n";
		const RigidState& s = states[i];
		for (size_t j = 0; j < s.size();j++) {			
			out << s[j].pos[0] << " " << s[j].pos[1] << " " << s[j].pos[2] << "\n";
			out << s[j].rot[0] << " " << s[j].rot[1] << " " << s[j].rot[2] << " " << s[j].rot[3] << "\n";
		}
	}
}

void GenerateSDF();

// main 
int main( int argc, char **argv )
{
	Utilities::logger.addSink(unique_ptr<Utilities::ConsoleSink>(new Utilities::ConsoleSink(Utilities::LogLevel::INFO)));
	Utilities::logger.addSink(unique_ptr<Utilities::ConsoleSink>(new Utilities::ConsoleSink(Utilities::LogLevel::DEBUG)));
	GenerateSDF();

	SimulationModel *model = new SimulationModel();
	model->init();
	Simulation::getCurrent()->setModel(model);
	cd = new DistanceFieldCollisionDetection();
	cd->init();

	buildModel();

	const auto& rbs = model->getRigidBodies();
	std::string bodiesFile = "bodies.obj";
	SaveMeshes(rbs, "F:/meshes/trunk/sim/"+bodiesFile);

	std::vector<RigidState> states;

	for (int i = 0; i < 1000; i++) {
		RigidState state;
		state.resize(rbs.size());
		for (size_t j = 0; j < rbs.size(); j++) {
			const auto* rb = rbs[j];
			auto q0 = rb->getRotation0();
			auto qI = rb->getRotationInitial();
			//std::cout << q0.x() << " " << q0.y() << " " << q0.z() << " " << q0.w() << "\n";
			//std::cout << qI.x() << " " << qI.y() << " " << qI.z() << " " << qI.w() << "\n";
			auto q = rb->getRotation();
			//std::cout << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << "\n";
			auto pos = rb->getPosition();
			//std::cout << pos.x() << " " << pos.y() << " " << pos.z() << "\n";
			state[j].rot = Quat4f(q.w(), q.x(), q.y(), q.z());
			state[j].pos = Vec3f(pos[0], pos[1], pos[2]);
		}
		states.push_back(state);
		timeStep();
	}
	SaveSimState(states, "F:/meshes/trunk/sim/rigid_states.txt", bodiesFile);
	delete Simulation::getCurrent();
	delete model;
	delete cd;
	return 0;
}

void timeStep ()
{

	// Simulation code
	SimulationModel *model = Simulation::getCurrent()->getModel();
	const unsigned int numSteps = 10;
	for (unsigned int i = 0; i < numSteps; i++)
	{
		Simulation::getCurrent()->getTimeStep()->step(*model);
	}
}

void buildModel ()
{
	TimeManager::getCurrent ()->setTimeStepSize (static_cast<Real>(0.001));
	SimulationModel* model = Simulation::getCurrent()->getModel();
	Simulation::getCurrent()->getTimeStep()->setCollisionDetection(*model, cd);
	createBodyModel();
}

void LoadObj(const string &fileName ,IndexedFaceMesh & mesh, VertexData &vd) {

	TrigMesh meshIn;
	meshIn.LoadObj(fileName);
	mesh.release();
	const unsigned int nPoints = (unsigned int)meshIn.GetNumVerts();
	const unsigned int nFaces = (unsigned int)meshIn.GetNumTrigs();
	mesh.initMesh(nPoints, nFaces * 2, nFaces);
	vd.reserve(nPoints);
	for (unsigned int i = 0; i < nPoints; i++)
	{
		Vec3f v = meshIn.GetVertex(i);
		vd.addVertex(Vector3r(v[0], v[1],v[2]));
	}

	for (unsigned int i = 0; i < nFaces; i++)
	{
		int posIndices[3] = { meshIn.t[3 * i], meshIn.t[3 * i + 1],meshIn.t[3 * i + 2] };		
		mesh.addFace(&posIndices[0]);
	}
	mesh.buildNeighbors();

	mesh.updateNormals(vd, 0);
	mesh.updateVertexNormals(vd);
}

void GenerateSDF() {
	// Generate SDF
	std::string modelFileName = "F:/meshes/trunk/trunk0.obj";

	string fileNameTrunk = "F:/meshes/trunk/trunk0.obj";
	std::string sdfFile = modelFileName + ".csdf";
	IndexedFaceMesh meshTrunk;
	VertexData vdTrunk;
	LoadObj(fileNameTrunk, meshTrunk, vdTrunk);

	VertexData& vd = vdTrunk;
	IndexedFaceMesh& mesh = meshTrunk;

	std::vector<unsigned int>& faces = mesh.getFaces();
	const unsigned int nFaces = mesh.numFaces();

#ifdef USE_DOUBLE
	Discregrid::TriangleMesh sdfMesh(&vd.getPosition(0)[0], faces.data(), vd.size(), nFaces);
#else
	// if type is float, copy vector to double vector
	std::vector<double> doubleVec;
	doubleVec.resize(3 * vd.size());
	for (unsigned int i = 0; i < vd.size(); i++)
		for (unsigned int j = 0; j < 3; j++)
			doubleVec[3 * i + j] = vd.getPosition(i)[j];
	Discregrid::TriangleMesh sdfMesh(&doubleVec[0], faces.data(), vd.size(), nFaces);
#endif
	Discregrid::TriangleMeshDistance md(sdfMesh);
	Eigen::AlignedBox3d domain;
	for (auto const& x : sdfMesh.vertices())
	{
		domain.extend(x);
	}
	domain.max() += 0.1 * Eigen::Vector3d::Ones();
	domain.min() -= 0.1 * Eigen::Vector3d::Ones();
	Vec3i sdfRes(50, 50, 50);
	LOG_INFO << "Set SDF resolution: " << sdfRes[0] << ", " << sdfRes[1] << ", " << sdfRes[2];
	std::shared_ptr<CubicSDFCollisionDetection::Grid> sdfGrid= std::make_shared<CubicSDFCollisionDetection::Grid>(domain, std::array<unsigned int, 3>({ unsigned(sdfRes[0]), unsigned(sdfRes[1]), unsigned(sdfRes[2]) }));
	auto func = Discregrid::DiscreteGrid::ContinuousFunction{};
	func = [&md](Eigen::Vector3d const& xi) {return md.signed_distance(xi).distance; };
	LOG_INFO << "Generate SDF\n";
	sdfGrid->addFunction(func, true);
	//LOG_INFO << "Save SDF: " << sdfFile;
	//sdfGrid->save(sdfFile);
	double val = sdfGrid->interpolate(0, Eigen::Vector3d(1, 1, 1));
	std::cout << val << "\n";
	Array2D8u distImage(800, 800);
	Vec2u size = distImage.GetSize();
	float z = 2.95;
	for (unsigned y = 0; y < size[1]; y++) {
		for (unsigned x = 0; x < size[0]; x++) {
			double val = sdfGrid->interpolate(0, Eigen::Vector3d(x / (float)size[0] * 14, y / (float)size[1] * 14, z));
		//	std::cout << val << "\n";
		  distImage(x,y) = uint8_t( val * 100 + 125 );
		}
	}
	SavePngGrey("F:/meshes/trunk/trunk_sdf_slice.png", distImage);
}

/** Create the rigid body model
*/
void createBodyModel()
{
	SimulationModel* model = Simulation::getCurrent()->getModel();
	SimulationModel::RigidBodyVector& rb = model->getRigidBodies();
	SimulationModel::ConstraintVector& constraints = model->getConstraints();

	string fileNameBox = "F:/meshes/trunk/cube.obj";
	IndexedFaceMesh meshBox;
	VertexData vdBox;
	LoadObj(fileNameBox, meshBox, vdBox);
	meshBox.setFlatShading(true);

	string fileNameTrunk = "F:/meshes/trunk/trunk0.obj";
	IndexedFaceMesh meshTrunk;
	VertexData vdTrunk;
	LoadObj(fileNameTrunk, meshTrunk, vdTrunk);

	string fileNameCube = "F:/meshes/trunk/cuboid_dm.obj";
	IndexedFaceMesh meshCube;
	VertexData vdCube;
	LoadObj(fileNameCube, meshCube, vdCube);
	meshCube.setFlatShading(true);

	const unsigned int num_piles_x = 2;
	const unsigned int num_piles_z = 2;
	const Real dx_piles = 14.0;
	const Real dz_piles = 14.0;
	const Real startx_piles = -0.5 * (Real)(num_piles_x - 1) * dx_piles;
	const Real startz_piles = -0.5 * (Real)(num_piles_z - 1) * dz_piles;
	const unsigned int num_piles = num_piles_x * num_piles_z;
	const unsigned int num_bodies_x = 3;
	const unsigned int num_bodies_y = 5;
	const unsigned int num_bodies_z = 3;
	const Real dx_bodies = 6.0;
	const Real dy_bodies = 6.0;
	const Real dz_bodies = 6.0;
	const Real startx_bodies = -0.5 * (Real)(num_bodies_x - 1) * dx_bodies;
	const Real starty_bodies = 14.0;
	const Real startz_bodies = -0.5 * (Real)(num_bodies_z - 1) * dz_bodies;
	const unsigned int num_bodies = num_bodies_x * num_bodies_y * num_bodies_z;
	rb.resize(num_piles + num_bodies + 1);
	unsigned int rbIndex = 0;

	// floor
	rb[rbIndex] = new RigidBody();
	rb[rbIndex]->initBody(1.0,
		Vector3r(0.0, -0.5, 0.0),
		Quaternionr(1.0, 0.0, 0.0, 0.0),
		vdBox, meshBox, Vector3r(100.0, 1.0, 100.0));
	rb[rbIndex]->setMass(0.0);
	rb[rbIndex]->setRestitutionCoeff(0);
	const std::vector<Vector3r>& vertices = rb[rbIndex]->getGeometry().getVertexDataLocal().getVertices();
	const unsigned int nVert = static_cast<unsigned int>(vertices.size());

	cd->addCollisionBox(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, Vector3r(100.0, 1.0, 100.0));
	rbIndex++;

	Real current_z = startz_piles;
	for (unsigned int i = 0; i < num_piles_z; i++)
	{
		Real current_x = startx_piles;
		for (unsigned int j = 0; j < num_piles_x; j++)
		{
			rb[rbIndex] = new RigidBody();
			rb[rbIndex]->initBody(100.0,
				Vector3r(current_x, 5.0, current_z),
				Quaternionr(1.0, 0.0, 0.0, 0.0),
				vdTrunk, meshTrunk,
				Vector3r(0.5, 10.0, 0.5));
			rb[rbIndex]->setMass(0.0);
			rb[rbIndex]->setRestitutionCoeff(0);
			const std::vector<Vector3r>& vertices = rb[rbIndex]->getGeometry().getVertexDataLocal().getVertices();
			const unsigned int nVert = static_cast<unsigned int>(vertices.size());
			cd->addCollisionCylinder(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, Vector2r(0.5, 10.0));
			current_x += dx_piles;
			rbIndex++;
		}
		current_z += dz_piles;
	}

	srand((unsigned int)time(NULL));

	Real current_y = starty_bodies;
	unsigned int currentType = 0;
	for (unsigned int i = 0; i < num_bodies_y; i++)
	{
		Real current_x = startx_bodies;
		for (unsigned int j = 0; j < num_bodies_x; j++)
		{
			Real current_z = startz_bodies;
			for (unsigned int k = 0; k < num_bodies_z; k++)
			{
				rb[rbIndex] = new RigidBody();

				Real ax = static_cast <Real> (rand()) / static_cast <Real> (RAND_MAX);
				Real ay = static_cast <Real> (rand()) / static_cast <Real> (RAND_MAX);
				Real az = static_cast <Real> (rand()) / static_cast <Real> (RAND_MAX);
				Real w = static_cast <Real> (rand()) / static_cast <Real> (RAND_MAX);
				Quaternionr q(w, ax, ay, az);
				q.normalize();

				currentType = rand() % 4;

				rb[rbIndex]->initBody(100.0,
					Vector3r(current_x, current_y, current_z),
					q, //Quaternionr(1.0, 0.0, 0.0, 0.0),
					vdCube, meshCube,
					Vector3r(1.0, 1.0, 1.0));
				rb[rbIndex]->setRestitutionCoeff(0);
				const std::vector<Vector3r>& vertices = rb[rbIndex]->getGeometry().getVertexDataLocal().getVertices();
				const unsigned int nVert = static_cast<unsigned int>(vertices.size());
				cd->addCollisionBox(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, Vector3r(1.99, 0.99, 0.49));

				currentType = (currentType + 1) % 4;
				current_z += dz_bodies;
				rbIndex++;
			}
			current_x += dx_bodies;
		}
		current_y += dy_bodies;
	}
}
