
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "ArrayUtil.h"
#include "BBox.h"
#include "ImageIO.h"
#include "ImageUtils.h"
#include "Timer.h"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "lodepng.h"
#include "ElementMesh.h"
#include "ElementMeshUtil.h"
#include "CSparseMat.h"
#include "cpu_voxelizer.h"

#include "cpu_voxelizer.h"
#include "VoxCallback.h"
#include "MeshUtil.h"

void ShowGLInfo(UILib& ui, int label_id) {
  std::string info = ui.GetGLInfo();
  ui.SetLabelText(label_id, info);
}

void HandleKeys(KeyboardInput input) {
  std::vector<std::string> keys = input.splitKeys();
  for (const auto& key : keys) {
    bool sliderChange = false;
    int sliderVal = 0;
  }
}

void LogToUI(const std::string& str, UILib& ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}

//triangle mesh for rendering an fe mesh. 
class FETrigMesh{
 public:
  FETrigMesh() {}
  void UpdateMesh(ElementMesh* em, float drawingScale) {
    if (!em) {
      return;
    }
    _drawingScale = drawingScale;
    _em = em;
    _mesh = std::make_shared<TrigMesh>();
    if (_showWireFrame) {
      ComputeWireframeMesh(*_em, *_mesh, _edges, _drawingScale, _beamThickness);      
    } else {
      ComputeSurfaceMesh(*_em, *_mesh, _meshToEMVert, _drawingScale);
    }
  }

  //copy "x" from _em to _mesh
  void UpdatePosition() {
    if (!(_em && _mesh)) {
      return;
    }
    if (_showWireFrame) {
      UpdateWireframePosition(*_em, *_mesh, _edges, _drawingScale,
                              _beamThickness);
      return;
    }
    for (size_t i = 0; i < _meshToEMVert.size(); i++) {
      unsigned emVert = _meshToEMVert[i];
      _mesh->v[3 * i] = _em->x[emVert][0];
      _mesh->v[3 * i + 1] = _em->x[emVert][1];
      _mesh->v[3 * i + 2] = _em->x[emVert][2];
    }
  }

  void ShowWireFrame(bool b) {
    if (b != _showWireFrame) {
      _showWireFrame = b;
      UpdateMesh(_em, _drawingScale);
    }
  }

  ElementMesh* _em = nullptr;
  std::shared_ptr<TrigMesh> _mesh;
  std::vector<uint32_t> _meshToEMVert;
  std::vector<Edge> _edges;
  bool _showWireFrame = true;
  float _drawingScale = 1;
  float _beamThickness = 0.5f;
};

const float MToMM = 1000;

//things helpful to be stored across time steps
struct SimState {
  CSparseMat K;
};

class Simulator {
 public:
  void StepGrad(ElementMesh& em) {
    if (em.X.empty()) {
      return;
    }
    std::vector<Vec3f> force = em.GetForce();

    Add(force, em.fe);
    Vec3f maxAbsForce = MaxAbs(force);
    std::cout << "max abs(force) " << maxAbsForce[0] << " " << maxAbsForce[1]
              << " " << maxAbsForce[2] << "\n";
    float h = 0.0001f;
    const float maxh = 0.0001f;
    h = maxh / maxAbsForce.norm();
    h = std::min(maxh, h);
    std::cout << "h " << h << "\n";
    std::vector<Vec3f> dx = h * force;
    Fix(dx, em.fixedDOF);
    Add(em.x, dx);
  }
  
