#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
void p157() {
  std::cout << "157\n";
  //abp = (a+b)10^n
  size_t MAX_N = 1e1;
  size_t cnt = 0;
  for (int n = 1; n <= 9; n++) {
  //a<=b.
    std::cout << n << "\n";
  for (size_t b = 1; b <= 2 * MAX_N; b++) {
    size_t minp = MAX_N / b + 1;
    size_t maxp = 2*MAX_N / b;
    size_t b10n = b * MAX_N;
    for (size_t p = minp; p <= maxp; p++) {
      size_t num = (p*b - MAX_N);
      if (b10n % num == 0) {
      //  std::cout << b << " " << (b10n / num) << "\n";
        cnt++;
      }
    }
  }
  MAX_N *=10;
  }

  std::cout << cnt << "\n";
}