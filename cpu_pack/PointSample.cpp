#include "PointSample.h"

#include <iostream>
#include <fstream>
#include <random>
#include <unordered_set>
#include <algorithm>
#include <QuickHull.hpp>
// Custom spatial key for filtering redundant points down to epsilon resolution
struct SpatialKey {
    int x, y, z;
    bool operator==(const SpatialKey &o) const {
      return x == o.x && y == o.y && z == o.z;
    }
};

struct SpatialKeyHash {
    std::size_t operator()(const SpatialKey &k) const {
      // Large prime numbers to minimize hash collisions
      return (k.x * 73856093) ^ (k.y * 19349663) ^ (k.z * 83492791);
    }
};

void SampleMeshSurface(const TrigMesh &mesh, float epsilon, std::vector<SamplePoint> &outSamples) {
  if (mesh.t.empty() || epsilon <= 0.0f)
    return;

  // Initialize Random Number Generator for stratified jittering
  std::mt19937 rng(1337); // Fixed seed for deterministic sample behavior
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  // Grid tracking to filter out redundant point overlaps
  std::unordered_set<SpatialKey, SpatialKeyHash> occupiedCells;

  // Process every single triangle individually
  for (size_t ti = 0; ti < mesh.GetNumTrigs(); ti++) {
    Vec3f A = mesh.GetTriangleVertex(ti, 0);
    Vec3f B = mesh.GetTriangleVertex(ti, 1);
    Vec3f C = mesh.GetTriangleVertex(ti, 2);

    Vec3f AB = B - A;
    Vec3f AC = C - A;
    Vec3f N = AB.cross(AC);
    N.normalize();
    // Calculate the physical lengths of the basis vectors forming the triangle
    float lenAB = std::sqrt(AB.dot(AB));
    float lenAC = std::sqrt(AC.dot(AC));

    // Determine how many steps we must take along each edge to respect epsilon.
    // Using ceil ensures that even ultra-thin slivers get at least 1 subdivision pass.
    int stepsU = std::max(1, static_cast<int>(std::ceil(lenAB / epsilon)));
    int stepsV = std::max(1, static_cast<int>(std::ceil(lenAC / epsilon)));

    // Step through the parametric grid of the triangle
    for (int i = 0; i < stepsU; ++i) {
        for (int j = 0; j < stepsV; ++j) {
        // Get clean, normalized stratified coordinates inside the cell
        float jitterU = dist(rng);
        float jitterV = dist(rng);

        float u = (static_cast<float>(i) + jitterU) / static_cast<float>(stepsU);
        float v = (static_cast<float>(j) + jitterV) / static_cast<float>(stepsV);

        // PERFECT REFLECTION:
        // If we are in the upper-right half of the parallelogram, 
        // fold it back into the lower-left triangle cleanly.
        if (u + v > 1.0f) {
            u = 1.0f - u;
            v = 1.0f - v;
        }

        // Compute final world space coordinate on the triangle face
        Vec3f samplePt = A + (AB * u) + (AC * v);

        // Quantize to the spatial grid cell coordinates
        int gx = static_cast<int>(std::floor(samplePt[0] / epsilon));
        int gy = static_cast<int>(std::floor(samplePt[1] / epsilon));
        int gz = static_cast<int>(std::floor(samplePt[2] / epsilon));

        SpatialKey key{gx, gy, gz};

        // Grid Decimation Gatekeeper: Only keep the point if the voxel cell is empty
        if (occupiedCells.insert(key).second) {
          outSamples.push_back(SamplePoint(samplePt, N));
        }
      }
    }
  }
}

int SamplePoints(const TrigMesh &mesh, float eps, std::vector<SamplePoint> &points) {
  points.clear();
  SampleMeshSurface(mesh, eps, points);
  if (mesh.nv.size() != mesh.GetNumVerts()) {
    return 0;
  }
  for (size_t vi = 0; vi < mesh.GetNumVerts(); vi++) {
    points.push_back(SamplePoint(mesh.Vert(vi), mesh.nv[vi]));
  }
  return 0;
}

void SavePointsObj(const std::string & filename, const std::vector<SamplePoint> & points)
{
    std::ofstream out(filename);
    for(size_t i = 0;i<points.size(); i++){
        out << "v " << points[i].x[0] <<" " <<points[i].x[1] <<" "<<points[i].x[2] <<"\n";
        Vec3f n = points[i].x + 0.2f * points[i].n;
        out << "v " << n[0] << " " << n[1] << " " << n[2] << "\n";
        out << "l " << (2 * i + 1) << " " << (2 * i + 2) << "\n";
    }
    out.close();
}

void SaveVec3fObj(const std::string & filename, const std::vector<Vec3f> & points){
    std::ofstream out(filename);
    for(size_t i = 0;i<points.size(); i++){
        out << "v " << points[i][0] <<" " <<points[i][1] <<" "<<points[i][2] <<"\n";
    }
    out.close();
}

