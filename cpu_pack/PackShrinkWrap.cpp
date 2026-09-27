#include "PackShrinkWrap.h"

#include "AdapSDF.h"
#include "AdapUDF.h"
#include "FastSweep.h"
#include "Log.h"
#include "MarchingCubes.h"
#include "MeshOps.h"
#include "PackingScene.h"
#include "cpu_voxelizer.h"
#include "FloodOutside.h"
#include "BBox.h"
#include "meshutil.h"

#include <algorithm>
#include <deque>

namespace {

size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u &size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

Vec3u GridIdx(size_t l, const Vec3u &size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}

bool InBound(int x, int y, int z, const Vec3u &size) {
  return x >= 0 && y >= 0 && z >= 0 && (unsigned)x < size[0] &&
         (unsigned)y < size[1] && (unsigned)z < size[2];
}

void FloodOutsideSeed(Vec3u seed, const Array3D<short> &dist, float distThresh,
                      Array3D8u &label) {
  Vec3u size = dist.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  while (!q.empty()) {
    size_t linearIdx = q.front();
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    label(idx[0], idx[1], idx[2]) = 1;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      uint8_t nbrLabel = label(ux, uy, uz);
      size_t nbrLinear = LinearIdx(ux, uy, uz, size);
      if (nbrLabel == 0) {
        short nbrDist = dist(ux, uy, uz);
        if (nbrDist >= distThresh) {
          label(ux, uy, uz) = 1;
          q.push_back(nbrLinear);
        }
      }
    }
  }
}

Array3D8u FloodOutsideShrink(const Array3D<short> &dist, float distThresh) {
  Array3D8u label;
  Vec3u size = dist.GetSize();
  label.Allocate(size[0], size[1], size[2]);
  label.Fill(0);

  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      if (y == 0 || y == size[1] - 1 || z == 0 || z == size[2] - 1) {
        for (unsigned x = 0; x < size[0]; x++) {
          label(x, y, z) = 1;
        }
        continue;
      }
      label(0, y, z) = 1;
      label(size[0] - 1, y, z) = 1;
      for (unsigned x = 1; x < size[0] - 1; x++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned x = size[0] - 1; x > 0; x--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned z = 1; z < size[2] - 1; z++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned z = size[2] - 1; z > 0; z--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      for (unsigned y = 1; y < size[1] - 1; y++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned y = size[1] - 1; y > 0; y--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1},
                                {1, 0, 0},  {0, 1, 0},  {0, 0, 1}};
  for (unsigned z = 1; z < size[2] - 1; z++) {
    for (unsigned y = 1; y < size[1] - 1; y++) {
      for (unsigned x = 1; x < size[0] - 1; x++) {
        for (unsigned ni = 0; ni < NUM_NBR; ni++) {
          unsigned ux = unsigned(x + nbrOffset[ni][0]);
          unsigned uy = unsigned(y + nbrOffset[ni][1]);
          unsigned uz = unsigned(z + nbrOffset[ni][2]);

          if (!label(x, y, z)) {
            continue;
          }
          uint8_t nbrLabel = label(ux, uy, uz);
          if (nbrLabel == 0) {
            short nbrDist = dist(ux, uy, uz);
            if (nbrDist >= distThresh) {
              FloodOutsideSeed(Vec3u(x, y, z), dist, distThresh, label);
            }
          }
        }
      }
    }
  }
  return label;
}

