#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include "ExtEuclid.h"
#include "PrimeTable.hpp"
#include "PowerMod.hpp"

struct Factorization{
  //prime factors
  std::vector<int> f;
  //prime exponent
  std::vector<int> p;
};

void add(Factorization & dst, const Factorization & src) {
    size_t srcIdx = 0;
    std::vector<int> newf, newp;
    int srcfactor = src.f[0];
    if (srcfactor == 1) {
      srcIdx++;
    }
    if (srcIdx >= src.f.size()) {
      return;
    }
    srcfactor = src.f[srcIdx];
    for (size_t j = 0; j < dst.f.size(); j++) {
      int factor = dst.f[j];      
      if (srcIdx < src.f.size()) {
        srcfactor = src.f[srcIdx];
        while (srcfactor <= factor) {
          newf.push_back(srcfactor);
          newp.push_back(src.p[srcIdx]);
          srcIdx++;
          if (srcIdx >= src.f.size()) {
            break;
          }
          srcfactor = src.f[srcIdx];
        } 
      }
      if ( (newf.size()>0) && newf[newf.size()-1] == factor) {
        newp[newp.size() - 1] += dst.p[j];
      }
      else {
        newf.push_back(factor);
        newp.push_back(dst.p[j]);
      }
    }
    while (srcIdx < src.f.size()) {
      newf.push_back(src.f[srcIdx]);
      newp.push_back(src.p[srcIdx]);
      srcIdx++;
    }
    dst.f = newf;
    dst.p = newp;
}

void sub(Factorization & dst, const Factorization & src) {
  size_t srcIdx = 0;
  std::vector<int> newf, newp;
  int dstIdx = 0;
  for (size_t i = 0; i < src.f.size(); i++) {
    int dstf = dst.f[dstIdx];
    int srcf = src.f[i];
    while (dstf < srcf) {
      dstIdx++;
      dstf = dst.f[dstIdx];
    }
    dst.p[dstIdx] -= src.p[i];
  }
  for (size_t i = 0; i < dst.f.size(); i++) {
    if (dst.p[i] > 0) {
      newf.push_back(dst.f[i]);
      newp.push_back(dst.p[i]);
    }
  }
  
  dst.f = newf;
  dst.p = newp;
}

size_t divisorSum(const Factorization & F, size_t m) {
  size_t sum = 1;
  for (size_t i = 0; i < F.f.size(); i++) {
    size_t factor = power(F.f[i], 1 + F.p[i], m);
    factor -= 1;
    size_t inv = size_t(modInverse(F.f[i] - 1, m));
    factor *= inv;
    factor %= m;
    sum = mul_mod(factor, sum, m);
  }
  return sum;
}

void p650() {
  std::cout << "650\n";
  std::vector<int> primes;
  const size_t maxPrime = 200;
  fillPrimes(maxPrime, primes);
  const size_t MAX_NUM = 20000;
  std::vector<Factorization> fa(MAX_NUM + 1);
  for (int i = 1; i <= MAX_NUM; i++) {
    factorize(i, primes, fa[i].f, fa[i].p);
  }

  Factorization dst = fa[15];
  Factorization src = fa[12];
  add(dst, src);
  size_t m = 1000000007L;
  size_t sum = 4;
  std::vector<std::vector<Factorization> > nk(MAX_NUM+1);
  nk[2].resize(2);
  Factorization f;
  f.f.resize(1, 2);
  f.p.resize(1, 1);
  nk[2][1] = f;
  ///////////B(n) = B(n - 1) * n^ { n - 2 }/(n-2)!
  Factorization B0 , Bn;
  Factorization factorial;
  B0.f.resize(1, 2);
  B0.p.resize(1, 1);
  factorial.f.resize(1, 2);
  factorial.p.resize(1, 1);
  Bn = B0;
  for (int n = 3; n <= 20000; n++) {
    if (n % 100 == 0) {
      std::cout << n << "\n";
    }
    Factorization numerator, denominaator;
    numerator = fa[n];
    for (size_t i = 0; i < numerator.p.size(); i++) {
      numerator.p[i] *= n - 1;
    }
    add(Bn, numerator);
    sub(Bn, factorial);
    
    Factorization n2 = fa[n];
    add(factorial, n2);
    
    
    size_t s = divisorSum(Bn, m);
    sum += s;
    sum %= m;

    if (n % 5 == 0) {
      std::cout << n << " " << s << " " <<
        sum << "\n";
    }
    B0 = Bn;

    //nk[n].resize(n);
    ////(n,1)=n
    //nk[n][1] = fa[n];
    ////(n,n-1) = n
    //nk[n][n - 1] = fa[n];
    //for (int k = 2; k < n-1; k++) {
    //  //(n,k) = n/k (n-1, k-1).
    //  nk[n][k] = nk[n - 1][k - 1];
    //  add(nk[n][k], fa[n]);
    //  sub(nk[n][k], fa[k]);
    //}
    //nk[n - 1].clear();
    //nk[n - 1].shrink_to_fit();
  }
  std::cout << sum << "\n";
  //std::vector<Factorization> ffa(MAX_NUM + 1);
  //ffa[1] = fa[1];
  //ffa[2] = fa[2];
  ////factorization of factorials
  //for (int i = 3; i <= MAX_NUM; i++) {
  //  size_t prevIdx = 0;
  //  std::vector<int> newf, newp;
  //  int prevfactor = ffa[i - 1].f[0];

  //  for (size_t j = 0; j < fa[i].f.size(); j++) {
  //    int factor = fa[i].f[j];      
  //    if (prevIdx < ffa[i - 1].f.size()) {
  //      prevfactor = ffa[i - 1].f[prevIdx];
  //      while (prevfactor <= factor) {
  //        newf.push_back(prevfactor);
  //        newp.push_back(ffa[i - 1].p[prevIdx]);
  //        prevIdx++;
  //        if (prevIdx >= ffa[i - 1].f.size()) {
  //          break;
  //        }
  //        prevfactor = ffa[i - 1].f[prevIdx];
  //      } 
  //    }
  //    if ( (newf.size()>0) && newf[newf.size()-1] == factor) {
  //      newp[newp.size() - 1] += fa[i].p[j];
  //    }
  //    else {
  //      newf.push_back(factor);
  //      newp.push_back(fa[i].p[j]);
  //    }
  //  }
  //  while (prevIdx < ffa[i - 1].f.size()) {
  //    newf.push_back(ffa[i - 1].f[prevIdx]);
  //    newp.push_back(ffa[i - 1].p[prevIdx]);
  //    prevIdx++;
  //  }
  //  ffa[i].f = newf;
  //  ffa[i].p = newp;
  //}
}