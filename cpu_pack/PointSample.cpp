#include "PointSample.h"

#include <iostream>
#include <fstream>
#include <random>
#include <unordered_set>
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
    }
    out.close();
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