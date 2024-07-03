#include <filesystem>
#include "ElementMesh.h"
#include "TrigMesh.h"
#include <iostream>
#include <fstream>
#include "ArrayUtil.h"
#include "BBox.h"
#include "ImageIO.h"
#include "cpu_voxelizer.h"
#include "VoxCallback.h"
#include "MeshUtil.h"
#include "ElementMeshUtil.h"

namespace fs = std::filesystem;

std::vector<Vec3f> CentralDiffForce(ElementMesh& em, float h) {
  std::vector<Vec3f> f(em.x.size());
  for (size_t i = 0; i < em.x.size(); i++) {
    for (size_t j = 0; j < 3; j++) {
      em.x[i][j] -= h;
      float negEne = em.GetElasticEnergy();

      em.x[i][j] += 2 * h;
      float posEne = em.GetElasticEnergy();

      float force = -(posEne - negEne) / (2 * h);
      f[i][j] = force;
      em.x[i][j] -= h;
    }
  }
  return f;
}

void PrintMat3(const Matrix3f& m, std::ostream& out) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      out << m(i, j) << " ";
    }
    out << "\n";
  }
}

void TestForceFiniteDiff() {
  ElementMesh em;
  em.LoadTxt("F:/github/webgl/fem/data/hex_m.txt");

  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    em.X[i] -= box.vmin;
  }
  // em.SaveTxt("F:/dump/hex_m.txt");
  float ene = em.GetElasticEnergy();
  std::vector<Vec3f> force = em.GetForce();

  std::cout << "E: " << ene << "\n";
  em.fe = std::vector<Vec3f>(em.X.size());
  em.fixedDOF = std::vector<bool>(em.X.size() * 3, false);

  PullRight(Vec3f(0, -1, 0), 0.001, em);
  float h = 0.001;
  std::vector<Vec3f> dx = h * em.fe;
  Add(em.x, dx);
  force = em.GetForce();
  std::vector<Vec3f> refForce = CentralDiffForce(em, 0.0001f);
  for (size_t i = 0; i < force.size(); i++) {
    std::cout << force[i][0] << " " << force[i][1] << " " << force[i][2]
              << ", ";
    Vec3f diff = refForce[i] - force[i];
    if (diff.norm() > 0.1) {
      std::cout << refForce[i][0] << " " << refForce[i][1] << " "
                << refForce[i][2] << " ";
    }
    if (em.fe[i][1] < 0) {
      std::cout << " @";
    }
    std::cout << "\n";
  }

  Matrix3f F(0.8, -0.16, 0, 0.2, 0.99, 0.1, 0.2, 0.3, 1);
  Matrix3f Finv = F.inverse();
  Matrix3f prod = Finv * F;
  PrintMat3(prod, std::cout);
  std::cout << "\n";
  std::cout << F.determinant() << " " << Finv.determinant() << "\n";
}

void TestStiffnessFiniteDiff() {
  ElementMesh em;
  em.LoadTxt("F:/github/webgl/fem/data/hex_m.txt");

  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    em.X[i] -= box.vmin;
  }
  // em.SaveTxt("F:/dump/hex_m.txt");
  float ene = em.GetElasticEnergy();
  std::vector<Vec3f> force = em.GetForce();

  std::cout << "E: " << ene << "\n";
  em.fe = std::vector<Vec3f>(em.X.size());
  em.fixedDOF = std::vector<bool>(em.X.size() * 3, false);

  PullRight(Vec3f(0, -1, 0), 0.001, em);
  float h = 0.001;
  std::vector<Vec3f> dx = h * em.fe;
  Add(em.x, dx);
}

void TestSparseCG(const CSparseMat& K) {
  std::vector<double> x, b, prod;

  unsigned cols = K.Cols();
  x.resize(cols, 0);
  b.resize(cols, 0);
  prod.resize(cols, 0);
  for (size_t col = 0; col < cols; col++) {
    b[col] = 1 + col % 11;
  }
  int ret = CG(K, x, b, 1000);

  K.MultSimple(x.data(), prod.data());
  for (size_t i = 0; i < prod.size(); i++) {
    std::cout << "(" << prod[i] << " " << b[i] << ") ";
  }
}

