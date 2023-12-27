#include "MaterialModel.h"
#include "femError.h"
#include "ElementMesh.h"
#include "StrainEneNeo.h"

MaterialModel::MaterialModel() {
  enePtr = std::make_shared<StrainEneNeo>();
  Young2Lame(1e6, 0.4f, enePtr->param.data());
}

float MaterialModel::GetEnergy(size_t eidx, const ElementMesh& mesh) const {
  float ene = 0;
  const Element& ele = *(mesh.e[eidx]);
  if (quadType == QUADRATURE_TYPE::GAUSS2) {
    const auto q = Quadrature::Gauss2;
    for (int ii = 0; ii < q.x.size(); ii++) {
      Matrix3f F = ele.DefGradGauss2(ii, mesh.X, mesh.x);
      double det = F.determinant();
      if (det <= 0) {
        fem_error = -1;
      }
      ene += ele.DetJ(ii) * q.w[ii] * enePtr->getEnergy(F);
    }
  }
  return ene;
}
