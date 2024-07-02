#include "MaterialModel.h"
#include "femError.h"
#include "ElementMesh.h"
#include "StrainEneNeo.h"
#include "StableNeo.h"

MaterialModel::MaterialModel() {
  enePtr = std::make_shared<StableNeo>();
  std::vector<float> param(2, 0);
  Young2Lame(1e6, 0.4f, param.data());
  enePtr->SetParam(param);
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

Array2Df MaterialModel::GetStiffness(int eidx, const ElementMesh& mesh) const{
  Array2Df K;
  const Element* ele = mesh.e[eidx].get();
  K.Allocate(3 * ele->NumVerts(), 3 * ele->NumVerts());
  K.Fill(0);
  if (quadType == QUADRATURE_TYPE::GAUSS2) {
    const auto& q = Quadrature::Gauss2;
    for (unsigned qi = 0; qi < q.x.size(); qi++) {
      Array2Df Kq = Stiffness(qi, ele, mesh);
      Kq *= (ele->DetJ(qi) * q.w[qi]);
      K += Kq;
    }
  }
  return K;
}

Array2Df MaterialModel::Stiffness(unsigned qi, const Element* ele,
                   const ElementMesh& em) const {
  Array2Df K;
  
  unsigned nV = ele->NumVerts();
  K.Allocate(3 * nV, 3 * nV);
  Matrix3f F = ele->DefGradGauss2(qi, em.X, em.x);
  const unsigned dim = 3;
  std::vector<Vec3f> JdN(nV);
  for (unsigned ii = 0; ii < nV; ii++) {
    Vec3f gradN = ele->ShapeFunGradGauss2(qi, ii);
    JdN[ii] = ele->Jinv(qi).transposed() * gradN;
  }
  for (unsigned ii = 0; ii < nV; ii++) {
    enePtr->CacheF(F);
    std::vector<Matrix3f> dP = enePtr->getdPdx(F, JdN[ii]);
    for (unsigned vv = 0; vv < nV; vv++) {
      for (unsigned jj = 0; jj < dim; jj++) {
        Vec3f dfdxi = dP[jj] * JdN[vv];
        for (unsigned kk = 0; kk < dim; kk++) {
          K(vv * dim + kk, ii * dim + jj) = dfdxi[kk];
          K(ii * dim + jj, vv * dim + kk) = dfdxi[kk];
        }
      }
    }
  }
  return K;
}
