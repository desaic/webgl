#include "Fraction.h"
#include <iostream>
#include <vector>
#include <set>
void p155() {
  std::cout << "155\n";
  std::vector<std::set<Fraction> >combs;
  Fraction c0(60, 1);
  std::set<Fraction> s0;
  s0.insert(c0);
  
  const int nC = 18;
  combs.resize(nC+1);
  combs[1] = s0;
  size_t cnt = 1;
  std::set<Fraction> fullSet;
  fullSet.insert(c0);
  for (int i = 2; i <= nC; i++) {
    std::cout << i << "================\n\n";
    for (int j = 1; j <= i/2; j++) {
      int k = i - j;
      for (auto it = combs[j].begin(); it != combs[j].end(); it++) {
        Fraction fj = *it;
        for (auto itk = combs[k].begin(); itk != combs[k].end(); itk++) {
          Fraction fk = *itk;
          Fraction sum1 = fj + fk;
          sum1.reduce();
          combs[i].insert(sum1);

          Fraction sum2(fj.n*fk.n, fj.n*fk.d + fk.n*fj.d);
          combs[i].insert(sum2);

          fullSet.insert(sum1);
          fullSet.insert(sum2);
        }
      }
    }
  
    cnt += combs[i].size();
    std::cout << i << " " << cnt << "\n";
  }
  int idx = 0;
  
  for (auto it = combs[nC].begin(); it != combs[nC].end(); it++) {
    if (idx % 100 == 0) {
    //  std::cout << *it << "\n";
    }
    idx++;
  }
  std::cout << fullSet.size() << "\n";
}