void TestModes(ElementMesh& em) {
  Array2Df dx = dlmread("F:/dump/modes.txt");
  Vec2u size = dx.GetSize();
  unsigned mode = 11;
  for (unsigned i = 0; i < size[0]; i += 3) {
    unsigned vi = i / 3;
    em.x[vi] =
        em.X[vi] + 0.01f * Vec3f(dx(i, mode), dx(i + 1, mode), dx(i + 2, mode));
  }
  TrigMesh wire;
  std::vector<Edge> edges;
  ComputeWireframeMesh(em, wire, edges, 1, 0.001);

  wire.SaveObj("F:/dump/mode0.obj");
}

void TestSparse() {
  CSparseMat K;
  ElementMesh em;
  em.LoadTxt("F:/github/webgl/fem/data/hex_m.txt");
  em.InitStiffnessPattern();
  em.CopyStiffnessPattern(K);
  em.x[0][0] -= 0.01;

  em.ComputeStiffness(K);
  unsigned rows = K.Rows();
  unsigned cols = K.Cols();
  unsigned nnz = K.NNZ();
  std::cout << "K " << rows << " x " << cols << " , " << nnz << "\n";
  Array2Df dense(cols, rows);
  int ret = K.ToDense(dense.DataPtr());

  Array2Df Kref;
  em.ComputeStiffnessDense(Kref);

  for (unsigned row = 0; row < rows; row++) {
    for (unsigned col = 0; col < cols; col++) {
      float diff = std::abs(Kref(col, row) - dense(col, row));
      if (diff > 0.1f) {
        std::cout << row << " " << col << " " << dense(col, row) << " "
                  << Kref(col, row) << "\n";
      }
    }
  }

  unsigned ei = 10;
  unsigned i = 4;
  float h = 1e-4;

  const Element& ele = *em.e[ei];
  unsigned vi = ele[i];
  const unsigned DIM = 3;
  std::vector<Vec3f> fm, fp;
  for (unsigned d = 0; d < DIM; d++) {
    unsigned col = DIM * vi + d;
    em.x[vi][d] -= h;
    fm = em.GetForce();
    em.x[vi][d] += 2 * h;
    fp = em.GetForce();
    em.x[vi][d] -= h;
    for (size_t j = 0; j < fp.size(); j++) {
      fp[j] -= fm[j];
      fp[j] /= (2 * h);
    }

    for (unsigned row = 0; row < rows; row++) {
      float central = fp[row / 3][row % 3];
      std::cout << central << " " << Kref(col, row) << "\n";
      float diff = std::abs(central + Kref(col, row));
      if (diff > 0.5f) {
        std::cout << row << " " << central << " " << Kref(col, row) << "\n";
      }
    }
  }

  std::vector<float> displacement(rows, 0);
  displacement[0] = 0.001;
  std::vector<float> prod = K.MultSimple(displacement.data());
  std::cout << prod[0] << " " << prod[1] << " " << prod[2] << "\n";

  em.x[0][0] += displacement[0];
  std::vector<Vec3f> force = em.GetForce();
  std::cout << force[0][0] << " " << force[0][1] << " " << force[0][2] << "\n";
  FixLeft(0.001, em);
  FixDOF(em.fixedDOF, K, 1000);
  // K.ToDense(dense.DataPtr());
  // dlmwrite("F:/dump/Kfix.txt", dense);
  // TestModes(em);
  TestSparseCG(K);
}

