#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
int DigitalRoot(size_t x) {
  size_t sum = x;
  while (sum >= 10) {
    x = sum;
    sum = 0;
    while (x > 0) {
      sum += x % 10;
      x /= 10;
    }
  }
  return sum;
}
void p159() {
  std::cout << "159\n";
  size_t MAX_N = 1e6;
  std::vector<size_t> dr(MAX_N + 1, 0);
  std::vector<size_t> drs(MAX_N + 1, 0);
  for (int i = 0; i < 9; i++) {
    dr[i] = i;
  }
  
  for (size_t i = 2; i < MAX_N; i++) {
    dr[i] = DigitalRoot(i);
    drs[i] = dr[i];
  }
  for (size_t x = 2; x < MAX_N; x++) {
    for (size_t y = x; y < MAX_N; y++) {
      size_t prod = x * y;
      if (prod >= MAX_N) {
        break;
      }
      drs[prod] = std::max(drs[prod], drs[x] + drs[y]);
    }
    if (x * 2 >= MAX_N) {
      break;
    }
    
  }
  size_t sum = 0;
  for (size_t i = 0; i < MAX_N; i++) {
    sum += drs[i];
  }
  std::cout << sum << "\n";
}