  void StepCG(ElementMesh& em, SimState& state) {
    if (em.X.empty()) {
      return;
    }
    //elastic and external force combined
    std::vector<Vec3f> force = em.GetForce();
    Add(force, em.fe);
    size_t numDOF = em.x.size() * 3;
    std::vector<double> dx(numDOF, 0), b(numDOF);
    float boundaryTol = 1e-4f;
    std::vector<bool> fixed = em.fixedDOF;
    for (size_t i = 0; i < em.x.size(); i++) {

      for (unsigned j = 0; j < 3; j++) {
        unsigned l = 3 * i + j;
        //within or under lower bound and the total force is not pulling it away
        if (em.x[i][j] < em.lb[i][j] + boundaryTol ){//&& force[i][j]<0) {
          fixed[l] = true;
        }
      }
    }

    for (size_t i = 0; i < fixed.size(); i++) {
      if (fixed[i]) {
        b[i] = 0;
      } else {
        b[i] = force[i / 3][i % 3];
      }
    }
    float identityScale = 1000;
    em.ComputeStiffness(state.K);
    FixDOF(fixed, state.K, identityScale);
    CG(state.K, dx, b, 800);
    const float h0 = 1.0f;
    const unsigned MAX_LINE_SEARCH = 10;
    double E0 = em.GetPotentialEnergy();
    
    double newE = E0;
    auto oldx = em.x;
    float h = h0;
    bool updated = false;
    for (unsigned li = 0;li<MAX_LINE_SEARCH;li++){
      em.x = oldx;
      AddTimes(em.x, dx, h);
      newE = em.GetPotentialEnergy();
      std::cout << newE << "\n";
      if (newE < E0) {
        updated = true;
        break;
      }
      h /= 2;
    }
    if (!updated) {
      em.x = oldx;
    }
    //std::vector<Vec3f> dx = h * force;
  }
  
  void InitCG(ElementMesh& em, SimState& state) { 
    em.InitStiffnessPattern();
    em.CopyStiffnessPattern(state.K);
  }
};

void PullRight(const Vec3f& force, float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < dist) {
      em.fe[i] += force;
    }
  }
}

void PullLeft(const Vec3f& force, float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (em.X[i][0] - box.vmin[0] < dist) {
      em.fe[i] += force;
    }
  }
}

void AddGravity(const Vec3f& G, ElementMesh& em) {
  for (size_t i = 0; i < em.X.size(); i++) {
    em.fe[i] += G;    
  }
}

void FixLeft(float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (em.X[i][0] - box.vmin[0] < dist) {
      em.fixedDOF[3 * i] = true;
      em.fixedDOF[3 * i + 1] = true;
      em.fixedDOF[3 * i + 2] = true;
    }
  }
}

void FixRight(float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < dist) {
      em.fixedDOF[3 * i] = true;
      em.fixedDOF[3 * i + 1] = true;
      em.fixedDOF[3 * i + 2] = true;
    }
  }
}

void FixMiddle(float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  float midx = 0.5f*(box.vmax[0] + box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if ( std::abs(em.X[i][0] - midx) < dist) {
      em.fixedDOF[3 * i] = true;
      em.fixedDOF[3 * i + 1] = true;
      em.fixedDOF[3 * i + 2] = true;
    }
  }
}

void PullMiddle(const Vec3f& force, float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  float midx = 0.5f * (box.vmax[0] + box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (std::abs(em.X[i][0] - midx) < dist) {
      em.fe[i] += force;
    }
  }
}
void FixFloorLeft(float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (em.X[i][0] - box.vmin[0] < dist) {
      em.fixedDOF[3 * i + 1] = true;      
    }
  }
}

void SetFloorForRightSide(float y1, float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < dist) {
      em.lb[i][1] = y1;
    }
  }
}

Vec3f Trilinear(const Element& hex,
               const std::vector<Vec3f>& X, const std::vector<Vec3f>& x, Vec3f & v) {
  
  Vec3f out(0,0,0);
  Vec3f X0 = X[hex[0]];
  float h = X[hex[1]][2] - X0[2];
  float w[3];
  for (unsigned i = 0; i < 3; i++) {
    w[i] = (v[i] - X0[i])/h;
    w[i] = std::clamp(w[i], 0.0f, 1.0f);
  }
  Vec3f z00 =
      (1 - w[2]) * (x[hex[0]] - X[hex[0]]) + w[2] * (x[hex[1]] - X[hex[1]]);
  Vec3f z01 =
      (1 - w[2]) * (x[hex[2]] - X[hex[2]]) + w[2] * (x[hex[3]] - X[hex[3]]);
  Vec3f z10 =
      (1 - w[2]) * (x[hex[4]] - X[hex[4]]) + w[2] * (x[hex[5]] - X[hex[5]]);
  Vec3f z11 =
      (1 - w[2]) * (x[hex[6]] - X[hex[6]]) + w[2] * (x[hex[7]] - X[hex[7]]);
  Vec3f y0 = (1 - w[1]) * z00 + w[1] * z01;
  Vec3f y1 = (1 - w[1]) * z10 + w[1] * z11;
  Vec3f u = (1 - w[0]) * y0 + w[0] * y1;
  out = v + u;
  return out;
}

