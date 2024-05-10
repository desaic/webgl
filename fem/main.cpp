
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
#include "FastSweep.h"
#include "CSparseMat.h"
#include "cpu_voxelizer.h"
#include "VoxCallback.h"
#include "MeshUtil.h"
#include "AdapUDF.h"

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

void TestAdapDF() {
  Timer timer;
  timer.start();

  SDFMesh sdfMesh;
  TrigMesh mesh;
  timer.start();
  mesh.LoadObj("F:/dolphin/meshes/Falcon1.obj");
  timer.end();
  sdfMesh.SetVoxelSize(1);
  sdfMesh.SetBand(5);
  float sec = timer.getSeconds();
  std::cout << "load stl time " << sec << " s\n";
  sdfMesh.SetMesh(&mesh);
  timer.start();
  sdfMesh.Compute();
  timer.end();
  sec =timer.getSeconds();
  std::cout << "sdf time " << sec << " s\n";
  TrigMesh surf;
  sdfMesh.MarchingCubes(0.25, &surf);

  BBox box;
  ComputeBBox(mesh.v, box);
  float h = sdfMesh.GetVoxelSize();
  Array2D8u slice;
  Vec3f boxSize = box.vmax - box.vmin;
  unsigned sx = boxSize[0] / h;
  unsigned sy = boxSize[1] / h;
  slice.Allocate(sx, sy);
  slice.Fill(0);
  surf.SaveObj("F:/dump/falcon_march.obj");
  float z = 0.8f * box.vmin[2] + 0.2f * box.vmax[2];
  for (unsigned x = 0; x < sx; x++) {
    for (unsigned y = 0; y < sy; y++) {
      Vec3f coord((x + 0.5f) * h, (y + 0.5f) * h, z);
      coord += box.vmin;
      float dist = sdfMesh.DistTrilinear(coord);
      slice(x, y) = dist * 30;
    }
  }
  SavePngGrey("F:/dump/slice.png", slice);
}

void SaveSlice(float z, SDFMesh & sdf, const BBox & box, float h) {
  Array2D8u slice;
  Vec3f boxSize = box.vmax - box.vmin;
  unsigned sx = boxSize[0] / h;
  unsigned sy = boxSize[1] / h;
  slice.Allocate(sx, sy);
  slice.Fill(0);
  for (unsigned x = 0; x < sx; x++) {
    for (unsigned y = 0; y < sy; y++) {
      Vec3f coord((x + 0.5f) * h, (y + 0.5f) * h, z);
      coord += box.vmin;
      float dist = sdf.DistTrilinear(coord);
      slice(x, y) = dist * 30;
    }
  }
  SavePngGrey("F:/dump/slice.png", slice);
}

void SaveSlice(float z, const AdapDF * df, const BBox& box, float h) {
  Array2D8u slice;
  Vec3f boxSize = box.vmax - box.vmin;
  unsigned sx = boxSize[0] / h;
  unsigned sy = boxSize[1] / h;
  slice.Allocate(sx, sy);
  slice.Fill(0);
  for (unsigned x = 0; x < sx; x++) {
    for (unsigned y = 0; y < sy; y++) {
      Vec3f coord((x + 0.5f) * h, (y + 0.5f) * h, z);
      coord += box.vmin;
      float dist = df->GetCoarseDist(coord);
      slice(x, y) = dist * 10;
    }
  }
  SavePngGrey("F:/dump/slice.png", slice);
}

void SaveSlice(unsigned z, const Array3D<short> dist) {
  Array2D8u slice;
  Vec3u size= dist.GetSize();
  slice.Allocate(size[0],size[1]);
  slice.Fill(0);
  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {
      short d = dist(x, y, z);
      slice(x, y) = 50+ d / 50;
    }
  }
  SavePngGrey("F:/dump/dist"+std::to_string(z)+".png", slice);
}

enum class VoxLabel : unsigned char {
  UNVISITED = 0,
  INSIDE,
  OUTSIDE
};

void SaveSlice(unsigned z, const Array3D<VoxLabel> & labels) {
  Array2D8u slice;
  Vec3u size = labels.GetSize();
  unsigned sx = size[0];
  unsigned sy = size[1];
  slice.Allocate(sx, sy);
  slice.Fill(0);
  for (unsigned x = 0; x < sx; x++) {
    for (unsigned y = 0; y < sy; y++) {
      slice(x, y) = uint8_t(labels(x, y, z)) * 50;
    }
  }
  SavePngGrey("F:/dump/wrap_label" + std::to_string(z) + ".png", slice);
}

size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u& size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

Vec3u GridIdx(size_t l, const Vec3u& size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}

bool InBound(int x, int y, int z, const Vec3u& size) {
  return x >= 0 && y >= 0 && z >= 0 && x < int(size[0]) && y < int(size[1]) &&
         z < int(size[2]);
}

