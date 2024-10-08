#include "Lattice.h"

#include <string>

namespace slicer {

std::string toString(LatticeType type) {
  switch (type) {
    case LatticeType::DIAMOND:
      return "diamond";
    case LatticeType::CUBE:
      return "cube";
    case LatticeType::GYROID:
      return "gyroid";
    case LatticeType::FLUORITE:
      return "fluorite";
    case LatticeType::OCTET:
      return "octet";
    default:
      return "unknown";
  }
}

LatticeType ParseLatticeType(const std::string& s) {
  LatticeType t = LatticeType::DIAMOND;
  if (s == "diamond") {
    t = LatticeType::DIAMOND;
  } else if (s == "cube") {
    t = LatticeType::CUBE;
  } else if (s == "gyroid") {
    t = LatticeType::GYROID;
  } else if (s == "fluorite") {
    t = LatticeType::FLUORITE;
  } else if (s == "octet") {
    t = LatticeType::OCTET;
  }
  return t;
}

static const float INIT_CELL_DIST = 10000.0f;
/// point line segment distance
double PtLineSegDist(const Vec3f& p, const Vec3f& l0, const Vec3f& l1) {
  Vec3f l = l1 - l0;
  Vec3f l0p = p - l0;
  if (l0p.dot(l) <= 0.0f || l.norm() < 1e-6) {
    return l0p.norm();
  }
  Vec3f l1p = p - l1;
  if (l1p.dot(l) >= 0.0f) {
    return l1p.norm();
  }
  Vec3f c = l.cross(l0p);
  return c.norm() / l.norm();
}

/// draw distance field for a beam on top of existing grid.
///  very inefficient. called only during init.
void DrawBeamDist(const Vec3f& p0, const Vec3f& p1, Array3Df& grid, const Vec3f& voxelSize) {
  Vec3u s = grid.GetSize();
  for (unsigned x = 0; x < s[0]; x++) {
    for (unsigned y = 0; y < s[1]; y++) {
      for (unsigned z = 0; z < s[2]; z++) {
        Vec3f p(x, y, z);
        p = p * voxelSize;
        double d = PtLineSegDist(p, p0, p1);
        if (d < grid(x, y, z)) {
          grid(x, y, z) = d;
        }
      }
    }
  }
}

void DrawBeams(unsigned numBeams, float beams[][6], Array3Df& grid, const Vec3f& voxelSize,
               const Vec3f& cellSize) {
  for (unsigned b = 0; b < numBeams; b++) {
    Vec3f p0(beams[b][0], beams[b][1], beams[b][2]);
    Vec3f p1(beams[b][3], beams[b][4], beams[b][5]);
    p0 = cellSize * p0;
    p1 = cellSize * p1;
    DrawBeamDist(p0, p1, grid, voxelSize);
  }
}

Array3Df MakeCubeCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize) {
  Array3Df grid;
  grid.Allocate(gridSize, INIT_CELL_DIST);
  const unsigned NUM_BEAMS = 3;
  float beams[NUM_BEAMS][6] = {
      {0, 0.5, 0.5, 1, 0.5, 0.5}, {0.5, 0, 0.5, 0.5, 1, 0.5}, {0.5, 0.5, 0, 0.5, 0.5f, 1}};
  DrawBeams(NUM_BEAMS, beams, grid, voxelSize, cellSize);
  return grid;
}