class FemApp {
 public:
  FemApp(UILib* ui) : _ui(ui) {
    //EmbedMesh();
    floor = std::make_shared<TrigMesh>();
    *floor = MakePlane(Vec3f(-0, -0.1, -0), Vec3f(500, -0.1, 500),
                      Vec3f(0, 1, 0));
    for (size_t i = 0; i < floor->uv.size(); i++) {
      floor->uv[i] *= 5;
    }
    GLRender* gl = ui->GetGLRender();
    gl->SetDefaultCameraView(Vec3f(250,100,500),Vec3f(200,0,250));
    GLLightArray* lights = gl->GetLights();
    lights->SetLightPos(0, 250, 100, 250);
    lights->SetLightPos(1, 500, 100, 250);
    lights->SetLightColor(0, 0.8, 0.7, 0.6);
    lights->SetLightColor(1, 0.8, 0.8, 0.8);
    lights->SetLightPos(2, 0, 100, 100);
    lights->SetLightColor(2, 0.8, 0.7, 0.6);
    Array2D8u checker;
    MakeCheckerPatternRGBA(checker);
    _floorMeshId = _ui->AddMesh(floor);
    _ui->SetMeshTex(_floorMeshId, checker, 4);
    std::string voxFile = "F:/dump/vox6080.txt";
    _hexInputId = _ui->AddWidget(std::make_shared<InputText>("mesh file", voxFile));
    _ui->AddButton("Load Hex mesh", [&] {
      std::string file = GetInputString(_hexInputId);
      LoadElementMesh(file);      
    });
    _ui->AddButton("Save x", [&] { _save_x = true; });
    _ui->AddButton("Run sim", [&] { _runSim = true; });
    _ui->AddButton("Stop sim", [&] { _runSim = false; });
    _xInputId =
        _ui->AddWidget(std::make_shared<InputText>("x file", "F:/dump/x_in.txt"));
    _ui->AddButton("Load x", [&] {
      std::string file = GetInputString(_xInputId);
      LoadX(_em, file);
    });
    _wireframeId = _ui->AddCheckBox("showWireFrame", true);
  }

  std::string GetInputString(int widgetId) const {
    std::shared_ptr<InputText> input = std::dynamic_pointer_cast<InputText>(
        _ui->GetWidget(widgetId));
    std::string file = input->GetString();
    return file;
  }

  void InitExternalForce() { 
    _em.fe= std::vector<Vec3f>(_em.X.size());
    _em.fixedDOF=std::vector<bool>(_em.X.size() * 3, false);
    _em.lb.resize(_em.X.size(), Vec3f(-1000, -1000, -1000));
    //PullRight(Vec3f(0,-0.001,  0), 0.01, _em);    
    //PullLeft(Vec3f(0,-0.001, 0), 0.01, _em);
    //FixLeft(0.03, _em);
    //FixRight(0.03, _em);
    //PullMiddle(Vec3f(0, 0.002, 0), 0.03, _em);
    //FixFloorLeft(0.4, _em);
    //_em.lb.resize(_em.X.size(), Vec3f(-1000, -1, -1000));
    //SetFloorForRightSide(0.0055, 0.4, _em);
    FixMiddle(0.03, _em);
    //AddGravity(Vec3f(0, -0.0005, 0),_em);
  }

  void LoadElementMesh(const std::string& file) {
    std::lock_guard<std::mutex> lock(_refresh_mutex);
    _em.LoadTxt(file);
    UpdateRenderMesh();
    InitExternalForce(); 
    _sim.InitCG(_em, _simState);
  }

  void UpdateRenderMesh() {
    _renderMesh.UpdateMesh(&_em, _drawingScale);
    if (_meshId < 0) {
      _meshId = _ui->AddMesh(_renderMesh._mesh);
    }
    _ui->SetMeshNeedsUpdate(_meshId);
    
  }

