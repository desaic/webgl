#include "HullNudge.h"

#include "NudgeInternal.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>

namespace {

struct Neighbor {
  const HullSet *set;
  Matrix3f rot, rotInv;
  Vec3f pos;
};

std::vector<Vec3f> BoxCornerList(const Box3f &b) {
  std::vector<Vec3f> c;
  for (int i = 0; i < 8; i++) {
    c.push_back(Vec3f((i & 1) ? b.vmax[0] : b.vmin[0], (i & 2) ? b.vmax[1] : b.vmin[1],
                      (i & 4) ? b.vmax[2] : b.vmin[2]));
  }
  return c;
}

}  // namespace

RigidTransform NudgeWithHulls(PackingScene &scene, HullLibrary &lib, unsigned itemIdx,
                              const RigidTransform &tran,
                              const std::function<Vec3f(const Vec3f &)> &forceField,
                              size_t maxSteps, std::vector<RigidTransform> &trajectory,
                              HullNudgeStats *stats) {
  Utils::Stopwatch total;
  total.Start();
  RigidTransform out = tran;
  const MeshInfo &item = scene.items[itemIdx];
  const HullSet *self = lib.Get(itemIdx, item);
  if (!self) return out;
  const std::vector<SamplePoint> &samples = self->samples;

  const Vec3f ext = self->box.vmax - self->box.vmin;
  const float minExtent = std::min({ext[0], ext[1], ext[2]});
  float ds = 0.5f;
  if (minExtent < 3.0f) ds = 0.2f;
  else if (minExtent < 5.0f) ds = 0.3f;
  const float eps = ds * 0.1f;
  const float activeBuffer = ds;
  const float queryRadius = eps + activeBuffer;
  const float contactAngleThreshDeg = 20.0f;

  RBSimParams sim;
  SetSimParams(sim, minExtent);
  if (maxSteps > 0) sim.steps = maxSteps;
  const float dt = 1.0f / 60.0f;
  const float nudgeAcceleration = 200.0f;

  const float invMass = 1.0f / item.rb.vol;
  const Vec3f invI_local(1.0f / item.rb.inertia(0, 0), 1.0f / item.rb.inertia(1, 1),
                         1.0f / item.rb.inertia(2, 2));

  Vec3f currentT = tran.position;
  Quat4f currentQ = Quat4f::fromRotationMatrix(tran.rotation);
  Vec3f linearVel(0.0f), angularVel(0.0f);

  const size_t settleWindow = 5;
  const float settlePosTol = 0.1f;       // cm
  const float settleAngleTol = 0.0087f;  // rad
  std::vector<Vec3f> recentPos;
  std::vector<Quat4f> recentQ;
  const std::vector<Vec3f> boxCorners = BoxCornerList(self->box);

  for (size_t step = 0; step < sim.steps; step++) {
    const Matrix3f R = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());
    const Matrix3f Rinv = R.transposed();

    // force and torque from the field at every sample.
    Vec3f forceSum(0.0f), torqueSum(0.0f);
    for (const SamplePoint &sp : samples) {
      const Vec3f r = R * sp.x;
      const Vec3f f = forceField(currentT + r);
      forceSum += f;
      torqueSum += r.cross(f);
    }
    const float inv = samples.empty() ? 0.0f : 1.0f / float(samples.size());
    Vec3f forceDir = forceSum * inv;
    Vec3f torqueLocal = Rinv * (torqueSum * inv);
    Vec3f angAccLocal(torqueLocal[0] * invI_local[0] * item.rb.vol,
                            torqueLocal[1] * invI_local[1] * item.rb.vol,
                            torqueLocal[2] * invI_local[2] * item.rb.vol);
    angularVel += (R * angAccLocal) * (nudgeAcceleration * dt);
    linearVel += (forceDir * nudgeAcceleration) * dt;

    // neighbors from the broad phase
    Utils::Stopwatch contactClock;
    contactClock.Start();
    std::vector<Vec3f> corners;
    for (const Vec3f &c : boxCorners) corners.push_back(R * c + currentT);
    const Box3f instBox = ComputeBBox(corners);
    std::vector<Neighbor> neighbors;
    for (unsigned j : scene.broadPhase.GetNearby(instBox, queryRadius)) {
      const InstanceInfo &inst = scene.instances[j];
      if (inst.isExclusion) continue;
      const HullSet *hs = lib.Get(inst.itemId, scene.items[inst.itemId]);
      if (!hs) continue;
      neighbors.push_back({hs, inst.tran.rotation, inst.tran.rotation.transposed(), inst.tran.position});
    }

    std::vector<Contact> contacts;
    for (const SamplePoint &sp : samples) {
      const Vec3f p = R * sp.x + currentT;
      for (size_t m = 0; m < neighbors.size(); m++) {
        const Neighbor &nb = neighbors[m];
        Vec3f nLocal(0, 0, 0);
        const float d = nb.set->Distance(nb.rotInv * (p - nb.pos), queryRadius, &nLocal);
        if (stats) stats->pointTests++;
        if (d < queryRadius) contacts.push_back({p, nb.rot * nLocal, d, int(m)});
      }
      // container wall: signed by which side of the wall the sample is on.
      if (scene.containerGrid.InRange(p, queryRadius)) {
        const ContactInfo info = scene.containerGrid.NearestTriangle(p, queryRadius);
        if (info.dist >= 0.0f && info.dist < queryRadius) {
          float signedDist = info.dist;
          if ((p - info.closestPt).dot(info.normal) < 0.0f) signedDist = -info.dist;
          contacts.push_back({p, info.normal, signedDist, -1});
        }
      }
    }
    if (stats) stats->contacts += contacts.size();
    contacts = reduceContactManifold(contacts, contactAngleThreshDeg);
    if (stats) stats->contactMs += contactClock.ElapsedMS();

    SolveContactConstraintsPGS(contacts, currentT, R, Rinv, invMass, invI_local, eps,
                               int(sim.pgsPasses), dt, linearVel, angularVel);

    currentT += linearVel * dt;
    currentQ = IntegrateAngularVelocity(angularVel, dt) * currentQ;
    currentQ.normalize();
    linearVel *= sim.damping;
    angularVel *= sim.damping;
    trajectory.push_back(RigidTransform(
        currentT, Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w())));
    if (stats) stats->steps++;

    if (linearVel.dot(linearVel) < sim.minVel && angularVel.dot(angularVel) < sim.minVel) break;
    recentPos.push_back(currentT);
    recentQ.push_back(currentQ);
    if (recentPos.size() > settleWindow) {
      const size_t old = recentPos.size() - 1 - settleWindow;
      const Quat4f &q0 = recentQ[old];
      const float qdot = std::min(1.0f, std::fabs(q0.w() * currentQ.w() + q0.x() * currentQ.x() +
                                                   q0.y() * currentQ.y() + q0.z() * currentQ.z()));
      if ((currentT - recentPos[old]).norm() < settlePosTol && 2.0f * std::acos(qdot) < settleAngleTol) break;
    }
  }
  out.position = currentT;
  out.rotation = Matrix3f::rotation(currentQ.x(), currentQ.y(), currentQ.z(), currentQ.w());
  if (stats) stats->totalMs += total.ElapsedMS();
  return out;
}
