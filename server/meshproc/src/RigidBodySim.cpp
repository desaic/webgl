#include "RigidBodySim.h"
#include "hacdCircularList.h"
#include "hacdVector.h"
#include "hacdICHull.h"
#include "hacdGraph.h"
#include "hacdHACD.h"

#include "ConvexBuilder.h"
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btIDebugDraw.h"
#include "LinearMath/btGeometryUtil.h"
#include "BulletCollision/CollisionShapes/btShapeHull.h"

RigidBodySim::RigidBodySim() :
	broadphase(nullptr),
	dispatcher(nullptr),
	solver(nullptr),
	collisionConfiguration(nullptr),
	world(nullptr),
	//y z flipped in this example because graphics vs 3d printing.
	worldSize({400, 200, 250}) //x y z in mm. 
{
}

btRigidBody* createRigidBody(float mass, const btTransform& startTransform, btCollisionShape* shape, const btVector4& color = btVector4(1, 0, 0, 1))
{
	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isDynamic = (mass != 0.f);
	btVector3 localInertia(0, 0, 0);
	if (isDynamic) {
		shape->calculateLocalInertia(mass, localInertia);
	}
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo cInfo(mass, myMotionState, shape, localInertia);
	btRigidBody* body = new btRigidBody(cInfo);
	//body->setContactProcessingThreshold(m_defaultContactProcessingThreshold);
	//negative index bodies are not returned in 
	body->setUserIndex(-1);
	return body;
}

void RigidBodySim::Load()
{
	setupEmptyDynamicsWorld();
	addBounds();
	
	//add a random box for debugging.
	btVector3 boxHalfSize = btVector3(10, 10, 10);
	btBoxShape* colShape = new btBoxShape(boxHalfSize);
	collisionShapes.push_back(colShape);
	btTransform startTransform;
	startTransform.setIdentity();
	btScalar mass(1.f);
	btVector3 localInertia(0, 0, 0);
	colShape->calculateLocalInertia(mass, localInertia);
	startTransform.setOrigin(btVector3(50,20,50));
	btRigidBody* body = createRigidBody(mass, startTransform, colShape);
	world->addRigidBody(body);
	body->setUserIndex(0);
	for (size_t i = 9; i < meshes.size(); i++) {
		addMeshToBullet(meshes[i]);
	}
}

void RigidBodySim::Step()
{
	float DEFAULT_TIME_STEP = 0.01;
	//time step
	float h = DEFAULT_TIME_STEP;
	world->stepSimulation(h);
	std::vector<RigidState> states;
	GetStates(&states);
	
}

RigidBodySim::~RigidBodySim() {
  for (size_t i = 0; i < meshes.size(); i++) {
    delete meshes[i];
  }
  meshes.clear();
}


void addConvexDecomp(TrigMesh * mesh) {

}


void RigidBodySim::setupEmptyDynamicsWorld()
{
	collisionConfiguration = new btDefaultCollisionConfiguration();

	btVector3 worldAabbMin(0,0,0);
	btVector3 worldAabbMax(worldSize[0], worldSize[1], worldSize[2]);

	broadphase = new btAxisSweep3(worldAabbMin, worldAabbMax);
	dispatcher = new	btCollisionDispatcher(collisionConfiguration);
	solver = new btSequentialImpulseConstraintSolver();
	world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
	world->setGravity(btVector3(0, -9.8, 0));
}

///\param size is converted to halfsize
///\param collisionShapes newly created collision shape pointer will be pushed here
void addStaticBoxToWorld(btVector3 pos, btVector3 size, btDynamicsWorld * world,
	btAlignedObjectArray<btCollisionShape*>	* collisionShapes) {
	btVector3 halfSize = 0.5 * size;
	btBoxShape* shape = new btBoxShape(halfSize);
	collisionShapes->push_back(shape);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(pos);
	btScalar mass(0.);	
	createRigidBody(mass, transform, shape, btVector4(0, 0, 1, 1));
	btRigidBody* body = createRigidBody(mass, transform, shape);
	world->addRigidBody(body);
}

void RigidBodySim::addBounds()
{
	float thickness = 10;
	//floor
	btVector3 size(worldSize[0], thickness, worldSize[2] );
	btVector3 pos(worldSize[0]/2, -thickness / 2, worldSize[2]/2);
	addStaticBoxToWorld(pos, size, world, &collisionShapes);
	//-x
	size = btVector3(thickness, worldSize[1], worldSize[2]);
	pos  = btVector3(-thickness / 2, worldSize[1] / 2, worldSize[2] / 2);
	addStaticBoxToWorld(pos, size, world, &collisionShapes);
	//x
  pos = btVector3(worldSize[1] + thickness / 2, worldSize[1] / 2, worldSize[2] / 2);
	addStaticBoxToWorld(pos, size, world, &collisionShapes);
	//-z
	size = btVector3(worldSize[0], worldSize[1], thickness);
	pos =  btVector3(worldSize[0]/2, worldSize[1] / 2, -thickness/2);
	addStaticBoxToWorld(pos, size, world, &collisionShapes);
	//z
	pos = btVector3(worldSize[0] / 2, worldSize[1] / 2, worldSize[2] + thickness / 2);
	addStaticBoxToWorld(pos, size, world, &collisionShapes);
}

void RigidBodySim::addMeshToBullet(TrigMesh* mesh)
{

}

void RigidBodySim::GetStates(std::vector<RigidState>* states)
{
	int numCollisionObjects = world->getNumCollisionObjects();
	for (int i = 0; i < numCollisionObjects; i++) {
		btCollisionObject* colObj = world->getCollisionObjectArray()[i];
		int index = colObj->getUserIndex();
		//not a user shape
		if (index < 0) {
			continue;
		}
		btCollisionShape* colShape = colObj->getCollisionShape();
		btVector3 pos = colObj->getWorldTransform().getOrigin();
		btQuaternion orn = colObj->getWorldTransform().getRotation();
		
		RigidState s;
		for (int d = 0; d < 3; d++) {
			s.pos[d] = pos[d];
		}
		orn.getEulerZYX(s.rot[2], s.rot[1], s.rot[0]);
		states->push_back(s);
	}
}