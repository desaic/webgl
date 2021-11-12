//https://www.spoj.com/problems/TETRA/
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

double trigArea(int u, int v, int w)
{
  double prod = (double(u + v + w)) * double(v - w + u) * double(w - u + v) * double(u - v + w);
  double A = std::sqrt(prod) * 0.25;
  return A;
}
//gives wrong answer without ordering edges.
double tetVolV0(const std::vector<int>& s)
{
  //uvwUVW
  //012345
  int u = s[0];
  int v = s[1];
  int w = s[2];
  int U = s[3];
  int V = s[4];
  int W = s[5];

  int XU = w - U + v;
  int Xv = U - v + w;
  int Xw = v - w + U;
  int YV = u - V + w;
  int Yw = V - w + u;
  int Yu = w - u + V;
  int ZW = v - W + u;
  int Zu = W - u + v;
  int Zv = u - v + W;
  int X0 = U + v + w;
  int Y0 = W + w + u;
  int Z0 = W + u + v;
  double X = double(XU) * X0;
  double Y = double(YV) * Y0;
  double Z = double(ZW) * Z0;
  double x = double(Xv) * Xw;
  double y = double(Yw) * Yu;
  double z = double(Zu) * Zv;
  double xi = std::sqrt(x * Y * Z);
  double eta = std::sqrt(y * Z * X);
  double zeta = std::sqrt(z * X * Y);
  double lambda = std::sqrt(x * y * z);
  double prod = (xi + eta + zeta - lambda) * (lambda + xi + eta - zeta) *
    (eta + zeta + lambda - xi) * (zeta + lambda + xi - eta);
  double vol = std::sqrt(prod) / (192.0 * u * v * double(w));
  return vol;
}

static double tetVolV1(const std::vector<int>& s)
{
  //uvwUVW
  //012345
  int a = s[0];
  int b = s[1];
  int c = s[2];
  int x = s[3];
  int y = s[4];
  int z = s[5];

  double X = b * b + c * c - x * x;
  double Y = a * a + c * c - y * y;
  double Z = a * a + b * b - z * z;
  double abc = double(a) * b * c;
  double XYZ = double(X) * Y * double(Z);
  double sum = 4.0 * abc * abc - a * a * X * X - b * b * Y * Y - c * c * Z * Z + XYZ;
  double vol = std::sqrt(sum) / 12.0;
  return vol;
}

int sp16()
//int main(int argc, char* argv[])
{
  int N = 0;
  std::cin >> N;

  for (int i = 0; i < N; i++) {
    std::vector<int> sides(6, 0);
    for (size_t j = 0; j < sides.size(); j++) {
      std::cin >> sides[j];
    }
    //swap edge 3 and 5 to be consistent to volume formula
    int tmp = sides[3];
    sides[3] = sides[5];
    sides[5] = tmp;
    double areas[4];
    areas[0] = trigArea(sides[0], sides[1], sides[5]);
    areas[1] = trigArea(sides[0], sides[2], sides[4]);
    areas[2] = trigArea(sides[1], sides[2], sides[3]);
    areas[3] = trigArea(sides[3], sides[4], sides[5]);
    double v = tetVolV1(sides);
    double areaSum = 0;
    for (unsigned j = 0; j < 4; j++) {
      areaSum += areas[j];
    }
    //areas * rad / 3 = vol
    double rad = 3 * v / areaSum;

    std::cout << std::fixed << std::setprecision(4) << rad << "\n";
  }

  return 0;
}