Array3Df MakeDiamondCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize) {
  Array3Df grid;
  grid.Allocate(gridSize, INIT_CELL_DIST);
  const unsigned NUM_BEAMS = 28;
  // 16 beams within the cell and 12 beams for beams outside of the cell.
  float beams[NUM_BEAMS][6] = {{0.25, 0.25, 0.25, 0, 0, 0},      {0.25, 0.25, 0.25, .5f, .5f, 0},
                               {0.25, 0.25, 0.25, .5f, 0, .5f},  {0.25, 0.25, 0.25, 0, .5f, .5f},

                               {0.75, 0.75, 0.25, 1, 1, 0},      {0.75, 0.75, 0.25, .5f, .5f, 0},
                               {0.75, 0.75, 0.25, 1, .5f, .5f},  {0.75, 0.75, 0.25, .5f, 1, .5f},

                               {0.25, 0.75, 0.75, .5f, .5f, 1},  {0.25, 0.75, 0.75, 0, 1, 1},
                               {0.25, 0.75, 0.75, 0, .5f, .5f},  {0.25, 0.75, 0.75, .5f, 1, 0.5},

                               {0.75, 0.25, 0.75, 1, 0, 1},      {0.75, 0.25, 0.75, .5f, 0, .5f},
                               {0.75, 0.25, 0.75, 1, .5f, 0.5f}, {0.75, 0.25, 0.75, 0.5f, .5f, 1},

                               {1, 0, 0, 1.25, 0.25, 0.25},      {1, 0, 0, 0.75, -0.25, 0.25},
                               {1, 0, 0, 0.75, 0.75, -0.25},

                               {0, 1, 0, -0.25, 0.75, 0.25},     {0, 1, 0, 0.25, 1.25, 0.25},
                               {0, 1, 0, 0.75, 0.75, -0.25},

                               {0, 0, 1, -0.25, 0.25, 0.75},     {0, 0, 1, 0.25, -0.25, 0.75},
                               {0, 0, 1, 0.25, 0.25, 1.25},

                               {1, 1, 1, 1.25, 0.75, 0.75},      {1, 1, 1, 0.75, 1.25, 0.75},
                               {1, 1, 1, 0.75, 0.75, 1.25}};

  DrawBeams(NUM_BEAMS, beams, grid, voxelSize, cellSize);
  return grid;
}

Array3Df MakeGyroidCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize) {
  Array3Df grid;
  grid.Allocate(gridSize, INIT_CELL_DIST);
  float twoPi = 6.2831853f;
  float maxVal = 1.3f;
  Vec3u size = grid.GetSize();
  float distScale = 0.32f * cellSize[0] / std::abs(maxVal);
  for (unsigned z = 0; z < size[2]; z++) {
    float fz = float(z) / size[2] * twoPi;
    for (unsigned y = 0; y < size[1]; y++) {
      float fy = float(y) / size[1] * twoPi;
      for (unsigned x = 0; x < size[0]; x++) {
        float fx = float(x) / size[0] * twoPi;
        float distVal =
            std::sin(fx) * std::cos(fy) + std::sin(fy) * std::cos(fz) + std::sin(fz) * std::cos(fx);
        grid(x, y, z) = distScale * std::abs(distVal);
      }
    }
  }
  return grid;
}

Array3Df MakeFluoriteCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize) {
  Array3Df grid;
  grid.Allocate(gridSize, INIT_CELL_DIST);
  const unsigned NUM_BEAMS = 32;
  float beams[NUM_BEAMS][6];
  const unsigned NUM_F_VERTS = 8;
  const unsigned NUM_CA_VERTS = 14;
  Vec3f FVerts[NUM_F_VERTS] = {{0.25f, 0.25f, 0.25f}, {0.75f, 0.25f, 0.25f}, {0.25f, 0.75f, 0.25f},
                               {0.75f, 0.75f, 0.25f}, {0.25f, 0.25f, 0.75f}, {0.75f, 0.25f, 0.75f},
                               {0.25f, 0.75f, 0.75f}, {0.75f, 0.75f, 0.75f}};
  Vec3f CaVerts[NUM_CA_VERTS] = {{0, 0, 0},       {1, 0, 0},       {0, 1, 0},       {1, 1, 0},
                                 {0, 0, 1},       {1, 0, 1},       {0, 1, 1},       {1, 1, 1},
                                 {0.5f, 0.5f, 0}, {0.5f, 0.5f, 1}, {0.5f, 0, 0.5f}, {0.5f, 1, 0.5f},
                                 {0, 0.5f, 0.5f}, {1, 0.5f, 0.5f}};
  unsigned beamIndex = 0;
  float distThresh = 0.44f;
  for (unsigned i = 0; i < NUM_F_VERTS; i++) {
    for (unsigned j = 0; j < NUM_CA_VERTS; j++) {
      float dist = (FVerts[i] - CaVerts[j]).norm();
      if (dist < distThresh && beamIndex < NUM_BEAMS) {
        for (unsigned d = 0; d < 3; d++) {
          beams[beamIndex][d] = FVerts[i][d];
          beams[beamIndex][d + 3] = CaVerts[j][d];
        }
        beamIndex++;
      }
    }
  }
  DrawBeams(NUM_BEAMS, beams, grid, voxelSize, cellSize);
  return grid;
}

