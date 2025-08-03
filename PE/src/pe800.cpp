#include<iostream>
#include <vector>
#include <cmath>
struct PrimeTable {

  PrimeTable(size_t maxP) {
    p.push_back(2);
    for (size_t i = 3; i < maxP; i+=2) {
      if (IsPrime(i)) {
        p.push_back(i);
      }
    }
  }

  bool IsPrime(size_t num) {
    for (size_t i = 0; i < p.size();i++) {
      size_t pi = p[i];
      if (pi * pi > num) {
        break;
      }
      if (num % pi == 0) {
        return false;
      }
    }
    return true;
  }
  std::vector<size_t>p;
};

//index of largest number below or equal target
//-1 if there is not such number
int64_t binary_search(std::vector<size_t>& vec, size_t target) {
  int64_t low = 0;
  int64_t high = vec.size() - 1;
  if (vec.size() == 0){
    return -1;
  }
  if (vec[0] > target) {
    return -1;
  }
  while (low < high - 1) {
    int64_t mid = (low + high) / 2;
    if (vec[mid] == target) {
      return mid;
    }
    if (vec[mid] > target) {
      high = mid;
    }
    else {
      low = mid;
    }
  }
  if (target >= vec[high]) {
    return high;
  }
  return low;
}

void pe800() {  
  const size_t MAX_P = 800800 * 20; 
  PrimeTable pt(MAX_P);
  //for (size_t i = 0; i < 100; i++) {
  //  int64_t idx = binary_search(pt.p, i);
  //  if (idx >= 0) {
  //    std::cout << i << " " << idx << " " << pt.p[idx] << "\n";
  //  }
  //}
  double logMax = 800800 * std::log(800800);
  size_t sum = 0;
  for (size_t i = 0; i < pt.p.size()-1;i++) {
    size_t p1 = pt.p[i];
    double logp1 = std::log(p1);
    size_t maxp2 = size_t(logMax / logp1)+1;
    if (maxp2 < 2) {
      break;
    }
    //std::cout << maxp2 << "\n";
    int64_t startj = binary_search(pt.p, maxp2);
    if (startj < 0) {
      continue;
    }
    
    for (int64_t j = startj; j > i; j--) {
      size_t p2 = pt.p[j];
      double logVal = logp1 * p2 + std::log(p2) * p1;
      if (logVal <= logMax) {
        sum += (j - i);
        break;
      }
    }
  }
  std::cout << "pe 800\n";
  std::cout << sum << "\n";
}
