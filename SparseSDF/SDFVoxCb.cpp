#include "SDFVoxCb.h"
#include "PointTrigDist.h"

void SDFVoxCb::operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
  Vec3u coarseIdx(x / 4, y / 4, z / 4);
  // update distances of 8 vertices in the sdf->dist array
  Triangle trig = m->GetTriangleVerts(trigIdx);
  const unsigned NUM_CUBE_VERTS = 8;
  const unsigned CUBE_VERTS[NUM_CUBE_VERTS][3] = {
      {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
  for (unsigned ci = 0; ci < NUM_CUBE_VERTS; ci++) {
    unsigned vx = x + CUBE_VERTS[ci][0];
    unsigned vy = y + CUBE_VERTS[ci][1];
    unsigned vz = z + CUBE_VERTS[ci][2];
    Vec3f vertCoord(vx * sdf->voxSize[0] + sdf->origin[0],
                    vy * sdf->voxSize[1] + sdf->origin[1],
                    vz * sdf->voxSize[2] + sdf->origin[2]);
    TrigDistInfo distInfo = PointTrigDist(vertCoord, (float*)(&trig));
    float d = std::sqrt(distInfo.sqrDist);
    d = d / sdf->distUnit;
    sdf->dist(vx, vy, vz) = std::min(sdf->dist(vx, vy, vz), short(d));
  }
}

void SDFVoxCb::BeginTrig(size_t trigIdx) {}

/// free any triangle specific data.
void SDFVoxCb::EndTrig(size_t trigIdx) {}