  int LoadX(ElementMesh& em, const std::string& inFile) {
    std::ifstream in(inFile);
    size_t xSize = 0;
    in >> xSize;
    if (xSize != em.X.size()) {
      std::cout << "mismatch x size vs mesh size.\n";
      return -1;
    }
    em.x.resize(xSize);
    for (size_t i = 0; i < em.x.size(); i++) {
      in >> em.x[i][0] >> em.x[i][1] >> em.x[i][2];
    }
    return 0;
  }
  
  void SaveX(const ElementMesh& em, const std::string& outFile) {
    std::ofstream out(outFile);
    out << em.x.size() << "\n";
    for (size_t i = 0; i < em.x.size(); i++) {
      out << em.x[i][0] << " " << em.x[i][1] << " " << em.x[i][2] << "\n";
    }
  }
  
  void Refresh() {
    std::lock_guard<std::mutex> lock(_refresh_mutex);
    bool wire = _ui->GetCheckBoxVal(_wireframeId);
    _renderMesh.ShowWireFrame(wire);
    if (_runSim) {
      _sim.StepCG(_em, _simState);
      _renderMesh.UpdatePosition();
      _ui->SetMeshNeedsUpdate(_meshId);
    }
    if (_save_x) {
      SaveX(_em,"F:/dump/x_out.txt");
      _save_x = false;
    }
  }
  
  void EmbedMesh() { 
    ElementMesh em;
    em.LoadTxt("F:/dump/vox6080.txt");
    LoadX(em, "F:/dump/x_6080_11.txt");
    TrigMesh mesh;
    mesh.LoadObj("F:/dolphin/meshes/gasket/gasket6080_in.obj");
    const float meterTomm=1000.0f;
    for (auto& X : em.X) {
      X = meterTomm * X;
    }
    for (auto& x : em.x) {
      x = meterTomm * x;
    }
    Vec3u gridSize;
    float h = 2.7f;
    Array3D<unsigned> grid;
    BBox box;
    ComputeBBox(em.X, box);
    for (unsigned i = 0; i < 3; i++) {
      gridSize[i] = unsigned((box.vmax[i] - box.vmin[i]) / h - 0.5f) + 1;
    }
    grid.Allocate(gridSize[0], gridSize[1], gridSize[2], ~0);
    bool hasMax[3] = {0,0,0};
    for (size_t i = 0; i < em.e.size(); i++) {
      Vec3f center = em.X[(*em.e[i])[0]] + 0.5f * Vec3f(h, h, h);
      unsigned ix = unsigned(center[0] / h);
      unsigned iy = unsigned(center[1] / h);
      unsigned iz = unsigned(center[2] / h);
      grid(ix, iy, iz) = i;
      if (ix == gridSize[0] - 1) {
        hasMax[0] = true;
      }
      if (iy == gridSize[1] - 1) {
        hasMax[1] = true;
      }
      if (iz == gridSize[2] - 1) {
        hasMax[2] = true;
      }
    }
    //for each surface mesh vertex, assign it to an element.
    std::vector<unsigned> vertEle(mesh.GetNumVerts(), 0);
    std::vector<float> newVerts(mesh.v.size(), 0);
    Vec3f delta[6] = {{0, 0, -0.1f}, {0, 0, 0.1f},  {0, -0.1f, 0},
                      {0, 0.1f, 0},  {-0.1f, 0, 0}, {0.1f, 0, 0}};
    for (size_t i = 0; i < mesh.GetNumVerts(); i++) {
      //find closest cuboid center
      Vec3u gridIdx;
      Vec3f v0 = *(Vec3f*)(&mesh.v[3*i]);
      int intIndex[3];
      for (unsigned j = 0; j < 3; j++) {
        intIndex[j] = int(v0[j] / h);
        intIndex[j] = std::clamp(intIndex[j], 0 , int(gridSize[j]) - 1);
        gridIdx[j] = unsigned(intIndex[j]);
      }
      std::vector<Vec3u> candidates(7);
      unsigned eidx=em.e.size();
      candidates[0] = gridIdx;
      for (unsigned c = 0; c < 6; c++) {
        Vec3f v1 = v0 + delta[c];
        for (unsigned j = 0; j < 3; j++) {
          intIndex[j] = int(v1[j] / h);
          intIndex[j] = std::clamp(intIndex[j], 0, int(gridSize[j]) - 1);
          gridIdx[j] = unsigned(intIndex[j]);
        }
        candidates[c + 1] = gridIdx;
      }
      for (size_t c = 0; c < candidates.size(); c++) {
        gridIdx = candidates[c];
        unsigned ei = grid(gridIdx[0], gridIdx[1], gridIdx[2]);
        if (ei < em.e.size()) {
          eidx = ei;
          break;
        }
      }
      if (eidx >= em.e.size()) {
        std::cout << i << " ";
        std::cout << v0[0] << " " << v0[1] << " " << v0[2] << "\n";
      }
      Vec3f moved = Trilinear(*em.e[eidx], em.X, em.x, v0);
      newVerts[3 * i] = moved[0];
      newVerts[3 * i + 1] = moved[1];
      newVerts[3 * i + 2] = moved[2];
    }
    mesh.v = newVerts;
    mesh.SaveObj("F:/dump/deformed.obj");
  }

