#include "ArrayUtil.h"

#include <fstream>
#include <sstream>

Array2Df dlmread(const std::string& file) {
  std::ifstream in(file);
  std::string line;
  std::vector<std::vector<float> > rows;
  size_t minLen = 10000000000ull;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::vector<float> row;
    float val;
    std::string token;
    while (std::getline(iss, token, ',')) {
      try {
        val = stof(token);
      } catch (const std::exception& e) {
        break;
      }
      row.push_back(val);
    }
    if (row.size() > 0) {
      minLen = std::min(minLen, row.size());
      rows.push_back(row);
    } else {
      break;
    }
  }
  if (rows.size() == 0) {
    return Array2Df();
  }
  Array2Df out(minLen, rows.size());
  Vec2u size = out.GetSize();
  for (unsigned row = 0; row < size[1]; row++) {
    for (unsigned col = 0; col < size[0]; col++) {
      out(col, row) = rows[row][col];
    }
  }
  return out;
}

void dlmwrite(const std::string& file, const Array2Df& mat) {
  std::ofstream out(file);
  Vec2u size = mat.GetSize();
  for (unsigned row = 0; row < size[1]; row++) {
    for (unsigned col = 0; col < size[0] - 1; col++) {
      out << mat(col, row) << ",";
    }
    out << mat(size[0] - 1, row) << "\n";
  }
}

void Add(std::vector<Vec3f>& dst, const std::vector<Vec3f>& src) {
  for (size_t i = 0; i < dst.size(); i++) {
    dst[i] += src[i];
  }
}

void AddTimes(std::vector<Vec3f>& dst, const std::vector<double>& src,
              float c) {
  for (size_t i = 0; i < dst.size(); i++) {
    dst[i][0] += c * src[3 * i];
    dst[i][1] += c * src[3 * i + 1];
    dst[i][2] += c * src[3 * i + 2];
  }
}

std::vector<Vec3f> operator*(float c, const std::vector<Vec3f>& v) {
  std::vector<Vec3f> out(v.size());
  for (size_t i = 0; i < out.size(); i++) {
    out[i] = c * v[i];
  }
  return out;
}

void Fix(std::vector<Vec3f>& dx, const std::vector<bool> fixedDOF) {
  for (size_t i = 0; i < dx.size(); i++) {
    for (size_t j = 0; j < 3; j++) {
      if (fixedDOF[3 * i + j]) {
        dx[i][j] = 0;
      }
    }
  }
}

Vec3f MaxAbs(const std::vector<Vec3f>& v) {
  if (v.size() == 0) {
    return Vec3f(0, 0, 0);
  }

  Vec3f ma = v[0];

  for (size_t i = 1; i < v.size(); i++) {
    for (size_t j = 0; j < 3; j++) {
      ma[j] = std::max(ma[j], std::abs(v[i][j]));
    }
  }
  return ma;
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
  double sum = 0;
  for (size_t i = 0; i < R.size(); i++) {
    sum += R[i] * R[i];
  }
  sum = std::sqrt(sum);
  return sum;
}