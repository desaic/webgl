
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "ImageIO.h"
#include "Timer.h"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "lodepng.h"
#include "ElementMesh.h"
#include "ElementMeshUtil.h"

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
      ComputeWireframeMesh(*_em, *_mesh, _edges, _drawingScale);      
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
      UpdateWireframePosition(*_em, *_mesh, _edges, _drawingScale);
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
};

const float MToMM = 1000;

void MakeCheckerPatternRGBA(Array2D8u& out) {
  unsigned numCells = 4;
  unsigned cellSize = 4;
  unsigned width = numCells * cellSize;
  out.Allocate(width*4, width);
  out.Fill(200);
  for (size_t i = 0; i < width; i++) {
    for (size_t j = 0; j < width; j++) {
      if ( (i/cellSize + j/cellSize) % 2 == 0) {
        continue;
      }
      for (unsigned chan = 0; chan < 3; chan++) {
        out(4 * i + chan, j) = 50;
      }
    }
  }
}

void Add(std::vector<Vec3f>& dst, const std::vector<Vec3f>& src) {
  for (size_t i = 0; i < dst.size(); i++) {
    dst[i] += src[i];
  }
}

std::vector<Vec3f> operator*(float c, const std::vector<Vec3f>& v) {
  std::vector<Vec3f> out(v.size());
  for (size_t i = 0; i < out.size(); i++) {
    out[i] = c * v[i];
  }
  return out;
}

void Fix(std::vector<Vec3f>& dx, const std::vector<bool> fixedDOF) {
  for (size_t i = 0; i < dx.size(); i++) {
    for (size_t j = 0; j < 3; j++) {
      if (fixedDOF[3*i+j]) {
        dx[i][j] = 0;
      }
    }
  }
}

Vec3f MaxAbs(const std::vector<Vec3f>& v) { 
  if (v.size() == 0) {
    return Vec3f(0, 0, 0);
  }

  Vec3f ma = v[0]; 

  for (size_t i = 1; i < v.size(); i++) {
    for (size_t j = 0; j < 3; j++) {
      ma[j] = std::max(ma[j], std::abs(v[i][j]));
    }
  }
  return ma;
}

class Simulator {
 public:
  void Step(ElementMesh& em) {
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
    sim.Step(_em);
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
  Simulator sim;
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

void TestFEM() {
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
    std::cout          << "\n";
  }

  Matrix3f F(0.8, -0.16, 0, 0.2, 0.99, 0.1, 0.2, 0.3, 1);
  Matrix3f Finv = F.inverse();
  Matrix3f prod = Finv * F;
  PrintMat3(prod,std::cout);
  std::cout << "\n";
  std::cout << F.determinant() << " " << Finv.determinant() << "\n";
}

int main(int, char**) {
  TestFEM(); 
  UILib ui;
  FemApp app(&ui);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 800);
  int buttonId = ui.AddButton("GLInfo", {});
  int gl_info_id = ui.AddLabel(" ");
  std::function<void()> showGLInfoFunc =
      std::bind(&ShowGLInfo, std::ref(ui), gl_info_id);
  ui.SetButtonCallback(buttonId, showGLInfoFunc);
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
