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


std::vector<Vec3f> MaterialModel::GetForce(int eidx, const ElementMesh& mesh) const{
  const Element& ele = *(mesh.e[eidx]);
  int nV = ele.NumVerts();
  std::vector<Vec3f> f(nV, Vec3f(0,0,0));

  if (quadType == QUADRATURE_TYPE::GAUSS2) {
    const auto& q = Quadrature::Gauss2;
    std::vector<Matrix3f> P(q.w.size());
    for (unsigned int ii = 0; ii < q.x.size(); ii++) {
      Matrix3f F = ele.DefGradGauss2(ii, mesh.X, mesh.x);
      if (F.determinant() <= 0) {
        fem_error = -1;
      }
      P[ii] = enePtr->getPK1(F);
    }
    for (unsigned int jj = 0; jj < q.x.size(); jj++) {
      for (int ii = 0; ii < nV; ii++) {
        Vec3f gradN = ele.ShapeFunGradGauss2(jj, ii);
        Vec3f JdN = ele.Jinv(jj).transposed() * gradN;
        f[ii] -= ele.DetJ(jj) * q.w[jj] * (P[jj] * JdN);
      }
    }
  }

  return f;
}
