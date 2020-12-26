#include <iostream>
#include "PowerMod.hpp"
#include "ExtEuclid.h"
size_t countFactor(size_t n, int f) {
  size_t count = 0;
  size_t p = f;
  while (n > p) {
    count += n / p;
    p *= f;
  }
  return count;
}

//N!/base^x mod base^basePow
size_t factDivMod(size_t N, int base, int basePow)
{
  size_t f = 0;
  int maxPow = 0;
  size_t x = N;
  while (x >= base) {
    x /= base;
    maxPow++;
  }
  size_t bp = base;
  for (int i = 1; i < basePow; i++) {
    bp *= base;
  }
  size_t km = N;
  size_t a = 1;
  for (size_t i = 2; i < bp; i++) {
    if (i % 5 == 0) {
      continue;
    }
    a *= i;
    a %= bp;
  }
  f = 1;
  for (int m = 0; m <= maxPow; m++) {
    size_t q = km / bp;
    size_t r = km % bp;
    size_t fm = power(a, q, bp);
    for (size_t i = 1; i <= r; i++) {
      if (i % 5 == 0) {
        continue;
      }
      fm *= i;
      fm %= bp;
    }
    km /= base;
    f *= fm;
    f %= bp;
  }
  return f;
}

void p160() {
  size_t MAX_N = 1000000000000L;
  size_t factMod5 = factDivMod(MAX_N, 5, 5);
  size_t factor5Count = countFactor(MAX_N, 5);
  int bp = 3125;
  size_t powerOf2 = power(2, factor5Count, bp);
  long long a, b;
  extEuclid(powerOf2, 3125, a, b);
  factMod5 *= a;
  factMod5 %= bp;

  extEuclid(32, bp, a, b);
  size_t x = (factMod5 * a * 32) % (32 * bp);
  //16576
  std::cout << x << "\n";
}