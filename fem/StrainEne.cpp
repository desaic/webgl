#include "StrainEne.h"

#include <iostream>

#include "StableNeo.h"
#include "StrainCorotLin.h"
#include "StrainEneNeo.h"
#include "StrainLin.h"

StrainEne::~StrainEne() {}

///@param E array of 2 doubles. E and nu
///@param mu array of size 2. mu and lambda.
void Young2Lame(double E, double nu, float* mu) {
  mu[0] = E / (2 * (1 + nu));
  mu[1] = E * nu / ((1 + nu) * (1 - 2 * nu));
}

std::vector<StrainEne*> loadMaterials(std::istream& in,
                                      std::vector<double>& densities) {
  int nMat = 0;
  in >> nMat;
  std::vector<StrainEne*> materials(nMat, 0);
  for (int ii = 0; ii < nMat; ii++) {
    int matType = 0;
    in >> matType;
    int nParam = 0;
    in >> nParam;
    std::vector<double> param(nParam, 0);
    for (int jj = 0; jj < nParam; jj++) {
      in >> param[jj];
    }
    StrainEne* ene = 0;
    float density = 0;
    bool knownMat = true;

    switch (matType) {
      case StrainEne::LIN:
        ene = new StrainLin();
        ene->param.resize(2);
        Young2Lame(param[0], param[1], ene->param.data());
        break;

      case StrainEne::COROT:
        ene = new StrainCorotLin();
        ene->param.resize(2);
        Young2Lame(param[0], param[1], ene->param.data());
        break;

      case StrainEne::NEO:
        ene = new StrainEneNeo();
        ene->param.resize(2);
        Young2Lame(param[0], param[1], ene->param.data());
        break;
      case StrainEne::SNH:
        ene = new StableNeo();
        ene->param.resize(3);
        Young2Lame(param[0], param[1], ene->param.data());
        {
          float lambda = param[0] + param[1];
          float alpha = (1.0 + param[0] / (param[1] + 1e-9f));
          param[1] = lambda;
          param[2] = alpha;
        }
        break;
      default:
        std::cout << "Unrecognized material type " << matType << "\n";
        knownMat = false;
        break;
    }
    if (knownMat) {
      in >> density;
    }
    densities.push_back(density);
    materials[ii] = ene;
  }
  return materials;
}
