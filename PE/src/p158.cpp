
#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
void p158() {
  std::cout << "158\n";
  size_t n = 26;
  std::vector<size_t> nk(n + 1, 0);
  nk[0] = 1;
  for (int i = 1; i <= n; i++) {
    std::vector<size_t> prevRow = nk;
    for (int j = 1; j <= i; j++) {
      nk[j] = prevRow[j - 1] + prevRow[j];
    }
  }
  for (size_t k = 3; k <= n; k++) {
    size_t pow2 = std::pow(2, k) - k - 1;
    size_t comb = nk[k] * (pow2);
    std::cout << comb << "\n";
  }
}