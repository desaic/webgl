#include "Digits.hpp"
std::vector<uint8_t> base(int n, int b) {
  std::vector<uint8_t> res;
  while (n > 0) {
    res.push_back(n%b);
    n /= b;
  }
  return res;
}

void incrementBase(std::vector<uint8_t> & num,
  int base) {
  for (int i = 0; i < num.size(); i++) {
    num[i] ++;
    if (num[i] >= base) {
      num[i] = 0;
      if (i == num.size() - 1) {
        num.push_back(1);
        break;
      }
    }
    else {
      break;
    }
  }
}