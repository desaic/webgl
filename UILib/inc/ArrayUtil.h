#include <string>

#include "Array2D.h"
#include "Vec3.h"

Array2Df dlmread(const std::string& file);

void dlmwrite(const std::string& outfile, const Array2Df& mat);

void Add(std::vector<Vec3f>& dst, const std::vector<Vec3f>& src);

void AddTimes(std::vector<Vec3f>& dst, const std::vector<double>& src, float c);

std::vector<Vec3f> operator*(float c, const std::vector<Vec3f>& v);

void Fix(std::vector<Vec3f>& dx, const std::vector<bool> fixedDOF);

Vec3f MaxAbs(const std::vector<Vec3f>& v);

void ptwiseProd(const std::vector<double>& a, const std::vector<double>& b,
                std::vector<double>& c);

void sub(std::vector<double>& src, const std::vector<double>& b);

std::vector<double> ToDouble(const std::vector<float>& a);

double Linf(const std::vector<double>& R);

double L2(const std::vector<double>& R);