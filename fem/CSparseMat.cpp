#include "CSparseMat.h"
#include "cg_rc.h"
#include <fstream>

void CSparseMat::Allocate(unsigned rows, unsigned cols, unsigned nz) {
  csMat.nzmax = nz;
  csMat.m = rows;
  csMat.n = cols;
  colStart.resize(cols + 1, 0);
  vals.resize(nz, 0);
  rowIdx.resize(nz, 0);
  // column pointers (size n+1) or col indices (size nzmax)
  csMat.p = colStart.data();
  // row indices, size nzmax
  csMat.i = rowIdx.data();
  // numerical values, size nzmax
  csMat.x = vals.data();
  // #of entries in triplet matrix, -1 for compressed - col
  csMat.nz = -1;
}

std::vector<float> CSparseMat::MultSimple(const float* v) const {
  std::vector<float> prod(Rows(), 0);
  for (unsigned col = 0; col < Cols(); col++) {
    float vcol = v[col];
    size_t st = colStart[col];
    size_t nnz = colStart[col + 1] - colStart[col];
    for (unsigned i = 0; i < nnz; i++) {
      unsigned row = rowIdx[st + i];
      float val = vals[st + i];
      prod[row] += vcol * val;
    }
  }
  return prod;
}

void CSparseMat::MultSimple(const double* v, double* b) const {
  for (unsigned row = 0; row < Rows(); row++) {
    b[row] = 0;
  }
  for (unsigned col = 0; col < Cols(); col++) {
    double vcol = v[col];
    size_t st = colStart[col];
    size_t nnz = colStart[col + 1] - colStart[col];
    for (unsigned i = 0; i < nnz; i++) {
      unsigned row = rowIdx[st + i];
      float val = vals[st + i];
      b[row] += vcol * val;
    }
  }
}

std::vector<float> CSparseMat::Diag() const {
  std::vector<float> d(Cols());
  for (unsigned col = 0; col < Cols(); col++) {
    size_t st = colStart[col];
    size_t nnz = colStart[col + 1] - st;
    for (unsigned i = 0; i < nnz; i++) {
      if (rowIdx[st + i] == col) {
        d[col] = vals[st + i];
        break;
      }
    }
  }
  return d;
}

int CSparseMat::ToDense(float* matOut) const {
  if (matOut == nullptr) {
    return -1;
  }
  size_t matSize = Rows() * size_t(Cols());
  if (matSize == 0) {
    return -2;
  }
  if (matSize > MAX_DENSE_MATRIX_ENTRIES) {
    return -3;
  }
  for (size_t i = 0; i < matSize; i++) {
    matOut[i] = 0;
  }
  for (unsigned col = 0; col < Cols(); col++) {
    size_t st = colStart[col];
    size_t numRows = colStart[col + 1] - st;
    for (unsigned i = 0; i < numRows; i++) {
      size_t row = rowIdx[st + i];
      double val = vals[st + i];
      matOut[row * Rows() + col] = val;
    }
  }
  return 0;
}

void FixDOF(std::vector<bool>& fixedDOF, CSparseMat& K, float scale) {

  unsigned rows = K.Rows();
  unsigned cols = K.Cols();
  for (unsigned col = 0; col < cols; col++) {
    size_t st = K.colStart[col];
    size_t numRows = K.colStart[col+1] - st;
    if (fixedDOF[col]) {
      for (unsigned i = 0; i < numRows; i++) {
        unsigned row = K.rowIdx[st + i];
        if (row == col) {
          K.vals[st + i] = scale;
        } else {
          K.vals[st + i] = 0;
        }
      }
    } else {
      for (unsigned i = 0; i < numRows; i++) {
        unsigned row = K.rowIdx[st + i];
        if (fixedDOF[row]) {
          K.vals[st + i] = 0;
        }
      }
    }
  }
}

void ptwiseProd(const std::vector<double>& a, const std::vector<double>& b,
                std::vector<double>& c) {
  for (size_t i = 0; i < c.size(); i++) {
    c[i] = a[i] * b[i];
  }
} 

void sub(std::vector<double>& src, const std::vector<double>& b) {
  for (size_t i = 0; i < src.size(); i++) {
    src[i] -= b[i];
  }
}

std::vector<double> ToDouble(const std::vector<float>& a) {
  std::vector<double> v(a.size());
  for (size_t i = 0; i < a.size(); i++) {
    v[i] = a[i];
  }
  return v;
}

double Linf(const std::vector<double>& R) {
  if (R.size() == 0) {
    return 0;
  }
  double m = R[0];
  for (size_t i = 0; i < R.size(); i++) {
    m = std::max(m, std::abs(R[i]));
  }
  return m;
}

double L2(const std::vector<double>& R) {
  if (R.size() == 0) {
    return 0;
  }
  double sum=0;
  for (size_t i = 0; i < R.size(); i++) {
    sum += R[i] * R[i];   
  }
  sum = std::sqrt(sum);
  return sum;
}

int CG(const CSparseMat& K, std::vector<double> & x, const std::vector<double>& b,
       int maxIter) {
  int job = 1;
  size_t N = K.Cols();
  std::vector<double> R(N), Z(N), P(N), Q(N), prod(N);
  int next_job = cg_rc(N, b.data(), x.data(), R.data(), Z.data(), P.data(), Q.data(),
              job);

  std::vector<float> diagInvf = K.Diag();

  for (size_t i = 0; i < diagInvf.size(); i++) {
    if (diagInvf[i] > 0) {
      diagInvf[i] = 1.0f / diagInvf[i];
    }
  }
  std::vector<double> diagInvd = ToDouble(diagInvf);
  
  double bmax = Linf(b);
  float reduction = 0.001f;
  double initL2 = L2(b);
  int ret = 0;
  std::ofstream log("F:/dump/CG_converge.txt");
  for (int i = 0; i < maxIter; i++) {  
    job = 2;
    if (next_job == 1) {
      K.MultSimple(P.data(), Q.data());
    } else if (next_job == 2) {
      //precond
      ptwiseProd(R, diagInvd, Z);
    } else if (next_job == 3) {
      K.MultSimple(x.data(), prod.data());
      sub(R, prod);
    } else if (next_job == 4) {
      double norm = Linf(R);
      double l2norm = L2(R);
      log << norm << ", ";
      log << l2norm << "\n";
      if (norm < bmax * reduction || l2norm<initL2*reduction) {
        ret = i;
        break;
      }
    } else {
      return -1;
    }
    next_job = cg_rc(N, b.data(), x.data(), R.data(), Z.data(), P.data(),
                     Q.data(), job);
  }
  return ret;
}