void CenterMeshes(std::string dir) {
  int starti = 0, endi = 60;
  for (int fi = starti; fi <= endi; fi++) {
    std::string istr = std::to_string(fi);
    std::string modelDir = dir + "/mmp/" + istr + "/models/";
    if (!std::filesystem::exists(modelDir)) {
      continue;
    }
    std::string cageFile = modelDir + "cage.obj";
    if (!std::filesystem::exists(cageFile)) {
      continue;
    }

    fs::path directory_path(modelDir);

    // Create an iterator pointing to the directory
    fs::directory_iterator it(directory_path);
    std::vector<fs::path> paths;
    std::vector<std::string> exts;
    // Iterate through the directory entries
    for (const auto& entry : it) {
      // Check if it's a regular file
      if (entry.is_regular_file()) {
        if (entry.path().filename() == "cage.obj") {
          continue;
        }
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".stl" || ext == ".obj") {
          paths.push_back(entry.path());
          exts.push_back(ext);
        }
      }
    }
    std::vector<TrigMesh> meshes(paths.size());
    BBox box;
    for (size_t i = 0;i<paths.size();i++) {      
      if (exts[i] == ".stl") {
        meshes[i].LoadStl(paths[i].string());
      } else {
        meshes[i].LoadObj(paths[i].string());
      }
      if (i == 0) {
        ComputeBBox(meshes[i].v, box);
      } else {
        UpdateBBox(meshes[i].v, box);
      }
    }
    Vec3f center = 0.5f * (box.vmax + box.vmin);
    for (size_t i = 0; i < paths.size(); i++) {
      for (size_t j = 0; j < meshes[i].GetNumVerts(); j++) {
        meshes[i].v[3 * j] -= center[0];
        meshes[i].v[3 * j + 1] -= center[1];
        meshes[i].v[3 * j + 2] -= center[2];
      }
      if (exts[i] == ".stl") {
        meshes[i].SaveStl(paths[i].string());
      } else {
        meshes[i].SaveObj(paths[i].string());
      }
    }   

  }
}

int PngToGrid(int argc, char** argv) {
  if (argc < 2) {
    return -1;
  }
  std::string pngFile = argv[1];
  std::string outFile = "woodgrain.txt";
  if (argc > 2) {
    outFile = std::string(argv[2]);
  }
  Array2D8u image;
  int ret = LoadPngGrey(pngFile, image);
  if (ret < 0) {
    return 0;
  }
  Vec2u imageSize = image.GetSize();
  float res = 0.064f;
  Vec3u gridSize(imageSize[0], imageSize[1], 30);
  std::ofstream out(outFile);
  out << "voxelSize\n0.064 0.064 0.064\nspacing\n0.2\ndirection\n0 0 1\ngrid\n";
  out << gridSize[0] << " " << gridSize[1] << " " << gridSize[2] << "\n";
  for (size_t z = 0; z < gridSize[2]; z++) {
    float h = float(z) / float(gridSize[2]);

    for (size_t y = 0; y < gridSize[1]; y++) {
      for (size_t x = 0; x < gridSize[0]; x++) {
        float pix = image(x, y) / 255.0f;
        int val = 1;
        if (h <= pix) {
          val = 2;
        }
        out << val << " ";
      }
      out << "\n";
    }
    out << "\n";
  }
  return ret;
}

class VoxCb : public VoxCallback {
 public:
  void operator()(unsigned x, unsigned y, unsigned z,
                  unsigned long long trigIdx) override {
    Vec3u size = grid.GetSize();
    if (x < size[0] && y < size[1] && z < size[2]) {
      grid(x, y, z) = 1;
    }
  }
  Array3D8u grid;
};

Vec3u linearToGrid(unsigned l, const Vec3u& size) {
  Vec3u idx;
  idx[2] = l / (size[0] * size[1]);
  unsigned r = l - idx[2] * size[0] * size[1];
  idx[1] = r / size[0];
  idx[0] = l % (size[0]);
  return idx;
}

