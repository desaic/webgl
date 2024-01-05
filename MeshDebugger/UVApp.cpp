#include "UVApp.h"
std::vector<Vec3b> generateRainbowPalette(int numColors);
DebuggerState::DebuggerState() {
    trigLabelImage.Allocate(4096, 1024);
    FillRainbowColor(trigLabelImage);
    trigLabelImage(0, 0) = 255;
    trigLabelImage(1, 0) = 0;
    trigLabelImage(2, 0) = 0;
  }
  
  void DebuggerState::SetMesh(TrigMesh* m) {
    uvState.mesh = m;
    uvState.Init();
    uv = m->uv;
  }
  
  void DebuggerState::InitUI(UILib* ui_in) { ui = ui_in;
    sliderId = ui->AddSlideri("trig level", 20, 0, 9999);
    levelSlider =
        std::dynamic_pointer_cast<Slideri>(ui->GetWidget(sliderId));
    int boxId = ui->AddCheckBox("Show Trig Label", false);
    trigLabelCheckBox =
        std::dynamic_pointer_cast<CheckBox>(ui->GetWidget(boxId));
    ui->SetKeyboardCb(std::bind(&DebuggerState::HandleKeys, this, std::placeholders::_1));
  }

  void DebuggerState::HandleKeys(KeyboardInput input) {
    std::vector<std::string> keys = input.splitKeys();
    for (const auto& key : keys) {
      bool sliderChange = false;
      int sliderVal = 0;
      if (key == "LeftArrow") {
        sliderChange = true;
        sliderVal = levelSlider->GetVal();
        sliderVal--;
      } else if (key == "RightArrow") {
        sliderVal = levelSlider->GetVal();
        sliderVal++;
        sliderChange = true;
      }

      if (sliderChange) {
        levelSlider->SetVal(sliderVal);
        LevelSliderChanged();
      }
    }
  }
  
void FillRainbowColor(Array2D8u& image) {
  Vec2u size = image.GetSize();
  size_t numPix = (size[0] / 4) * size[1];
  std::vector<Vec3b> colors = generateRainbowPalette(int(numPix));
  auto& data = image.GetData();
  for (size_t i = 0; i < data.size() / 4; i++) {
    size_t colorIdx = i % colors.size();
    data[4 * i] = colors[colorIdx][0];
    data[4 * i + 1] = colors[colorIdx][1];
    data[4 * i + 2] = colors[colorIdx][2];
    data[4 * i + 3] = 255;
  }
}

void ShowTrigsUpToLevel(unsigned maxlevel, DebuggerState& debState);

void TestImageDisplay(UILib& ui) {
  Array2D8u image(300, 100);
  int imageId = ui.AddImage();
  image.Fill(128);
  Vec2u size = image.GetSize();
  Array2D8u imageA(size[0] / 3 * 4, size[1]);
  for (size_t row = 0; row < size[1]; row++) {
    for (size_t col = 0; col < size[0] / 3; col++) {
      imageA(4 * col, row) = image(3 * col, row);
      imageA(4 * col + 1, row) = image(3 * col + 1, row);
      imageA(4 * col + 2, row) = image(3 * col + 2, row);
      imageA(4 * col + 3, row) = 255;
    }
  }
  ui.SetImageData(imageId, image, 3);
}

void UVState::Init() {
  size_t numTrigs = mesh->GetNumTrigs();
  size_t numVerts = mesh->GetNumVerts();
  trigLabels.clear();
  trigLabels.resize(numTrigs, 0);
  vertToTrig.clear();
  vertToTrig.resize(numVerts);
  for (size_t i = 0; i < numTrigs; i++) {
    for (size_t j = 0; j < 3; j++) {
      size_t vIdx = mesh->t[3 * i + j];
      vertToTrig[vIdx].push_back(i);
    }
  }
  if (mesh->nt.size() != mesh->t.size()) {
    mesh->ComputeTrigNormals();
  }
  numCharts = 0;
  charts.clear();
  vertLabels.clear();
  vertLabels.resize(numVerts, 0);
  tmpUV.resize(numVerts);
  trigLevel.resize(numTrigs, 0);
  uv = mesh->uv;
}

void LoadUVSeg(const std::string& segFile, UVState& state) {
  std::ifstream in(segFile);
  std::string line;
  std::vector<unsigned> chartId;
  std::vector<unsigned> level;
  unsigned numTrigs = 0;
  unsigned trigId = 0;
  unsigned maxChartId = 0;
  while (std::getline(in, line)) {
    if (line.size() == 0) {
      continue;
    }
    if (line[0] == '#') {
      continue;
    }
    std::istringstream ss(line);
    if (line.find("num_trigs") != std::string::npos) {
      std::string tok;
      ss >> tok >> numTrigs;
      if (numTrigs > 0 || numTrigs < state.mesh->GetNumTrigs()) {
        chartId.resize(numTrigs);
        level.resize(numTrigs);
      } else {
        std::cout << "invalid num trigs " << numTrigs << "\n";
        return;
      }
    } else {
      ss >> chartId[trigId] >> level[trigId];
      maxChartId = std::max(chartId[trigId], maxChartId);
      trigId++;
    }
  }
  state.trigLevel = level;
  state.charts.resize(maxChartId + 1);
  for (size_t t = 0; t < chartId.size(); t++) {
    state.charts[chartId[t]].push_back(t);
  }
  state.trigLabels = chartId;
}