Array3Df MakeOctetCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize) {
  Array3Df grid;
  grid.Allocate(gridSize, INIT_CELL_DIST);
  const unsigned NUM_BEAMS = 20;
  float beams[NUM_BEAMS][6] = {{0, 0, 0, 1, 1, 0},
                               {1, 0, 0, 0, 1, 0},
                               {0, 0, 1, 1, 1, 1},
                               {1, 0, 1, 0, 1, 1},
                               {0, 0, 0, 1, 0, 1},
                               {1, 0, 0, 0, 0, 1},
                               {0, 1, 0, 1, 1, 1},
                               {1, 1, 0, 0, 1, 1},
                               {0, 0, 0, 0, 1, 1},
                               {0, 0, 1, 0, 1, 0},
                               {1, 0, 0, 1, 1, 1},
                               {1, 0, 1, 1, 1, 0},

                               {0.5f, 0.5f, 0, 0.5f, 0, 0.5f},
                               {0.5f, 0.5f, 0, 0.5f, 1, 0.5f},
                               {0.5f, 0.5f, 0, 0, 0.5f, 0.5f},
                               {0.5f, 0.5f, 0, 1, 0.5f, 0.5f},
                               {0.5f, 0.5f, 1, 0.5f, 0, 0.5f},
                               {0.5f, 0.5f, 1, 0.5f, 1, 0.5f},
                               {0.5f, 0.5f, 1, 0, 0.5f, 0.5f},
                               {0.5f, 0.5f, 1, 1, 0.5f, 0.5f}};

  DrawBeams(NUM_BEAMS, beams, grid, voxelSize, cellSize);
  return grid;
}

Array3Df MakeShearXCell(Vec3f voxelSize, Vec3f cellSize, Vec3u gridSize) {
  Array3Df grid;
  grid.Allocate(gridSize, INIT_CELL_DIST);
  const unsigned NUM_BEAMS = 12 + 2 * 4;
  float beams[NUM_BEAMS][6] = {
      {0, 0, 0, 1, 0, 0},   {0, 0, 1, 1, 0, 1},   {0, 1, 1, 1, 1, 1},
      {0, 1, 0, 1, 1, 0},   {0, 0, 0, 0, 1, 0},   {0, 0, 1, 0, 1, 1},
      {1, 0, 0, 1, 1, 0},   {1, 0, 1, 1, 1, 1},   {0, 0, 0, 0.2, 0, 1},
      {0, 1, 0, 0.2, 1, 1}, {0.8, 0, 0, 1, 0, 1}, {0.8, 1, 0, 1, 1, 1},
      {0, 0, 0, 0.2, 1, 1}, {0.2, 0, 1, 0, 1, 0}, {0.8, 0, 0, 1, 1, 1},
      {0.8, 1, 0, 1, 0, 1}, {0, 0, 0, 1, 1, 0},   {1, 0, 0, 0, 1, 0},
      {0, 0, 1, 1, 1, 1},   {0, 1, 1, 1, 0, 1}};

  DrawBeams(NUM_BEAMS, beams, grid, voxelSize, cellSize);
  return grid;
}

}  // namespace slicer
