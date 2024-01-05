#include "cad_app.h"
#include "ImageUtils.h"
void cad_app::Init(UILib* ui) {
  _ui = ui;
  _floor = std::make_shared<TrigMesh>();
  *_floor =
      MakePlane(Vec3f(-200, -0.1, -200), Vec3f(200, -0.1, 200), Vec3f(0, 1, 0));
  for (size_t i = 0; i < _floor->uv.size(); i++) {
    _floor->uv[i] *= 5;
  }
  Array2D8u checker;
  MakeCheckerPatternRGBA(checker);
  _floorMeshId = _ui->AddMesh(_floor);
  _ui->SetMeshTex(_floorMeshId, checker, 4);
}

void cad_app::Refresh() {

}