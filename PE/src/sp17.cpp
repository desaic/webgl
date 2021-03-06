//https://www.spoj.com/problems/CRYPTO1/
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <cstdint>

uint64_t expMod(uint64_t base, uint64_t e, uint64_t p)
{
  uint64_t t = 1L;
  while (e > 0){
    if (e % 2 != 0) {
      t = (t * base) % p;
    }
    base = (base * base) % p;
    e /= 2;
  }
  return t % p;
}

uint64_t FindNonQuadResidue(uint64_t p) {
  uint64_t e = (p - 1) / 2;
  for (uint64_t z = 2; z < p; z++) {
    uint64_t r = expMod(z, e, p);
    if (r == p - 1) {
      return z;
      break;
    }
  }
  return 0;
}

uint64_t power(uint64_t base, uint64_t e)
{
  if (e == 0) {
    return 1;
  }
  uint64_t p = base;
  for (uint64_t i = 0; i < e; i++) {
    p *= base;
  }
  return p;
}

//https://en.wikipedia.org/wiki/Tonelli%E2%80%93Shanks_algorithm
int sp17()
//int main(int argc, char* argv[])
{
  uint64_t n = 0;
  std::cin >> n;
  
  uint64_t p = 4000000007;
  uint64_t Q = p-1;
  uint64_t S = 0;
  while (Q % 2 == 0) {
    Q /= 2;
    S++;
  }
  uint64_t z = FindNonQuadResidue(p);
  uint64_t r = 0;
  uint64_t M = S;
  uint64_t c = expMod(z, Q, p);
  uint64_t t = expMod(n, Q, p);
  uint64_t R = expMod(n, (Q + 1) / 2, p);
  while (t != 0) {
    if (t == 1) {
      r = R;
      break;
    }
    uint64_t t2 = (t * t) % p;
    uint64_t i = 1;
    for (; i < M; i++) {
      if (t2 == 1) {
        break;
      }
      t2 = (t2 * t2) % p;
    }

    uint64_t pow = power(2, M-i-1);

    uint64_t b = expMod(c, pow, p);
    M = i;
    c = (b * b) % p;
    t = (t * c) % p;
    R = (R * b) % p;
  }

  const auto p0 = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>(
    std::chrono::seconds(r));
  std::time_t epoch_time = std::chrono::system_clock::to_time_t(p0);
  
  tm utc_tm = *gmtime(&epoch_time);
  int year = utc_tm.tm_year;
  if (year < 70 || year>130) {
    const auto p1 = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>(
      std::chrono::seconds(p-r));
    epoch_time = std::chrono::system_clock::to_time_t(p1);
  }
  std::cout << std::asctime(gmtime(&epoch_time));
  return 0;
}
