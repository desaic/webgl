#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>

std::vector<int> OrderEdges(const std::vector<int>& s)
{
  std::vector<int> s_out=s;
  int u = s[0];
  int v = s[1];
  int w = s[2];
  int U = s[3];
  int V = s[4];
  int W = s[5];

  int orders[4][6] =
  { {0,4,5,3,1,2},  //X
    {1,3,5,4,0,2},
    {2,3,4,5,0,1},
    {0,1,2,3,4,5} }; //exclude face A

  //face diffs
  int fd[12];
  fd[0]  = w - U + v;
  fd[1]  = U - v + w;
  fd[2]  = v - w + U;
  fd[3]  = u - V + w;
  fd[4]  = V - w + u;
  fd[5]  = w - u + V;
  fd[6]  = v - W + u;
  fd[7]  = W - u + v;
  fd[8]  = u - v + W;
  fd[9]  = V - W + U;
  fd[10] = W - U + V;
  fd[11] = U - V + W;

  std::vector<int> sorted(fd, fd+12);
  std::sort(sorted.begin(), sorted.end());

  int minCount[4] = { 0,0,0,0 };
  int thresh = sorted[2];
  for (int i = 0; i < 12; i++) {
    if (fd[i] <= thresh) {
      minCount[i / 3] ++;
    }
  }

  int minVCount = minCount[0];
  int minV = 0;
  for (int v = 1; v < 4; v++) {
    if (minCount[v] < minVCount) {
      minVCount = minCount[v];
      minV = v;
    }
  }

  for (int i = 0; i < 6; i++) {
    s_out[i] = s[orders[minV][i]];
  }

  u = s_out[0];
  v = s_out[1];
  w = s_out[2];
  U = s_out[3];
  V = s_out[4];
  W = s_out[5];

  fd[0] = w - U + v;
  fd[1] = U - v + w;
  fd[2] = v - w + U;
  fd[3] = u - V + w;
  fd[4] = V - w + u;
  fd[5] = w - u + V;
  fd[6] = v - W + u;
  fd[7] = W - u + v;
  fd[8] = u - v + W;
  fd[9] = V - W + U;
  fd[10] = W - U + V;
  fd[11] = U - V + W;

  for (int i = 0; i < 12; i++) {
    std::cout << fd[i] << " ";
  }
  std::cout << "\n";

  return s_out;
}

//gives wrong answer without ordering edges.
static double tetVolV0(const std::vector<int>& edges0)
{
  std::vector <int> edges = OrderEdges(edges0);
  //uvwUVW
  //012345
  int u = edges[0];
  int v = edges[1];
  int w = edges[2];
  int U = edges[3];
  int V = edges[4];
  int W = edges[5];

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
  long double  X = double(XU) * X0;
  long double  Y = double(YV) * Y0;
  long double  Z = double(ZW) * Z0;
  long double  x = double(Xv) * Xw;
  long double  y = double(Yw) * Yu;
  long double  z = double(Zu) * Zv;
  long double p = std::sqrt(x * Y * Z);
  long double q = std::sqrt(y * Z * X);
  long double r = std::sqrt(z * X * Y);
  long double s = std::sqrt(x * y * z);
  long double prod = (q + r + s - p) * (p + r + s - q) *
    (p + q + s - r) * (p + q + r - s);
  long double  vol = std::sqrt(prod) / (192.0 * u * v * (w));
  return double(vol);
}

static long double tetVolV2(const std::vector<int>& s)
{
  //uvwUVW
  //012345
  int a1 = s[0];
  int a2 = s[1];
  int a3 = s[2];
  int a4 = s[5];
  int a5 = s[3];
  int a6 = s[4];

  long double  a12 = a1 * a1;
  long double  a22 = a2 * a2;
  long double  a32 = a3 * a3;
  long double  a42 = a4 * a4;
  long double  a52 = a5 * a5;
  long double  a62 = a6 * a6;
  
  long double  r1 = a12 * a52 * (a22 + a32 + a42 + a62 - a12 - a52);
  long double  r2 = a22 * a62 * (a12 + a32 + a42 + a52 - a22 - a62);
  long double  r3 = a32 * a42 * (a12 + a22 + a52 + a62 - a32 - a42);
  long double  r4 = a12 * a22 * a42 + a22 * a32 * a52 + a12 * a32 * a62 + a42 * a52 * a62;
  long double  sum = r1 + r2 + r3 - r4;
  long double vol = std::sqrt(sum) / 12.0;
  return double(vol);
}

//int sp23()
int main()
{
  int N = 0;
  std::cin >> N;

  for (int i = 0; i < N; i++) {
    std::vector<int> sides(6, 0);
    for (size_t j = 0; j < sides.size(); j++) {
      std::cin >> sides[j];
    }
    //AB AC AD BC BD CD =>
    //AB AC AD CD BD BC

    int tmp = sides[3];
    sides[3] = sides[5];
    sides[5] = tmp;
    double vol = tetVolV2(sides);
    std::cout << std::fixed;
    std::cout << std::setprecision(4);
    std::cout << vol << "\n";
  }
  return 0;
}