std::vector<Vec3f> SubsampleMeshVertices(const std::vector<Vec3f>& vertices,
                                         float targetSpacing) {
    if (vertices.empty() || targetSpacing <= 0.0f) {
        return vertices;
    }

    std::unordered_set<SpatialKey, SpatialKeyHash> occupiedCells;
    std::vector<Vec3f> subsampledVerts;
    subsampledVerts.reserve(vertices.size() / 4);

    float invSpacing = 1.0f / targetSpacing;

    for (const auto& v : vertices) {
        SpatialKey key;
        key.x = static_cast<int>(std::floor(v[0] * invSpacing));
        key.y = static_cast<int>(std::floor(v[1] * invSpacing));
        key.z = static_cast<int>(std::floor(v[2] * invSpacing));

        if (occupiedCells.insert(key).second) {
            subsampledVerts.push_back(v);
        }
    }

    return subsampledVerts;
}


static Vec3f ConvexHullVertexNormal(size_t vertIdx,
                                   const quickhull::ConvexHull<float> &hull) {
  const auto &indices = hull.getIndexBuffer();
  const auto &verts = hull.getVertexBuffer();
  Vec3f normal(0, 0, 0);
  for (size_t i = 0; i < indices.size(); i += 3) {
    bool found = false;
    for (int j = 0; j < 3; j++) {
      if (indices[i + j] == vertIdx) {
        found = true;
        break;
      }
    }
    if (!found) {
      continue;
    }
    const auto &a = verts[indices[i]];
    const auto &b = verts[indices[i + 1]];
    const auto &c = verts[indices[i + 2]];
    quickhull::Vector3<float> ab = b - a;
    quickhull::Vector3<float> ac = c - a;
    quickhull::Vector3<float> n = ab.crossProduct(ac);
    normal[0] += n.x;
    normal[1] += n.y;
    normal[2] += n.z;
  }
  normal.normalize();
  return normal;
}

static std::vector<SamplePoint> ReduceHullPoints(
    const std::vector<SamplePoint> &hullPoints, float minSpacing,
    unsigned maxPoints) {
  if (hullPoints.size() <= maxPoints) {
    return hullPoints;
  }
  std::vector<SamplePoint> filtered;
  filtered.reserve(maxPoints);
  std::unordered_set<SpatialKey, SpatialKeyHash> occupiedCells;
  float invSpacing = 1.0f / minSpacing;

  for (const auto &sp : hullPoints) {
    SpatialKey key;
    key.x = static_cast<int>(std::floor(sp.x[0] * invSpacing));
    key.y = static_cast<int>(std::floor(sp.x[1] * invSpacing));
    key.z = static_cast<int>(std::floor(sp.x[2] * invSpacing));
    if (occupiedCells.insert(key).second) {
      filtered.push_back(sp);
      if (filtered.size() >= maxPoints) {
        break;
      }
    }
  }
  return filtered;
}

void AddConvexHullSamples(const TrigMesh &mesh,
                          float minSpacing,
                          unsigned maxPoints,
                          std::vector<SamplePoint> &samples) {
  if (mesh.v.empty()) {
    return;
  }
  size_t numVerts = mesh.GetNumVerts();
  std::vector<quickhull::Vector3<float>> pointCloud(numVerts);
  for (size_t i = 0; i < numVerts; i++) {
    pointCloud[i] = quickhull::Vector3<float>(mesh.v[3 * i], mesh.v[3 * i + 1],
                                               mesh.v[3 * i + 2]);
  }
  quickhull::QuickHull<float> qh;
  quickhull::ConvexHull<float> hull =
      qh.getConvexHull(pointCloud, true, false);

  const auto &hullVerts = hull.getVertexBuffer();
  size_t numHullVerts = hullVerts.size();

  std::vector<SamplePoint> hullPoints;
  hullPoints.reserve(numHullVerts);
  for (size_t i = 0; i < numHullVerts; i++) {
    Vec3f pos(hullVerts[i].x, hullVerts[i].y, hullVerts[i].z);
    Vec3f nrm = ConvexHullVertexNormal(i, hull);
    hullPoints.push_back(SamplePoint(pos, nrm));
  }

  auto reduced = ReduceHullPoints(hullPoints, minSpacing, maxPoints);
  samples.insert(samples.end(), reduced.begin(), reduced.end());
}

void TestSamplePoints(){
    float eps = 0.1f;
    TrigMesh mesh;
    mesh.LoadObj("F:/meshes/fruit_hand/fruits/Watermelon.obj");
    std::vector<SamplePoint> points;
    mesh.ComputeVertNormals();
    int ret = SamplePoints(mesh, eps, points);
    if(ret < 0 ){
        std::cout<<"sample points failed\n";
    }
    SavePointsObj("F:/meshes/fruit_hand/debug_sampling.obj", points);


    mesh.LoadObj("F:/meshes/fruit_hand/fruits/blueberries1.obj");    
    mesh.ComputeVertNormals();
    ret = SamplePoints(mesh, eps, points);
    SavePointsObj("F:/meshes/fruit_hand/debug_sampling_blueberries1.obj", points);
}