// closing step: dist must already be a valid unsigned distance-to-solid
// field (0 inside/at solid, unsigned distance outside), accurate near the
// surface. Applies shrinkwrap closing with the given radius in place.
// Split out from the old ComputeShrinkWrapDistField so both the accurate
// AdapUDF-seeded path and (if ever needed) a cheap voxelize-seeded path can
// share the same closing math.
void ApplyShrinkWrapClosing(Array3D<short> &dist, float voxelSize,
                            float shrinkRadius, float distUnit) {
  Vec3u size = dist.GetSize();
  const short MAX_DIST = 32700;

  float maxDist = std::max(2.0f, shrinkRadius / voxelSize);
  float outsideThresh = (shrinkRadius - voxelSize) / distUnit;
  Array3D8u frozen = FloodOutsideShrink(dist, outsideThresh);

  Array3D<short> borderDist;
  borderDist.Allocate(size[0], size[1], size[2]);
  for (size_t i = 0; i < frozen.GetData().size(); i++) {
    if (frozen.GetData()[i] > 0) {
      borderDist.GetData()[i] =
          std::max(short(0), short(shrinkRadius / distUnit - dist.GetData()[i]));
      frozen.GetData()[i] = 1;
    } else {
      borderDist.GetData()[i] = MAX_DIST;
    }
  }

  FastSweepParUnsigned(borderDist, voxelSize, distUnit, maxDist + 2, frozen);

  for (size_t i = 0; i < borderDist.GetData().size(); i++) {
    if (frozen.GetData()[i]) {
      dist.GetData()[i] = MAX_DIST;
      continue;
    }
    short d = borderDist.GetData()[i];
    if (d >= MAX_DIST) {
      d = -2 * voxelSize / distUnit;
    } else {
      d = short((shrinkRadius - 0.5f * voxelSize) / distUnit - d);
    }
    dist.GetData()[i] = std::min(dist.GetData()[i], d);
  }
}

Vec3i RoundVec(const Vec3f &fvec) {
  return Vec3i(std::round(fvec[0]), std::round(fvec[1]), std::round(fvec[2]));
}

TrigMesh MergeInstanceMeshes(const PackingScene &scene) {
  TrigMesh merged;
  for (const auto &inst : scene.instances) {
    merged.append(MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran));
  }
  return merged;
}

// seeds an unsigned distance-to-solid field (0 inside/at solid, unsigned
// distance outside) from mesh, sub-voxel accurate near the surface via
// AdapUDF's exact point-triangle distance -- unlike voxelize+corner-zero,
// this does not push the zero level set outward by up to a voxel, and it
// does not need the mesh to be watertight/correctly wound (AdapUDF is an
// unsigned field; AdapSDF would need reliable winding to tell inside from
// outside, which per-fruit meshes do not guarantee).
// band is in voxels and must cover shrinkRadius/voxelSize + a couple more
// for the second sweep in ApplyShrinkWrapClosing; AdapDF::MAX_BAND (16)
// caps how far this can reach, so very large shrinkRadius/voxelSize
// ratios are not supported by this path.
void BuildUnsignedDistField(TrigMesh &mesh, float voxelSize, float distUnit,
                            unsigned band, Array3D<short> &dist, Vec3f &origin) {
  AdapUDF udf;
  udf.voxSize = voxelSize;
  udf.distUnit = distUnit;
  udf.band = band;
  // mesh is a merge of per-instance meshes (MergeInstanceMeshes). Each
  // instance's triangle normals (nt) were computed in the item's local
  // frame and MakeTransformedMesh rotates vertex positions but not nt, so
  // nt on the merged mesh is stale for any rotated instance. Recompute it
  // fresh here rather than trust what came in -- AdapDF::ComputeCoarseDist
  // reads GetTrigNormal() (backed by nt) directly, it does not recompute
  // normals from the current vertex positions itself.
  mesh.ComputeTrigNormals();
  udf.BuildTrigList(&mesh);
  udf.Compress();
  mesh.ComputePseudoNormals();
  udf.ComputeCoarseDist();
  Array3D8u frozen;
  udf.FastSweepCoarse(frozen);
  dist = udf.dist;
  origin = udf.origin;
}

}  // namespace

