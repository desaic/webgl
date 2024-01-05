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
  DebuggerState();
  unsigned displayTrigLevel = 50;
  int sliderId = 0;
  std::shared_ptr<Slideri> levelSlider;
  std::shared_ptr<CheckBox> trigLabelCheckBox;
  std::vector<Vec2f> uv;
  UVState uvState;
  void SetMesh(TrigMesh* m);
  Array2D8u trigLabelImage;
  Array2D8u texImage;
  bool showTrigLabels = false;

  UILib* ui = nullptr;
  int meshId = 0;
  void Refresh();
  void LevelSliderChanged();
  void InitUI(UILib* ui_in);

  void HandleKeys(KeyboardInput input);
};

void DebugUV(UILib & ui);