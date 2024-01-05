#include "UILib.h"
#include "Vec2.h"
#include "Vec3.h"
#include <vector>

struct UVState {
  UVState() {}
  UVState(TrigMesh& m) : mesh(&m) { Init(); }

  void Init();

  TrigMesh* mesh = nullptr;
  std::vector<uint32_t> trigLabels;
  /// temporary variable. overwritten by each chart.
  std::vector<uint8_t> vertLabels;
  /// <summary>
  /// Maps from vertex index to list of triangle indices.
  /// </summary>
  std::vector<std::vector<uint32_t> > vertToTrig;
  /// <summary>
  /// Per-triangle uv coordinates.
  /// </summary>
  std::vector<Vec2f> uv;

  uint32_t numCharts = 0;
  /// list of triangles for each chart
  std::vector<std::vector<uint32_t> > charts;

  float distortionBound = 2.0f;

  // debug states
  std::vector<unsigned> trigLevel;

  // temporary vertex uv. can be overwritten by new charts.
  std::vector<Vec2f> tmpUV;
  std::vector<unsigned> rejectedVerts;
};

struct DebuggerState {
  DebuggerState() {
    trigLabelImage.Allocate(4096, 1024);
    FillRainbowColor(trigLabelImage);
    trigLabelImage(0, 0) = 255;
    trigLabelImage(1, 0) = 0;
    trigLabelImage(2, 0) = 0;
  }
  unsigned displayTrigLevel = 50;
  int sliderId = 0;
  std::shared_ptr<Slideri> levelSlider;
  std::shared_ptr<CheckBox> trigLabelCheckBox;
  std::vector<Vec2f> uv;
  UVState uvState;
  void SetMesh(TrigMesh* m) {
    uvState.mesh = m;
    uvState.Init();
    uv = m->uv;
  }
  Array2D8u trigLabelImage;
  Array2D8u texImage;
  bool showTrigLabels = false;

  UILib* ui = nullptr;
  int meshId = 0;
  void Refresh();
  void LevelSliderChanged();
  void InitUI(UILib* ui_in) { ui = ui_in;
    sliderId = ui->AddSlideri("trig level", 20, 0, 9999);
    levelSlider =
        std::dynamic_pointer_cast<Slideri>(ui->GetWidget(sliderId));
    int boxId = ui->AddCheckBox("Show Trig Label", false);
    trigLabelCheckBox =
        std::dynamic_pointer_cast<CheckBox>(ui->GetWidget(boxId));
    ui->SetKeyboardCb(std::bind(&DebuggerState::HandleKeys, this, std::placeholders::_1));
  }

  void HandleKeys(KeyboardInput input) {
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
};

void DebugUV(UILib & ui);