Array3D8u VoxelizeItems(PackingScene &scene, float voxelSize, Vec3f &outOrigin) {
  Box3f totalBox;
  totalBox.vmin = Vec3f(1e10f, 1e10f, 1e10f);
  totalBox.vmax = Vec3f(-1e10f, -1e10f, -1e10f);

  for (const auto &inst : scene.instances) {
    unsigned itemIdx = inst.itemId;
    const RigidTransform &tran = inst.tran;
    TrigMesh transformedMesh = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
    Box3f bbox = ComputeBBox(transformedMesh.v);
    totalBox.vmin = Vec3f(std::min(totalBox.vmin[0], bbox.vmin[0]),
                          std::min(totalBox.vmin[1], bbox.vmin[1]),
                          std::min(totalBox.vmin[2], bbox.vmin[2]));
    totalBox.vmax = Vec3f(std::max(totalBox.vmax[0], bbox.vmax[0]),
                          std::max(totalBox.vmax[1], bbox.vmax[1]),
                          std::max(totalBox.vmax[2], bbox.vmax[2]));
  }

  totalBox.vmin = AlignOriginToGrid(totalBox.vmin, voxelSize);
  totalBox.vmax = AlignMaxToGrid(totalBox.vmax, voxelSize);
  totalBox.vmax += 2.0f *Vec3f(voxelSize, voxelSize, voxelSize);
  totalBox.vmin -= 2.0f *Vec3f(voxelSize, voxelSize, voxelSize);

  // one origin convention everywhere: outOrigin is the min corner of voxel
  // (0,0,0), same as VoxConf::origin. it sits on the floor(x/voxelSize)
  // lattice that every per-item box is also snapped to, so the per-item index
  // offsets below are exact integers. no half voxel shifts anywhere.
  outOrigin = totalBox.vmin;

  Vec3u gridSize = ComputeGridSize(totalBox, voxelSize, 1);
  Array3D8u allVox;
  allVox.Allocate(gridSize[0], gridSize[1], gridSize[2]);
  allVox.Fill(0);

  unsigned clippedItemCount = 0;
  for (size_t instIdx = 0; instIdx < scene.instances.size(); instIdx++) {
    const auto &inst = scene.instances[instIdx];
    unsigned itemIdx = inst.itemId;
    const RigidTransform &tran = inst.tran;

    TrigMesh transformedMesh = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
    Box3f bbox = ComputeBBox(transformedMesh.v);
    // floor vmin (safe, only grows the box outward), ceil vmax (required --
    // flooring vmax can cut into the mesh by up to 1 voxel). Then pad BOTH
    // sides by the same 2 voxels so FloodOutside8u always has a clear,
    // symmetric border to flood from. An asymmetric pad here previously
    // caused a sign-dependent one-voxel clipping bug on the low side of
    // each axis.
    bbox.vmin = AlignOriginToGrid(bbox.vmin, voxelSize);
    bbox.vmax = AlignMaxToGrid(bbox.vmax, voxelSize);
    bbox.vmax += 2.0f * Vec3f(voxelSize, voxelSize, voxelSize);
    bbox.vmin -= 2.0f * Vec3f(voxelSize, voxelSize, voxelSize);

    VoxConf conf;
    conf.origin = bbox.vmin;
    conf.unit = {voxelSize, voxelSize, voxelSize};
    conf.gridSize = ComputeGridSize(bbox, voxelSize, 1);

    Array3D8u itemVox;
    itemVox.Allocate(conf.gridSize, 0);
    VoxelizeMesh(transformedMesh, itemVox, conf);
    FloodOutside8u(itemVox, 1, 2);

    // clipping check: a voxelized item should never touch the padded grid
    // boundary. If it does, the border was not clear and FloodOutside8u
    // could not distinguish inside from outside on that face, so the item
    // silently loses its interior fill there.
    Vec3u itemSize = conf.gridSize;
    bool touchesBoundary = false;
    for (unsigned z = 0; z < itemSize[2] && !touchesBoundary; z++) {
      for (unsigned y = 0; y < itemSize[1] && !touchesBoundary; y++) {
        if (itemVox(0, y, z) != 0 || itemVox(itemSize[0] - 1, y, z) != 0) {
          touchesBoundary = true;
        }
      }
    }
    for (unsigned z = 0; z < itemSize[2] && !touchesBoundary; z++) {
      for (unsigned x = 0; x < itemSize[0] && !touchesBoundary; x++) {
        if (itemVox(x, 0, z) != 0 || itemVox(x, itemSize[1] - 1, z) != 0) {
          touchesBoundary = true;
        }
      }
    }
    for (unsigned y = 0; y < itemSize[1] && !touchesBoundary; y++) {
      for (unsigned x = 0; x < itemSize[0] && !touchesBoundary; x++) {
        if (itemVox(x, y, 0) != 0 || itemVox(x, y, itemSize[2] - 1) != 0) {
          touchesBoundary = true;
        }
      }
    }
    if (touchesBoundary) {
      clippedItemCount++;
      LOGI("shrinkwrap: instance " << instIdx << " (item " << itemIdx
                                   << ") voxelization touches its padded "
                                      "grid boundary -- likely clipped\n");
    }

    Vec3i offset = RoundVec((1.0f / voxelSize) * (conf.origin - outOrigin));

    for (unsigned z = 0; z < conf.gridSize[2]; z++) {
      int gz = int(z) + offset[2];
      if (gz < 0 || (unsigned)gz >= gridSize[2]) continue;
      for (unsigned y = 0; y < conf.gridSize[1]; y++) {
        int gy = int(y) + offset[1];
        if (gy < 0 || (unsigned)gy >= gridSize[1]) continue;
        for (unsigned x = 0; x < conf.gridSize[0]; x++) {
          if (itemVox(x, y, z) != 0) {
            int gx = int(x) + offset[0];
            if (gx >= 0 && (unsigned)gx < gridSize[0]) {
              allVox((unsigned)gx, (unsigned)gy, (unsigned)gz) = 1;
            }
          }
        }
      }
    }
  }

  LOGI("shrinkwrap: " << clippedItemCount << "/" << scene.instances.size()
                      << " item voxelizations touched their padded grid "
                         "boundary\n");

  return allVox;
}