void VoxelizeMesh() {
  voxconf conf;
  conf.unit = Vec3f(2.7f, 2.7f, 2.7f);
  conf.origin = Vec3f(0, 0, 0);
  TrigMesh mesh;
  std::string meshDir = "F:/dolphin/meshes/gasket/";
  std::string meshFile = "gasket6080.obj";
  mesh.LoadObj(meshDir + meshFile);
  BBox box;
  ComputeBBox(mesh.v, box);
  size_t numVerts = mesh.GetNumVerts();
  for (size_t i = 0; i < mesh.v.size(); i += 3) {
    mesh.v[i] -= box.vmin[0];
    mesh.v[i + 1] -= box.vmin[1];
    mesh.v[i + 2] -= box.vmin[2];
  }
  box.vmax -= box.vmin;
  mesh.SaveObj("F:/dump/gasket6080_in.obj");

  box.vmin = Vec3f(0, 0, 0);
  conf.gridSize[0] = unsigned(box.vmax[0] / conf.unit[0]) + 1;
  conf.gridSize[1] = unsigned(box.vmax[1] / conf.unit[0]) + 1;
  conf.gridSize[2] = unsigned(box.vmax[2] / conf.unit[0]) + 1;
  VoxCb cb;
  cb.grid.Allocate(conf.gridSize, 0);
  mesh.ComputeTrigNormals();
  cpu_voxelize_mesh(conf, &mesh, cb);
  std::map<unsigned, unsigned> vertMap;
  Vec3u vertSize = cb.grid.GetSize();
  vertSize += Vec3u(1, 1, 1);
  Vec3u voxSize = cb.grid.GetSize();
  const std::vector<uint8_t>& v = cb.grid.GetData();
  const size_t NUM_HEX_VERTS = 8;
  unsigned HEX_VERTS[8][3] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 1},
                              {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}};
  std::vector<std::array<unsigned, NUM_HEX_VERTS> > elts;
  std::vector<unsigned> verts;
  Vec3f origin(0, 0, 0);
  SaveVolAsObjMesh("F:/dump/vox_6080.obj", cb.grid, (float*)(&conf.unit[0]),
                   (float*)(&origin), 1);
  std::vector<Vec3f> X;
  VoxGridToHexMesh(cb.grid, 1e-3f * conf.unit, box.vmin, X, elts);
  std::string outFile("F:/dump/vox6080.txt");
  SaveHexTxt(outFile, X, elts);
}

void MakeGyroid() {
  Array3D8u grid(100, 100, 100);
  grid.Fill(1);
  Vec3u size = grid.GetSize();
  float twoPi = 6.2831853f;
  float thresh = -0.3f;
  unsigned fillCount = 0;
  for (unsigned z = 0; z < size[2]; z++) {
    float fz = float(z) / size[2] * twoPi;
    for (unsigned y = 0; y < size[1]; y++) {
      float fy = float(y) / size[1] * twoPi;
      for (unsigned x = 0; x < size[0]; x++) {
        float fx = float(x) / size[0] * twoPi;
        float distVal = std::cos(fx) * std::sin(fy) +
                        std::cos(fy) * std::sin(fz) +
                        std::cos(fz) * std::sin(fx);
        if (distVal < thresh) {
          fillCount++;
          grid(x, y, z) = 2;
        }
      }
    }
  }
  std::cout << float(fillCount) / size[0] / size[1] / size[2] << "\n";

  std::ofstream out("F:/dump/gyroid.txt");
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        out << int(grid(x, y, z)) << " ";
      }
      out << "\n";
    }
    out << "\n";
  }
}

struct Segment {
  virtual Vec2f eval(float t) const = 0;
  virtual float len() const = 0;
};

/// always picks the center with shorter arc.
struct Arc : public Segment{
  Vec2f x0;
  Vec2f x1;
  float radius = 1;
  Vec2f center;
  float theta0 = 0;
  float theta1 = 0;
  Vec2f eval(float t) const {
    float theta = theta0 + t / radius;
    Vec2f local(std::cos(theta),std::sin(theta));
    return center + radius * local;
  }
  float len() const { return radius * (theta1 - theta0); }
  Arc(Vec2f a, Vec2f b, float r) : x0(a), x1(b), radius(r) {
    Vec2f mid = 0.5f * (a + b);
    Vec2f ab = b - a;
    Vec2f midDir(-ab[1],ab[0]);
    float eps = 1e-20;
    float ab_len = ab.norm();
    if (ab_len < eps) {
      ab_len = eps;
    }
    midDir = (1.0f / ab_len) * midDir;
    float diff = r * r - 0.25f * ab_len * ab_len;
    float h = 0;
    if (diff >= 0) {
      h = std::sqrt(diff);
    }
    center = mid + h * midDir;
    Vec2f ao = a - center;
    theta0 = std::atan2(ao[1], ao[0]);
    Vec2f bo = b - center;
    theta1 = std::atan2(bo[1], bo[0]);
    if (theta1 < theta0) {
      theta1 += 2 * 3.14159265f;
    }
  }
};

