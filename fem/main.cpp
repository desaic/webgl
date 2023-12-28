
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
  out.Fill(255);
  for (size_t i = 0; i < width; i++) {
    for (size_t j = 0; j < width; j++) {
      if ( (i/cellSize + j/cellSize) % 2 == 0) {
        continue;
      }
      for (unsigned chan = 0; chan < 3; chan++) {
        out(4 * i + chan, j) = 0;
      }
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
      floor->uv[i] *= 50;
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

  void LoadElementMesh(const std::string& file) {
    _em.LoadTxt(file);
    UpdateRenderMesh();
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
};

void TestFEM() {
  ElementMesh em;
  em.LoadTxt("F:/github/webgl/fem/data/hex_m.txt");
  float ene = em.GetElasticEnergy();
  std::cout << "E: " << ene << "\n";
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