void ComputeShrinkWrapDistField(PackingScene &scene, float shrinkRadius,
                                float voxelSize, Array3D<short> &dist,
                                Vec3f &origin, float &distUnit,
                                Array3D<short> *rawOut) {
  distUnit = 0.01f;
  dist = Array3D<short>();
  origin = Vec3f(0.0f, 0.0f, 0.0f);
  if (scene.instances.empty()) {
    return;
  }
  TrigMesh merged = MergeInstanceMeshes(scene);

  unsigned band = unsigned(std::min(
      float(AdapDF::MAX_BAND), std::max(2.0f, shrinkRadius / voxelSize) + 2.0f));

  BuildUnsignedDistField(merged, voxelSize, distUnit, band, dist, origin);
  if (rawOut != nullptr) {
    *rawOut = dist;
  }
  ApplyShrinkWrapClosing(dist, voxelSize, shrinkRadius, distUnit);
}

TrigMesh ComputeShrinkWrapMesh(const Array3D<short> &dist, const Vec3f &origin,
                               float voxelSize, float distUnit) {
  TrigMesh outMesh;
  if (dist.GetSize()[0] == 0) {
    return outMesh;
  }
  MarchingCubes(dist, 0.0f, distUnit, voxelSize, origin, &outMesh);
  LOGI("shrinkwrap: " << outMesh.GetNumTrigs() << " triangles, voxel size "
                       << voxelSize << " cm\n");
  return outMesh;
}

void SaveShrinkWrapMesh(const TrigMesh &mesh, const std::string &filename) {
  mesh.SaveObj(filename);
  LOGI("saved shrinkwrap mesh to " << filename << "\n");
}

