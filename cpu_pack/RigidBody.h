#include "Matrix3f.h"
#include "TrigMesh.h"

#include "eig3x3.h"

// helpers
Matrix3f eigVecFromVal(const Matrix3f &A_in, const double *eigenvalues);

class RigidBody {
  public:
    Vec3f oldOrigin;
    // going from inertia frame back to input pose.
    Matrix3f R0;

    Matrix3f inertia;

    TrigMesh mesh;

    RigidBody(TrigMesh &input);

    TrigMesh GetOriginalPose() const;
};


static void TestInertiaFrame(){
  const std::string fruitFile = "F:/meshes/fruit_hand/papaya_debug.obj";
  TrigMesh mesh;
  mesh.LoadObj(fruitFile);
  RigidBody rb(mesh);
  rb.mesh.SaveObj("F:/meshes/fruit_hand/debug_inertia_papaya.obj");
  TrigMesh roundTrip = rb.GetOriginalPose();
  roundTrip.SaveObj("F:/meshes/fruit_hand/debug_around_papaya.obj");
}