 private:
  UILib* _ui = nullptr;
  ElementMesh _em;
  FETrigMesh _renderMesh;
  int _hexInputId = -1;
  int _xInputId = -1;
  int _meshId = -1;
  int _wireframeId = -1;
  float _drawingScale = MToMM;
  std::shared_ptr<TrigMesh> floor;
  int _floorMeshId = -1;
  Simulator _sim;
  SimState _simState;
  std::mutex _refresh_mutex;
  bool _save_x = false;
  bool _runSim = false;
};

std::vector<Vec3f> CentralDiffForce(ElementMesh & em, float h) {
  std::vector<Vec3f> f(em.x.size()); 
  for (size_t i = 0; i < em.x.size(); i++) {
    for (size_t j = 0; j < 3; j++) {
      em.x[i][j] -= h;
      float negEne = em.GetElasticEnergy();
      
      em.x[i][j] += 2*h;
      float posEne = em.GetElasticEnergy();

      float force = -(posEne - negEne) / (2*h);
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
  //em.SaveTxt("F:/dump/hex_m.txt");
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
    std::cout << force[i][0] << " " << force[i][1] << " " << force[i][2]<<", ";
    Vec3f diff = refForce[i] - force[i];
    if (diff.norm() > 0.1) {
      std::cout << refForce[i][0] << " " << refForce[i][1] << " "
                << refForce[i][2] << " ";
    }
    if (em.fe[i][1] < 0) {
      std::cout << " @";
    }
    std::cout<< "\n";
  }

  Matrix3f F(0.8, -0.16, 0, 0.2, 0.99, 0.1, 0.2, 0.3, 1);
  Matrix3f Finv = F.inverse();
  Matrix3f prod = Finv * F;
  PrintMat3(prod,std::cout);
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
  std::vector<double> x, b,prod;
  
  unsigned cols = K.Cols();
  x.resize(cols, 0);
  b.resize(cols, 0);
  prod.resize(cols, 0);
  for (size_t col = 0; col < cols; col++) {
    b[col] = 1+ col%11;
  }
  int ret = CG(K, x, b, 1000);
  
  K.MultSimple(x.data(), prod.data());
  for (size_t i = 0; i < prod.size(); i++) {
    std::cout << "(" << prod[i] << " " << b[i] << ") ";
  }

}

void TestModes(ElementMesh & em) { 
  Array2Df dx = dlmread("F:/dump/modes.txt");
  Vec2u size = dx.GetSize();
  unsigned mode = 11;
  for (unsigned i = 0; i < size[0]; i+=3) {
    unsigned vi = i / 3;
    em.x[vi] = em.X[vi] + 0.01f * Vec3f(dx(i,mode), dx(i+1,mode), dx(i+2,mode));
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
  FixDOF(em.fixedDOF,K, 1000);
  //K.ToDense(dense.DataPtr());
  //dlmwrite("F:/dump/Kfix.txt", dense);
  //TestModes(em);
  TestSparseCG(K);
}


#include <filesystem>
namespace fs = std::filesystem;
void CenterMeshes() {
  std::string dir = "F:/dolphin/meshes/20240304-V8-01/mmp/";
  int starti = 4, endi = 60;
  for (int i = starti; i <= endi; i++) {
    std::string istr = std::to_string(i);
    std::string modelDir = dir + istr + "/models/";
    if (!std::filesystem::exists(modelDir)) {
      continue;
    }
    std::string cageFile = modelDir + "cage.obj";
    if (!std::filesystem::exists(cageFile)) {
      continue;
    }

    std::string originalStl = modelDir + "0.STL";
    fs::path directory_path(modelDir);

    // Create an iterator pointing to the directory
    fs::directory_iterator it(directory_path);
    std::vector<fs::path> stls;
    // Iterate through the directory entries
    for (const auto& entry : it) {
      // Check if it's a regular file
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".stl") {
          stls.push_back(entry.path());
        }
      }
    }
    for (const auto& path : stls) {
     TrigMesh mesh;
      mesh.LoadStl(path.string());
     BBox box;
      ComputeBBox(mesh.v,box);
     Vec3f center = 0.5f * (box.vmax + box.vmin);
      for (size_t i = 0; i < mesh.v.size(); i += 3) {
        mesh.v[i] -= center[0];
        mesh.v[i+1] -= center[1];
        mesh.v[i+2] -= center[2];
      }
      mesh.SaveStlTxt(path.string());
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
  Vec2u imageSize= image.GetSize();
  float res = 0.064f;
  Vec3u gridSize(imageSize[0], imageSize[1], 30);
  std::ofstream out(outFile);
  out << "voxelSize\n0.064 0.064 0.064\nspacing\n0.2\ndirection\n0 0 1\ngrid\n";
  out << gridSize[0]<<" "<<gridSize[1]<<" "<<gridSize[2]<<"\n";
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

unsigned linearIdx(const Vec3u& idx, const Vec3u& size) {
  unsigned l;
  l = idx[0] + idx[1] * size[0] + idx[2] * size[0] * size[1];
  return l;
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
  Vec3f origin (0,0,0);
  SaveVolAsObjMesh("F:/dump/vox_6080.obj", cb.grid, (float*)(&conf.unit[0]),
                   (float*)(&origin), 1);
  for (size_t i = 0; i < v.size(); i++) {
    if (!v[i]) {
      continue;
    }
    Vec3u voxIdx = linearToGrid(i, voxSize);
    std::array<unsigned, NUM_HEX_VERTS> ele;
    for (size_t j = 0; j < NUM_HEX_VERTS; j++) {
      Vec3u vi = voxIdx;
      vi[0] += HEX_VERTS[j][0];
      vi[1] += HEX_VERTS[j][1];
      vi[2] += HEX_VERTS[j][2];
      unsigned vl = linearIdx(vi, vertSize);
      auto it = vertMap.find(vl);
      unsigned vIdx;
      if (it == vertMap.end()) {
        vIdx = vertMap.size();
        vertMap[vl] = vIdx;
        verts.push_back(vl);
      } else {
        vIdx = vertMap[vl];
      }
      ele[j] = vIdx;
    }
    elts.push_back(ele);
  }
  std::ofstream out("F:/dump/vox6080.txt");
  out << "verts " << vertMap.size() << "\n";
  out << "elts " << elts.size() << "\n";
  for (auto v : verts) {
    unsigned l = v;
    Vec3u vertIdx = linearToGrid(l, vertSize);
    Vec3f coord;
    coord[0] = float(vertIdx[0]) * conf.unit[0];
    coord[1] = float(vertIdx[1]) * conf.unit[1];
    coord[2] = float(vertIdx[2]) * conf.unit[2];
    coord += box.vmin;
    coord *= 1e-3f;
    out << coord[0] << " " << coord[1] << " " << coord[2] << "\n";
  }
  for (size_t i = 0; i < elts.size(); i++) {
    out << "8 ";
    for (size_t j = 0; j < NUM_HEX_VERTS; j++) {
      out << elts[i][j] << " ";
    }
    out << "\n";
  }
}

int main(int argc, char** argv) {
  // PngToGrid(argc, argv);
  // CenterMeshes();
  UILib ui;
  FemApp app(&ui);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 800);
  ui.SetKeyboardCb(HandleKeys);
  int statusLabel = ui.AddLabel("status");
  ui.Run();
  app.EmbedMesh();
  const unsigned PollFreqMs = 20;
  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  return 0;
}