VoidField ComputeVoidField(PackingScene &scene, float shrinkRadius, float voxelSize) {
  VoidField vf;
  vf.voxelSize = voxelSize;
  Array3D<short> raw;
  ComputeShrinkWrapDistField(scene, shrinkRadius, voxelSize, vf.dist, vf.origin,
                             vf.distUnit, &raw);
  if (vf.dist.GetSize()[0] == 0) {
    return vf;
  }

  const short MAX_DIST = 32700;
  // vf.dist currently holds the CLOSED field (post-ApplyShrinkWrapClosing):
  // <= 0 inside the manufactured hull (fruit + small closed gaps), > 0
  // outside its silhouette entirely. raw holds distance-to-nearest-fruit
  // from before closing. Overwrite vf.dist in place: keep raw's value only
  // where the closed field says "inside the hull"; block everywhere else,
  // regardless of how that point reads relative to the container. This is
  // deliberately container-independent -- see the VoidField comment.
  for (size_t i = 0; i < vf.dist.GetData().size(); i++) {
    short closed = vf.dist.GetData()[i];
    vf.dist.GetData()[i] = (closed <= 0) ? raw.GetData()[i] : -MAX_DIST;
  }
  return vf;
}

VoidField ComputeShrinkwrapField(PackingScene &scene, float shrinkRadius, float voxelSize,
                                 unsigned skinBandVoxels) {
  VoidField vf;
  vf.voxelSize = voxelSize;
  Array3D<short> raw;
  ComputeShrinkWrapDistField(scene, shrinkRadius, voxelSize, vf.dist, vf.origin,
                             vf.distUnit, &raw);
  if (vf.dist.GetSize()[0] == 0) {
    return vf;
  }

  const short MAX_DIST = 32700;
  float halfBand = float(std::max(1u, skinBandVoxels)) * voxelSize;
  // vf.dist currently holds the CLOSED field. Keep raw's value only at
  // voxels within halfBand of the closed field's own zero crossing -- the
  // shrinkwrap SKIN itself, not the volume behind it (see this function's
  // doc comment). Everywhere else is blocked, same sentinel VoidField uses.
  for (size_t i = 0; i < vf.dist.GetData().size(); i++) {
    float closedPhys = float(vf.dist.GetData()[i]) * vf.distUnit;
    bool onSkin = std::fabs(closedPhys) <= halfBand;
    vf.dist.GetData()[i] = onSkin ? raw.GetData()[i] : -MAX_DIST;
  }
  return vf;
}

EnvelopeField ComputeEnvelopeField(PackingScene &scene, float voxelSize) {
  EnvelopeField ef;
  ef.voxelSize = voxelSize;
  ef.distUnit = 0.01f;
  if (scene.instances.empty()) {
    return ef;
  }
  TrigMesh merged = MergeInstanceMeshes(scene);
  BuildUnsignedDistField(merged, voxelSize, ef.distUnit, AdapDF::MAX_BAND, ef.dist, ef.origin);
  return ef;
}

float SampleDistField(const Array3D<short> &dist, const Vec3f &origin, float voxelSize,
                      float distUnit, const Vec3f &worldPos) {
  Vec3f local = (worldPos - origin) * (1.0f / voxelSize);
  int ix = int(std::floor(local[0]));
  int iy = int(std::floor(local[1]));
  int iz = int(std::floor(local[2]));
  Vec3u size = dist.GetSize();
  if (ix < 0 || iy < 0 || iz < 0 || (unsigned)ix >= size[0] || (unsigned)iy >= size[1] ||
      (unsigned)iz >= size[2]) {
    return 1e4f;
  }
  return float(dist((unsigned)ix, (unsigned)iy, (unsigned)iz)) * distUnit;
}


