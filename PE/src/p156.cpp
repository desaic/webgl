#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include "Digits.hpp"

size_t countDigit(size_t num, uint8_t d) {
  size_t prefix = num;
  size_t suffix = 0;
  size_t p = 1;
  size_t count = 0;
  while (prefix > 0) {
    uint8_t r = num % 10;
    prefix -= p * r;

    num /= 10;
    if (r == d) {
      count += (1+suffix);
    }
    else if (r > d) {
      count += p;
    }
    count += prefix / 10;
    suffix += p * r;
    p *= 10;
    
  }
  return count;
}

size_t tooManyDigits(size_t num, uint8_t d) {
  //find highest digit = d
  size_t p = 1;
  uint8_t carry = 0;
  size_t newNum = 0;
    while (num > 0) {
      uint8_t digit = num % 10;
      if (digit == d) {
        newNum = (num + 1)*p;
      }
      num /= 10;
      p *= 10;
    }
  return newNum;
}

size_t tooFewDigits(size_t num, uint8_t  d) {

  return 0;
}
//next increasing or decreasing interval
int nd(size_t num, uint8_t d) {
  int cnt = 0;
  while (num > 0) {
    size_t r = num % 10;
    if (r == d) {
      cnt++;
    }
    num /= 10;
  }
  return cnt;
}
void p156() {
  std::cout << "156\n";
  const size_t maxN = 1e10;
  std::cout << countDigit(9e10+1e9, 9)<<"\n";
  size_t s = 0;
  //base case table
  const int baseCaseSize = 100000;
  //std::vector<std::vector<size_t > > baseCase(10);
  std::vector<std::map<int, std::vector<int> > > distMap[10];
  int maxDiff = 0;
  for (uint8_t d = 1; d <= 9; d++) {
    std::cout << int(d) << "\n";
    distMap[d].resize(11);
    for (size_t i = 0; i < baseCaseSize; i++) {
      int c = countDigit(i, d);
      if (i == 99980) {
        std::cout << "debug \n";
      }
      int dist = int(c) - int(i);
      maxDiff = std::max(maxDiff, 10*std::abs(dist));
      for (int j = 0; j <= 10; j++) {
        auto it = distMap[d][j].find(dist+j*i);
        if (it == distMap[d][j].end()) {
          distMap[d][j][dist + j * i]= std::vector<int>(1, i);
        }
        else {
          distMap[d][j][dist + j * i].push_back(i);
        } 
      }
    }
  }
  size_t sum = 0;
  for (int d = 1; d <= 9; d++) {
    std::cout << int(d) << "\n";
    size_t maxNum = maxN * d;
    size_t f = 0;
    for (size_t i = 0; i < maxNum; i += baseCaseSize) {
      size_t c = countDigit(i, d);
      int n = nd(i, d);
      int64_t diff = int64_t(i) - int64_t(c);
      if ( std::abs(diff) > maxDiff) {
        continue;
      }
      if (n > 1 && (diff%n != 0)) {
        continue;
      }
      const auto it = distMap[d][n].find(int(diff));
      if (it == distMap[d][n].end()) {
        continue;
      }
      const auto & s = it->second;
      for (size_t j = 0; j < s.size(); j++) {
        size_t num = i + s[j];
        std::cout << num<<" "<<countDigit(num, d)<<"\n";
        sum += num;
      }
    }
  }
  std::cout << sum << "\n";
}
