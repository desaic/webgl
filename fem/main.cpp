
#include <filesystem>
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
    std::vector<Vec3f> force = em.GetForce();
    Add(force, em.fe);
    Vec3f maxAbsForce = MaxAbs(force);
    std::cout << "max abs(force) " << maxAbsForce[0] << " " << maxAbsForce[1]
              << " " << maxAbsForce[2] << "\n";
    float h = 0.0001f;
    const float maxh = 0.0001f;

    std::vector<double> dx, b;
    for (size_t i = 0; i < em.fixedDOF.size();i++) {
      if (em.fixedDOF[i]) {
      
      } else {
      
      }
    }
    CG(state.K, dx, b, 400);

    h = maxh / maxAbsForce.norm();
    h = std::min(maxh, h);
    std::cout << "h " << h << "\n";
    std::vector<Vec3f> dx = h * force;
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
      em.fe[i] = force;
    }
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

class FemApp {
 public:
  FemApp(UILib* ui) : _ui(ui) {
    floor = std::make_shared<TrigMesh>();
    *floor = MakePlane(Vec3f(-200, -0.1, -200), Vec3f(200, -0.1, 200),
                      Vec3f(0, 1, 0));
    for (size_t i = 0; i < floor->uv.size(); i++) {
      floor->uv[i] *= 5;
    }
    Array2D8u checker;
    MakeCheckerPatternRGBA(checker);
    _floorMeshId = _ui->AddMesh(floor);
    _ui->SetMeshTex(_floorMeshId, checker, 4);
    _hexInputId = _ui->AddWidget(std::make_shared<InputText>("mesh file", "./hex.txt"));
    _ui->AddButton("Load Hex mesh", [&] {
      std::string file = GetMeshFileName();
      LoadElementMesh(file);
    });
    _wireframeId = _ui->AddCheckBox("showWireFrame", true);
  }

  std::string GetMeshFileName() const {
    std::shared_ptr<InputText> input =
        std::dynamic_pointer_cast<InputText>(_ui->GetWidget(_hexInputId));
    std::string file = input->GetString();
    return file;
  }

  void InitExternalForce() { 
    _em.fe= std::vector<Vec3f>(_em.X.size());
    _em.fixedDOF=std::vector<bool>(_em.X.size() * 3, false);
    PullRight(Vec3f(0, -0.1, 0), 0.001, _em);
    FixLeft(0.001, _em);
  }

  void LoadElementMesh(const std::string& file) {
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

  void Refresh() {
    bool wire = _ui->GetCheckBoxVal(_wireframeId);
    _renderMesh.ShowWireFrame(wire);
    _sim.StepCG(_em, _simState);
    _renderMesh.UpdatePosition();
    _ui->SetMeshNeedsUpdate(_meshId);
  }

 private:
  UILib* _ui = nullptr;
  ElementMesh _em;
  FETrigMesh _renderMesh;
  int _hexInputId = -1;
  int _meshId = -1;
  int _wireframeId = -1;
  float _drawingScale = MToMM;
  std::shared_ptr<TrigMesh> floor;
  int _floorMeshId = -1;
  Simulator _sim;
  SimState _simState;
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

int main(int, char**) {
  //TestStiffnessFiniteDiff();
  TestSparse();
  UILib ui;
  FemApp app(&ui);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 800);
  ui.SetKeyboardCb(HandleKeys);
  int statusLabel = ui.AddLabel("status");
  ui.Run();

  const unsigned PollFreqMs = 20;
  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  return 0;
}