namespace {

Array3D8u ThresholdMask(const Array3D<short> &dist, float distUnit, float threshold) {
  Vec3u size = dist.GetSize();
  Array3D8u mask;
  mask.Allocate(size, 0);
  short rawThresh = short(threshold / distUnit);
  for (size_t i = 0; i < dist.GetData().size(); i++) {
    mask.GetData()[i] = (dist.GetData()[i] > rawThresh) ? 1 : 0;
  }
  return mask;
}

const int kFaceNbr6[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                             {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

// 1 voxel of erosion: a foreground voxel survives only if all 6 face
// neighbors are also foreground (grid boundary counts as background, so
// this also erodes away from the domain edge).
Array3D8u ErodeOnce(const Array3D8u &mask) {
  Vec3u size = mask.GetSize();
  Array3D8u out;
  out.Allocate(size, 0);
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (mask(x, y, z) == 0) {
          continue;
        }
        bool allSolid = true;
        for (unsigned n = 0; n < 6 && allSolid; n++) {
          int nx = int(x) + kFaceNbr6[n][0];
          int ny = int(y) + kFaceNbr6[n][1];
          int nz = int(z) + kFaceNbr6[n][2];
          if (!InBound(nx, ny, nz, size) ||
              mask((unsigned)nx, (unsigned)ny, (unsigned)nz) == 0) {
            allSolid = false;
          }
        }
        out(x, y, z) = allSolid ? 1 : 0;
      }
    }
  }
  return out;
}

// 1 voxel of dilation: a background voxel becomes foreground if any of its
// 6 face neighbors already is.
Array3D8u DilateOnce(const Array3D8u &mask) {
  Vec3u size = mask.GetSize();
  Array3D8u out = mask;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (mask(x, y, z) != 0) {
          continue;
        }
        bool anySolid = false;
        for (unsigned n = 0; n < 6 && !anySolid; n++) {
          int nx = int(x) + kFaceNbr6[n][0];
          int ny = int(y) + kFaceNbr6[n][1];
          int nz = int(z) + kFaceNbr6[n][2];
          if (InBound(nx, ny, nz, size) &&
              mask((unsigned)nx, (unsigned)ny, (unsigned)nz) != 0) {
            anySolid = true;
          }
        }
        out(x, y, z) = anySolid ? 1 : 0;
      }
    }
  }
  return out;
}

// erode then dilate by the same radius: kills thin webs/jagged crust that
// survived the threshold (erosion), then puffs the surviving cores back to
// their real boundary (dilation). plan.txt CANDIDATE SPOT MANAGEMENT.
Array3D8u MorphOpen(const Array3D8u &mask, unsigned radiusVoxels) {
  Array3D8u cur = mask;
  for (unsigned i = 0; i < radiusVoxels; i++) {
    cur = ErodeOnce(cur);
  }
  for (unsigned i = 0; i < radiusVoxels; i++) {
    cur = DilateOnce(cur);
  }
  return cur;
}

// unsigned distance to cleanedMask's own boundary, valid where cleanedMask
// is 1 (background cells are left at 0). Reuses FastSweepParUnsigned exactly
// like ApplyShrinkWrapClosing does: seed boundary voxels at distance 0,
// frozen, sweep the whole grid, then only the foreground half of the result
// is meaningful.
Array3D<short> DistanceTransformInMask(const Array3D8u &mask, float voxelSize,
                                       float distUnit) {
  Vec3u size = mask.GetSize();
  const short MAX_DIST = 32700;
  Array3D<short> dist;
  dist.Allocate(size, MAX_DIST);
  Array3D8u frozen;
  frozen.Allocate(size, 0);
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (mask(x, y, z) == 0) {
          continue;
        }
        bool isBoundary = false;
        for (unsigned n = 0; n < 6 && !isBoundary; n++) {
          int nx = int(x) + kFaceNbr6[n][0];
          int ny = int(y) + kFaceNbr6[n][1];
          int nz = int(z) + kFaceNbr6[n][2];
          if (!InBound(nx, ny, nz, size) ||
              mask((unsigned)nx, (unsigned)ny, (unsigned)nz) == 0) {
            isBoundary = true;
          }
        }
        if (isBoundary) {
          dist(x, y, z) = 0;
          frozen(x, y, z) = 1;
        }
      }
    }
  }
  float band = float(size[0] + size[1] + size[2]);
  FastSweepParUnsigned(dist, voxelSize, distUnit, band, frozen);
  for (size_t i = 0; i < mask.GetData().size(); i++) {
    if (mask.GetData()[i] == 0) {
      dist.GetData()[i] = 0;
    }
  }
  return dist;
}

