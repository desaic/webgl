#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include "uint128_t.h"

size_t bruteForce(int nDigit) {
  size_t cnt = 0;
  size_t maxNum = std::pow(16, nDigit);
  for (size_t i = 1; i < maxNum; i++) {
    std::stringstream stream;
    stream << std::hex << i;
    std::string result(stream.str());
    if (result.find('a') != std::string::npos && result.find('0') != std::string::npos 
      && result.find('1') != std::string::npos) {
      cnt++;
    }
  }
  return cnt;
}


uint128_t intpow(uint64_t base, int p) {
  uint128_t prod = 1;
  for (int i = 0; i < p; i++) {
    prod *= base;
  }
  return prod;
}

void p162() {
  int maxNDigit = 16;
  //n = 4 , cnt = 
  uint128_t count = 0;
  size_t test = bruteForce(6);
  std::cout << "test " << test << "\n";
  std::vector<std::vector< size_t > > nCk(maxNDigit + 1);
  nCk[0].resize(1, 1);
  nCk[1].resize(2, 1);
  nCk[2].resize(4, 1);
  nCk[2][1] = 2;
  for (size_t n = 3; n < nCk.size(); n++) {
    nCk[n].resize(n + 1);
    nCk[n][0] = 1;
    nCk[n][1] = n;
    nCk[n][n] = 1;
    nCk[n][n - 1] = n;
    for (size_t k = 2; k < n - 1; k++) {
      nCk[n][k] = nCk[n - 1][k] + nCk[n - 1][k - 1];
    }
  }
  //0 1 A
  for (int nDigit = 3; nDigit <= maxNDigit; nDigit++) {
    //first digit is 1 or A.
    //need at least one 0 and one A
    uint128_t localCnt = 0;
    for (int otherDigit = 0; otherDigit <= nDigit - 3; otherDigit++) {
      uint128_t mult = intpow(14, otherDigit) * nCk[nDigit - 1][otherDigit];
      localCnt += mult * (intpow(2, nDigit - 1 - otherDigit) - 2);
    }
    localCnt *= 2;
    //first digit is some other digit.
    //1. exclusively 0 ,1, and A
    //2. at least one 0,1, A and one other digit
    if (nDigit > 3) {
      for (int otherDigit = 0; otherDigit <= nDigit - 4; otherDigit++) {
        uint128_t mult = intpow(13, otherDigit + 1) * nCk[nDigit - 1][otherDigit];
        int remainDigit = nDigit - 1 - otherDigit;

        //at least 1 0,1,A in remaining digits
        uint128_t sum1 = 0;
        for (int numZero = 1; numZero <= remainDigit - 2; numZero++) {
          sum1 += nCk[remainDigit][numZero] * (intpow(2, remainDigit - numZero) - 2);
        }

        localCnt += sum1 * mult;
      }
    }

    count += localCnt;
  }
  std::cout << count << "\n";
}