/// <param name="dist">distance grid</param>
/// <param name="distThresh">stop flooding if distance is less than this</param>
/// <returns></returns>
Array3D<VoxLabel> FloodOutside(const Array3D<short>& dist, float distThresh) { 
  Array3D<VoxLabel> label;
  Vec3u size = dist.GetSize();
  label.Allocate(size[0],size[1],size[2]);
  label.Fill(VoxLabel::UNVISITED);

  std::deque<size_t> q;
  //add 6 faces of the grid to q.
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      q.push_back(LinearIdx(0, y, z, size));
      q.push_back(LinearIdx(size[0] - 1, y, z, size));
    }
  }
  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {
      q.push_back(LinearIdx(x, y, 0, size));
      q.push_back(LinearIdx(x, y, size[2]-1, size));
    }
  }
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      q.push_back(LinearIdx(x, 0, z, size));
      q.push_back(LinearIdx(x, size[1]-1, z, size));
    }
  }
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                               {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  while (!q.empty()) {
    size_t linearIdx = q.front();    
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    label(idx[0], idx[1], idx[2]) = VoxLabel::OUTSIDE;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      VoxLabel nLabel = label(ux, uy, uz);
      size_t nLinear = LinearIdx(ux, uy, uz, size);
      if (nLabel == VoxLabel::UNVISITED) {
        uint16_t nDist = dist(ux, uy, uz);
        if (nDist >= distThresh ) {
          label(ux, uy, uz) = VoxLabel::OUTSIDE;
          q.push_back(nLinear);
        }
      }
    }
  }

  return label;
}

void ShrinkWrap(TrigMesh& meshIn, TrigMesh& meshOut, float ballRadius) {
  std::shared_ptr<AdapUDF> udf = std::make_shared<AdapUDF>();
  SDFImpAdap * dfImp = new SDFImpAdap(udf);
  SDFMesh sdfMesh(dfImp);
  sdfMesh.SetMesh(&meshIn);
  float h = 1.0f;
  sdfMesh.SetBand(ballRadius / h);
  sdfMesh.SetVoxelSize(h);
  Timer timer;
  timer.start();

  //compute coarse grid only
  udf->voxSize = h;
  //band controls grid margin at first
  float distUnit = 1e-2;
  udf->band = 3;
  udf->distUnit = distUnit;
  udf->ClearTemporaryArrays();
  udf->BuildTrigList(&meshIn);
  udf->Compress();
  udf->ComputeCoarseDist();
  Array3D8u frozen;
  //band now controls how far fastsweep propagates
  udf->band = ballRadius / h;
  udf->FastSweepCoarse(frozen);
  timer.end();
  //distance >= thresh is considered outside.
  float outsideThresh = (ballRadius - udf->voxSize) / udf->distUnit;
  Array3D<VoxLabel> labels = FloodOutside(udf->dist, outsideThresh);
  Vec3u size = labels.GetSize();
  frozen.Allocate(size[0], size[1], size[2]);
  frozen.Fill(0);
  // distance from outermost shell
  Array3D<short> borderDist;
  borderDist.Allocate(size[0], size[1], size[2]);
  for (size_t i = 0;i<labels.GetData().size();i++) {
    if (labels.GetData()[i] == VoxLabel::OUTSIDE) {
      borderDist.GetData()[i] =
          ballRadius / h / distUnit - udf->dist.GetData()[i];
      frozen.GetData()[i] = 1;
    } else {
      borderDist.GetData()[i] = AdapDF::MAX_DIST;
    }
  }

  FastSweepParUnsigned(borderDist, udf->voxSize, distUnit,
                       udf->band + 2,
                       frozen);
  for (size_t i = 0; i < borderDist.GetData().size(); i++) {
    short d = borderDist.GetData()[i];
    if (d >= AdapDF::MAX_DIST) {
      d = -2 * h / distUnit;      
    } else {
      d = short(ballRadius / h / distUnit - d);
    }

    udf->dist.GetData()[i] = std::min(udf->dist.GetData()[i],
                                      d);
  }
  float sec = timer.getSeconds();
  BBox box;
  ComputeBBox(meshIn.v, box);
  std::cout << "sdf time " << sec << "s\n";
  sdfMesh.MarchingCubes(h, &meshOut);
}

void TestShrinkWrap() { 
  float radius = 10;
  TrigMesh mesh;
  mesh.LoadObj("F:/dolphin/meshes/vase.obj");
  TrigMesh wrap;
  ShrinkWrap(mesh, wrap, radius);
  wrap.SaveObj("F:/dump/wrap_vase_10mm.obj");
}

int main(int argc, char** argv) {
//  MakeCurve();
  // PngToGrid(argc, argv);
  // CenterMeshes();
  TestShrinkWrap();
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