// candidate is a local maximum if its value is >= every neighbor within a
// window sized to its OWN value -- a voxel with a wide local void is only
// compared against neighbors roughly that far away, so a deep pocket is not
// out-competed by an unrelated shallow one several widths away.
std::vector<VoidSpot> FindLocalMaxima(const Array3D<short> &dist, const Vec3f &origin,
                                      float voxelSize, float distUnit) {
  Vec3u size = dist.GetSize();
  std::vector<VoidSpot> maxima;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        short raw = dist(x, y, z);
        if (raw <= 0) {
          continue;
        }
        float w = float(raw) * distUnit;
        int r = int(std::ceil(w / voxelSize));
        bool isMax = true;
        for (int dz = -r; dz <= r && isMax; dz++) {
          for (int dy = -r; dy <= r && isMax; dy++) {
            for (int dx = -r; dx <= r && isMax; dx++) {
              if (dx == 0 && dy == 0 && dz == 0) {
                continue;
              }
              int nx = int(x) + dx, ny = int(y) + dy, nz = int(z) + dz;
              if (!InBound(nx, ny, nz, size)) {
                continue;
              }
              short nRaw = dist((unsigned)nx, (unsigned)ny, (unsigned)nz);
              if (nRaw > raw) {
                isMax = false;
              }
            }
          }
        }
        if (isMax) {
          Vec3f pos = origin + Vec3f((x + 0.5f) * voxelSize, (y + 0.5f) * voxelSize,
                                     (z + 0.5f) * voxelSize);
          maxima.push_back({pos, w});
        }
      }
    }
  }
  return maxima;
}

// sort by radius descending; drop a candidate into an already-accepted
// LARGER spot only if their inscribed disks already overlap
// (dist(A,B) < rA+rB, an exact test, not a padded/tuned constant). See
// plan.txt CANDIDATE SPOT MANAGEMENT for why k=1 (touching) is correct and
// should not be rounded up "to be safe".
std::vector<VoidSpot> SuppressNonMaxima(std::vector<VoidSpot> maxima) {
  std::sort(maxima.begin(), maxima.end(),
            [](const VoidSpot &a, const VoidSpot &b) { return a.radius > b.radius; });
  std::vector<VoidSpot> accepted;
  for (const VoidSpot &cand : maxima) {
    bool overlaps = false;
    for (const VoidSpot &acc : accepted) {
      float d = (cand.pos - acc.pos).norm();
      if (d < cand.radius + acc.radius) {
        overlaps = true;
        break;
      }
    }
    if (!overlaps) {
      accepted.push_back(cand);
    }
  }
  return accepted;
}

}  // namespace

VoidSpotResult ExtractVoidSpots(const VoidField &vf, float threshold,
                                unsigned openRadiusVoxels) {
  VoidSpotResult result;
  if (vf.dist.GetSize()[0] == 0) {
    return result;
  }
  Array3D8u mask = ThresholdMask(vf.dist, vf.distUnit, threshold);
  result.cleanedMask = MorphOpen(mask, openRadiusVoxels);
  result.cleanDist =
      DistanceTransformInMask(result.cleanedMask, vf.voxelSize, vf.distUnit);
  std::vector<VoidSpot> maxima =
      FindLocalMaxima(result.cleanDist, vf.origin, vf.voxelSize, vf.distUnit);
  result.spots = SuppressNonMaxima(std::move(maxima));
  return result;
}
