#include <iostream>
#include <vector>
#include "Digits.hpp"
int sum(const std::vector<uint8_t> & v) {
  int s = 0;
  for (size_t i = 0; i < v.size(); i++) {
    s += v[i];
  }
  return s;
}

void p154() {
  std::cout << "154\n";
  const int N = 200000;
  std::vector<uint8_t> nBase2 = base(N,2);
  std::vector<uint8_t> nBase5 = base(N, 5);
  std::vector<uint8_t> xBase2(1,0);
  std::vector<uint8_t> xBase5(1, 0);
  const int MIN_POW = 12;

  int S2n = sum(nBase2);
  int S5n = sum(nBase5);
  size_t cnt = 0;

  std::vector<int> base2Sums(N + 1, 0);
  std::vector<int> base5Sums(N + 1, 0);
  std::vector<uint8_t> iBase2(1, 0);
  std::vector<uint8_t> iBase5(1, 0);

  for (int i = 0; i <= N; i++) {
    base2Sums[i] = sum(iBase2);
    base5Sums[i] = sum(iBase5);
    incrementBase(iBase2, 2);
    incrementBase(iBase5, 5);
  }

  for (int x = 0; x <N; x++) {
    //std::vector<uint8_t> yBase2(1, 0);
    //std::vector<uint8_t> yBase5(1, 0);
    //std::vector<uint8_t> nxBase2 = base(N-x, 2);
    //std::vector<uint8_t> nxBase5 = base(N-x, 5);
    //int xPow2 = sum(xBase2);
    //int xPow5 = sum(xBase5);
    if (x % 100 == 0) {
      std::cout << x << "\n";
    }
    int xPow2 = base2Sums[x];
    int xPow5 = base5Sums[x];
    for (int y = 0; y <= N - x; y++ ) {
      //int yPow2 = sum(yBase2);
      //int yPow5 = sum(yBase5);
      int yPow2 = base2Sums[y];
      int yPow5 = base5Sums[y];
      int zPow2 = base2Sums[N-x-y];
      int zPow5 = base5Sums[N - x - y];

      int v2 = xPow2 + yPow2 +zPow2 - S2n;
      int v5 = (xPow5 + yPow5 +zPow5 - S5n) / 4;
      if (v2 >= MIN_POW && v5 >= MIN_POW) {
        cnt++;
      }
      //incrementBase(yBase2, 2);
      //incrementBase(yBase5, 5);
    }
    //incrementBase(xBase2, 2);
    //incrementBase(xBase5, 5);
  }
  std::cout << cnt << "\n";
}
