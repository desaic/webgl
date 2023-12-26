#include "Matrix3fSVD.h"
#include "svd3.h"
#include <iostream>

Vec3f SVD(const Matrix3f& a, Matrix3f& U, Matrix3f& V) {
  Vec3f s;
  // clang-format off
  float s12, s13;
  float s21, s23;
  float s31, s32;
  svd(a(0,0), a(0,1), a(0,2),
      a(1,0), a(1,1), a(1,2),
      a(2,0), a(2,1), a(2,2),
      U(0,0), U(0,1), U(0,2),
      U(1,0), U(1,1), U(1,2),
      U(2,0), U(2,1), U(2,2),
      s[0]  ,    s12,    s13,
      s21   ,   s[1],    s23,
      s31   ,    s32,   s[2],
      V(0,0), V(0,1), V(0,2),
      V(1,0), V(1,1), V(1,2),
      V(2,0), V(2,1), V(2,2)
    );
  // clang-format on
  return s;
}

void PrintMat(const Matrix3f& mat, std::ostream& out) {
  for (unsigned i = 0; i < 3; i++) {
    for (unsigned j = 0; j < 3; j++) {
      out << mat(i, j) << " ";
    }
    out << "\n";
  }
}

void TestSVD() {
  Matrix3f mat(1, 0, 2, 0, 1, 3, 0, 2, 4);
  Vec3f s;
  Matrix3f U, V;
  s = SVD(mat, U, V);
  std::cout << s[0] << " " << s[1] << " " << s[2] << "\n";
  std::cout << "\n";
  PrintMat(U, std::cout);
  std::cout << "\n";
  PrintMat(V, std::cout);
  std::cout << "\n";

  Matrix3f sdiag = Matrix3f::Diag(s[0], s[1], s[2]);
  Matrix3f prod = U * sdiag * V.transposed();
  PrintMat(prod, std::cout);
  std::cout << "\n";

  Matrix3f UUT = U * U.transposed();
  PrintMat(UUT, std::cout);
  std::cout << "\n";

  Matrix3f VVT = V * V.transposed();
  PrintMat(VVT, std::cout);
  std::cout << "\n";
}
