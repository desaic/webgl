#include <iostream>
#include <vector>
#include <array>

template <typename T>
class Matrix {
public:

  Matrix() :s({ 0,0 }) {}

  T& operator()(size_t row, size_t col) {
    return v[row * s[1] + col];
  }

  const T& operator()(size_t row, size_t col) const {
    return v[row * s[1] + col];
  }

  void Allocate(size_t rows, size_t cols) {
    s[0] = rows;
    s[1] = cols;
    v.resize(rows * cols);
  }

  void Fill(T val) {
    std::fill(v.begin(), v.end(), val);
  }

  //rows and cols.
  std::array<size_t, 2> s;
  std::vector <T> v;
  std::string ToString() const {
    std::ostringstream oss;
    for (size_t row = 0; row < s[0]; row++) {
      for (size_t col = 0; col < s[1]; col++) {
        oss << int((*this)(row, col)) << " ";
      }
      oss << "\n";
    }
    return oss.str();
  }
};

typedef Matrix<unsigned short> Matrixu16;
typedef std::array<int, 2> Vec2i;

int sp9() {
//int main() {
  int numCases;
  std::cin >> numCases;
  for (int c = 0; c < numCases; c++) {
    int rows, cols;
    std::cin >> rows >> cols;
    Matrixu16 h;
    h.Allocate(rows, cols);
    for (int row = 0; row < rows; row++) {
      for (int col = 0; col < cols; col++) {
        std::cin >> h(row, col);
      }
    }
    Vec2i p0, p1;
    std::cin >> p0[0] >> p0[1];
    std::cin >> p1[0] >> p1[1];
  }
  return 0;
}