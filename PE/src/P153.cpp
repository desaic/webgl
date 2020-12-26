#include<iostream>
#include "bigInt.h"
#include "ExtEuclid.h"
void p153() {
  int maxNum = 1e8;
 // BigInt::Rossi sum(0);
  long long sum = 0;

  for (long long re = 1; re <= maxNum; re++) {
    sum = sum + re*(maxNum/re);
    for (long long im = 1; im <= re; im++) {
      long long norm = re * re + im * im;
      if (norm > maxNum) {
        break;
      }
      long long g = GCD(re, im);
      if (g > 1) {
        continue;
      }
      long long fracSum = 0;
      long long mult = maxNum / norm;
      for (long long k = 1; k <= mult; k++) {
        fracSum += k * (mult / k);
      }
      long long total = 2*fracSum*re;
      if (im<re) {
        total += 2*fracSum*im;
      }
      sum = sum + total;
    }
  }
  //std::cout << sum.toStrDec() << "\n";
  std::cout << sum << "\n";
}