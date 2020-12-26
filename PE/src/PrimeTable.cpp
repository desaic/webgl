#include "PrimeTable.hpp"
#include "PowerMod.hpp"

bool divides(size_t N, std::vector<int> & plist) {
    for (size_t i = 0; i < plist.size(); i++) {
        if (N % plist[i] == 0) {
            return true;
        }
        if (plist[i] * (size_t)plist[i] >= N) {
            break;
        }
    }
    return false;
}

void fillPrimes(size_t N, std::vector<int> & plist) {
    unsigned int p = 3;
    plist.push_back(2);
    for (size_t i = 0; i < 1e8; i++) {
        if (!divides(p, plist)) {
            plist.push_back(p);
        }
        if (p >= N) {
            break;
        }
        p+=2;
    }
    size_t i = 0;
    if (p % 2 == 0) {
        p++;
    }
    for ( ; i < N; i++) {
        if (isPrime(p)) {
            plist.push_back(p);
        }
        if (p >= N) {
            break;
        }
        p+=2;
    }
}

void factorize(size_t num, const std::vector<int> & primes,
  std::vector<int> & factor, std::vector<int> & power)
{
  if (num == 0) {
    return;
  }
  if (num == 1) {
    factor.resize(1, 1);
    power.resize(1, 0);
    return;
  }
  for (size_t i = 0; i < primes.size(); i++) {
    if (num == 0) {
      break;
    }
    int prime = primes[i];
    if (prime*prime > num) {
      break;
    }
    int p = 0;
    while (num > 0 && (num % prime == 0)) {
      num /= prime;
      p++;
    }
    if (p > 0) {
      factor.push_back(prime);
      power.push_back(p);
    }
  }
  if (num > 1) {
    factor.push_back(num);
    power.push_back(1);
  }
}