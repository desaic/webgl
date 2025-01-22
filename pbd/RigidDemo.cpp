#define USE_DOUBLE 1
#include "Common/Common.h"
#include "Simulation/TimeManager.h"
#include "Simulation/SimulationModel.h"
#include "Simulation/TimeStepController.h"
#include <iostream>
#include "Simulation/Simulation.h"
#include "Utils/Logger.h"
#include "Utils/Timing.h"
#define _USE_MATH_DEFINES
#include "math.h"

#include "TrigMesh.h"

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

// main 
int main( int argc, char **argv )
{
	Utilities::logger.addSink(unique_ptr<Utilities::ConsoleSink>(new Utilities::ConsoleSink(Utilities::LogLevel::INFO)));
	Utilities::logger.addSink(unique_ptr<Utilities::ConsoleSink>(new Utilities::ConsoleSink(Utilities::LogLevel::DEBUG)));
	SimulationModel *model = new SimulationModel();
	model->init();
	Simulation::getCurrent()->setModel(model);

	buildModel();

	const auto& rbs = model->getRigidBodies();
	const auto* rb = rbs.back();
	auto q0 = rb->getRotation0();
	auto qI = rb->getRotationInitial();
	std::cout << q0.x() << " " << q0.y() << " " << q0.z() << " " << q0.w() << "\n";
	std::cout << qI.x() << " " << qI.y() << " " << qI.z() << " " << qI.w() << "\n";
	for (int i = 0; i < 100; i++) {
		auto q = rb->getRotation();
		rb->getRotationMatrix();
		std::cout << q.x() << " " << q.y() << " " << q.z() << " " << q.w() <<"\n";
		auto pos = rb->getPosition();
		std::cout << pos.x() << " " << pos.y() << " " << pos.z() << "\n";
		timeStep();
	}

	delete Simulation::getCurrent();
	delete model;

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
	TimeManager::getCurrent ()->setTimeStepSize (static_cast<Real>(0.005));

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

/** Create the rigid body model
*/
void createBodyModel()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	SimulationModel::RigidBodyVector &rb = model->getRigidBodies();

	string fileName =  "F:/meshes/trunk/cube.obj";
	IndexedFaceMesh mesh;
	VertexData vd;
	LoadObj(fileName, mesh, vd);
	
	string fileName2 = "F:/meshes/trunk/bunny_10k.obj";
	IndexedFaceMesh mesh2;
	VertexData vd2;
	LoadObj(fileName2, mesh2, vd2);

	rb.resize(numberOfBodies);	
	const Real density = 1.0;
	for (unsigned int i = 0; i < numberOfBodies-1; i++)
	{			
		rb[i] = new RigidBody();
		rb[i]->initBody(density, 
			Vector3r((Real)i*width, 0.0, 0.0),
			Quaternionr(1.0, 0.0, 0.0, 0.0), 
			vd, mesh);
	}
	// Make first body static
	rb[0]->setMass(0.0);

	// bunny
	const Quaternionr q(AngleAxisr(static_cast<Real>(1.0/6.0*M_PI), Vector3r(0.0, 0.0, 1.0)));
	const Vector3r t(static_cast<Real>(0.411) + (static_cast<Real>(numberOfBodies) - static_cast<Real>(1.0))*width, static_cast<Real>(-1.776), static_cast<Real>(0.356));
	rb[numberOfBodies - 1] = new RigidBody();
	rb[numberOfBodies - 1]->initBody(density, t, q, vd2, mesh2);

	for (unsigned int i = 0; i < numberOfBodies-1; i++)
	{
		model->addBallJoint(i, i + 1, Vector3r((Real)i*width + static_cast<Real>(0.5)*width, 0.0, 0.0));
	}
}
