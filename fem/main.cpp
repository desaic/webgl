
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
    float eleSize = em.X[1][2] - em.X[0][2];
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
    CG(state.K, dx, b, 10000);
    float h0 = 1.0f;
    const unsigned MAX_LINE_SEARCH = 10;
    double E0 = em.GetPotentialEnergy();
    
    double newE = E0;
    auto oldx = em.x;
    float h;
    bool updated = false;
    float maxdx = Linf(dx);
    h = std::min(h0, 0.5f*eleSize / maxdx);
    std::cout << "h " << h << "\n";
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
    FixLeft(0.03, _em);
    FixRight(0.01, _em);
    MoveRightEnd(0.03, 0.005, _em);
    //PullMiddle(Vec3f(0, 0.002, 0), 0.03, _em);
    //FixFloorLeft(0.4, _em);
    //_em.lb.resize(_em.X.size(), Vec3f(-1000, -1, -1000));
    //SetFloorForRightSide(0.0055, 0.4, _em);
    //FixMiddle(0.03, _em);
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

extern void TestSparse();

struct Segment {
  virtual Vec2f eval(float t)const=0;
  virtual float len() const= 0;
};

struct Arc {
  Vec2f x0;
  Vec2f x1;
  float radius=1;
  Vec2f eval(float t) const {}
  float len() const {}
};

struct Line{
  Vec2f x0;
  Vec2f x1;
};


struct Curve {
  std::vector<std::unique_ptr<Segment>> segs;
};

void MakeCurve() {

}

extern void CenterMeshes(std::string dir);

int main(int argc, char** argv) {
  CenterMeshes("F:/dolphin/meshes/20240502-test/");
  // PngToGrid(argc, argv);
  // CenterMeshes();
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