struct Line : public Segment {
  Vec2f x0;
  Vec2f x1;
  Line(Vec2f a, Vec2f b) : x0(a), x1(b) {}
  Vec2f eval(float t) const {
    Vec2f dir = (x1 - x0);
    float eps = 1e-20;
    float n = dir.norm();
    if (n < eps) {
      n = eps;
    }
    dir = (1.0f / n) * dir;
    return x0 + t * dir;
  }
  float len() const override { return (x1 - x0).norm(); }
};

struct Curve {
  std::vector<std::unique_ptr<Segment>> segs;
  void Add(std::unique_ptr<Segment> s) { segs.push_back(std::move(s)); }
  void AddLine(Vec2f a, Vec2f b) {
    std::unique_ptr<Segment> s = std::make_unique<Line>(a, b);
    Add(std::move(s));
  }
  void AddArc(Vec2f a, Vec2f b, float r) {
    std::unique_ptr<Segment> s = std::make_unique<Arc>(a, b, r);
    Add(std::move(s));
  }
  std::vector<Vec2f> SamplePoints(float ds) {
    std::vector<Vec2f> points;
    for (size_t i = 0;i<segs.size();i++) {
      float l = segs[i]->len();
      float numSamples = l / ds + 1;
      for (size_t j = 0; j < numSamples; j++) {
        float t = (float(j + 0.5f) / numSamples) * l;
        points.push_back(segs[i]->eval(t));
      }
    }
    return points;
  }
};

struct Placement {
  float rot=0;
  Vec2f trans;
};

Vec2f ApplyRot(Vec2f v, float r) {
  float c = std::cos(r);
  float s = std::sin(r);
  Vec2f out;
  out[0] = c * v[0] - s * v[1];
  out[1] = s * v[0] + c * v[1];
  return out;
}

std::vector<Placement> PlaceTracks(Vec2f hole0, Vec2f hole1,
                                   const std::vector<Vec2f>& curve) {
  std::vector<Placement> p;
  unsigned i = 0;
  if (curve.size() < 2) {
    return p;
  }
  Vec2f p0 = curve[0];
  float eps = 0.05f;
  float trackWidth = (hole1 - hole0).norm();
  for (unsigned i = 1; i < curve.size(); i++) {
    Vec2f c1 = curve[i];
    float dist = (c1 - p0).norm();
    Vec2f p1;
    if (dist > trackWidth) {
      // approximate interpolation
      float prevDist = (curve[i - 1] - p0).norm();
      // weight for curve[i-1].
      float alpha = 1;
      if (prevDist < trackWidth) {
        alpha = (dist - trackWidth) / (dist - prevDist);
      }
      p1 = alpha * curve[i - 1] + (1 - alpha) * curve[i];      
      Vec2f p01 = p1 - p0;
      float r = std::atan2(p01[1], p01[0]);      
      Vec2f hole0Rot = ApplyRot(hole0, r);
      Vec2f tran = p0 - hole0Rot;
      p.push_back({r, tran});
      p0 = p1;

    }
  }
  return p;
}

void MakeCurve() { 
  Curve c;

  c.AddArc({-0.024, 19.119}, {-18.88, 4.494}, 19);
  c.AddArc({-18.88, 4.494}, {-0.16, -19.4}, 19);
  c.AddLine({-0.16, -19.4} ,{45, -32.66});
  c.AddLine({45, -32.66}, {133, -31.16});
  c.AddArc({133, -31.16}, {146.68, -26.222}, 19.4);
  c.AddLine({146.68, -26.222}, {178.22, 1.7});
  c.AddArc({178.22, 1.7}, {183.3, 7.7781}, 18.5);
  c.AddArc({183.3, 7.7781}, {166, 33.3}, 18.5);
  c.AddLine({166, 33.3} , {68.042, 33.8});
  c.AddLine({68.042, 33.8}, {-0.024, 19.519});
  auto points = c.SamplePoints(0.1f);
  std::ofstream out("F:/dump/place.txt");
  Vec2f hole0(-3.4157, 0.53905);
  Vec2f hole1(5.0823, 0.53937);
  std::vector<Placement> placement = PlaceTracks(hole0, hole1, points);
  for (size_t i = 0; i < placement.size(); i++) {
    const auto& p = placement[i];
    out << p.trans[0] << " " << p.trans[1] << " " << p.rot << "\n";
  }
}