std::vector<Vec3b> generateRainbowPalette(int numColors) {
  std::vector<Vec3b> colors;

  float frequency =
      M_PI /
      numColors;  // Controls the frequency of the rainbow (adjust as needed)
  for (int i = 0; i < numColors; ++i) {
    int red = static_cast<uint8_t>(std::sin(frequency * i) * 127 + 128);
    int green = static_cast<uint8_t>(
        std::sin(frequency * i + 2 * M_PI / 3) * 127 + 128);
    int blue = static_cast<uint8_t>(
        std::sin(frequency * i + 4 * M_PI / 3) * 127 + 128);
    colors.push_back(Vec3b(red, green, blue));
  }

  return colors;
}

void MakePerPixelUV(const std::vector<uint32_t> & trigLabels, TrigMesh& mesh,
                    unsigned width) {
  unsigned numTrig = std::min(mesh.GetNumTrigs(), trigLabels.size());
  float pixSize = 1.0f / width;
  for (unsigned t = 0; t < numTrig; t++) {
    unsigned l = (trigLabels[t] * 100000)%(width*width);
    unsigned row = l / width;
    unsigned col = l % width;
    Vec2f uv((col + 0.5f) * pixSize, (row + 0.5f) * pixSize);
    mesh.uv[3 * t] = uv;
    mesh.uv[3 * t + 1] = uv;
    mesh.uv[3 * t + 2] = uv;
  }
}

void DebuggerState::LevelSliderChanged() {
  displayTrigLevel = levelSlider->GetVal();
  ShowTrigsUpToLevel(displayTrigLevel, *this);
}

void DebuggerState::Refresh() {
  if(!uvState.mesh){return;}
  bool checkBoxVal = trigLabelCheckBox->GetVal();
  if (showTrigLabels != checkBoxVal) {
    showTrigLabels = checkBoxVal;
    if (showTrigLabels) {
      Vec2u texSize = trigLabelImage.GetSize();
      MakePerPixelUV(uvState.trigLabels, *uvState.mesh, texSize[0] / 4);
      ui->SetMeshTex(meshId, trigLabelImage, 4);
      //LevelSliderChanged();
    } else {
      uvState.mesh->uv = uv;
      ui->SetMeshTex(meshId, texImage, 4);
    }
    ui->SetMeshNeedsUpdate(meshId);
  }
  int sliderVal = levelSlider->GetVal();
  if (displayTrigLevel != sliderVal) {
    LevelSliderChanged();
  }
}

DebuggerState debState;

void ShowTrigsUpToLevel(unsigned maxlevel, DebuggerState& debState) {
  if (debState.showTrigLabels) {
    auto& image = debState.trigLabelImage;
    Vec2u size = image.GetSize();
    unsigned width = size[0] / 4;
    unsigned lastIdx = width * size[1] - 1;
    auto& mesh = *(debState.uvState.mesh);
    unsigned numTrig = std::min(mesh.GetNumTrigs(), debState.uvState.trigLabels.size());
    float pixSize = 1.0f / size[0];
    for (size_t t = 0; t < numTrig; t++) {
      unsigned label = debState.uvState.trigLabels[t];
      unsigned tlevel = debState.uvState.trigLevel[t];
      if (tlevel > maxlevel) {
        label = lastIdx;
      }
      unsigned row = label / width;
      unsigned col = label % width;
      Vec2f uv = pixSize * Vec2f(col + 0.5f, row + 0.4f);
      mesh.uv[3 * t] = uv;
      mesh.uv[3 * t + 1] = uv;
      mesh.uv[3 * t + 2] = uv;
    }
    debState.ui->SetMeshNeedsUpdate(debState.meshId);
  }
}

void DebugUV(UILib & ui) {
  auto mesh = std::make_shared<TrigMesh>();
  mesh->LoadObj("F:/dump/uv_out.obj");
  int meshId = ui.AddMesh(mesh);
  ui.SetMeshColor(meshId, Vec3b(250, 180, 128));
  debState.SetMesh(mesh.get());
  LoadUVSeg("F:/dump/seg.txt", debState.uvState);

  Array2D8u texImage;
  LoadPngColor("F:/dump/checker.png", texImage);
  ConvertChan3To4(texImage, debState.texImage);
  ui.SetMeshTex(meshId, debState.texImage, 4);
  debState.meshId = meshId;
}
