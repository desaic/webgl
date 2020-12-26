#include <iostream>
#include <vector>
#include <algorithm>
void printNum(std::ostream & out, const std::vector<int> & num) {
  for (size_t i = 0; i < num.size(); i++) {
    size_t idx = num.size() - i - 1;
    if (num[idx] >= 0) {
      out << num[idx];
    }
  }
}

size_t rightRot(size_t i) {
  size_t d = i % 10;
  if (d == 0) {
    return i / 10;
  }
  size_t y = i / 10;
  size_t pow = 10;
  while (pow <= y) {
    pow *= 10;
  }
  y += d*pow;
  return y;
}

size_t toLong(const std::vector<int>& num, size_t maxd) {
  maxd = std::min(num.size(), maxd);
  size_t pow = 1;
  size_t y = 0;
  for (size_t i = 0; i < maxd; i++) {
    if (num[i] < 0) {
      break;
    }
    y += num[i] * pow;
    pow *= 10;
  }
  return y;
}

void p168() {
  std::cout << 168 << "\n";
  size_t N = 101; 
  size_t sum = 0;
  for (size_t i = 11; i < 10000000; i++) {
    size_t j = rightRot(i);
    if (i == 410256) {
      std::cout << "debug\n";
    }
    if (j % i == 0) {
      std::cout << j<< "/"<<i<< " = "<<(j/i)<<"\n";
    }
  }
  //loop over value of first digit
  for (int d1 = 1; d1 < 10; d1++) {
    //loop over multipliers
    for (int m1 = 1; m1 < 10; m1++) {
      std::vector<int> num(N, -1);
      num[0] = d1;
      int prevd = d1;
      int prod = d1 * m1;
      int nextd;
      int prevCarry = 0;
      for (size_t pos = 1; pos < num.size() ; pos++) {
        prod = num[pos-1] * m1;
        nextd = (prod + prevCarry) % 10;
        prevCarry = (prod + prevCarry) / 10;
        num[pos] = nextd;
        if (nextd == d1 && prevCarry == 0 && num[pos-1]>0 && pos>1) {
          //if (m1 > 1) {
            printNum(std::cout, num);
            std::cout << " " << m1 << "\n";
            sum += toLong(num, std::min(5, int(pos)));
         // }
          sum %= 100000;
        }
      }
    }
  }
  std::cout << sum << "\n";
}