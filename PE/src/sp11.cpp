#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <queue>
size_t countFactor(size_t n, unsigned f) {
  size_t count = 0;
  size_t p = f;
  while (n >= p) {
    count += n / p;
    p *= f;
  }
  return count;
}

int sp11() {
//int main() {
  int numCases;
  std::cin >> numCases;
  for (int c = 0; c < numCases; c++) {
    size_t N;
    std::cin >> N;
    size_t factor5 = countFactor(N, 5);
    size_t factor2 = countFactor(N, 2);
    size_t cnt = factor5;
    if (factor2 < factor5) {
      cnt = factor2;
    }
    std::cout << cnt << "\n";
  }
  return 0;
}
