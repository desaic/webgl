#pragma once
#include<vector>
bool divides(size_t N, std::vector<int> & plist);

void fillPrimes(size_t N, std::vector<int> & plist);

void factorize(size_t num, const std::vector<int> & primes,
  std::vector<int> & factor, std::vector<int> & power);