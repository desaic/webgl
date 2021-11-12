#include <iostream>
#include <set>
struct IntPair
{
  int i1;
  int i2;
  IntPair() :i1(0), i2(0) {}
  IntPair(int num1, int num2) :i1(num1), i2(num2){}
};

struct CmpIntPair {
  bool operator ()(const IntPair& a, const IntPair& b) const{
    if (a.i1 < b.i1) {
      return true;
    }
    else if (a.i1 > b.i1) {
      return false;
    }
    return a.i2 < b.i2;
  }
};

int gcd(int a, int b) {
  if (a <= 1) {
    return 1;
  }
  if (b <= 1) {
    return 1;
  }
  if (a < b) {
    int tmp = b;
    b = a;
    a = tmp;
  }
  int r = b;
  while (r > 0) {
    r = a % b;
    a = b;
    b = r;
  }
  return a;
}

void TestGCD()
{
  std::cout << gcd(47, 13) << "\n";
  std::cout << gcd(13, 47) << "\n";
  std::cout << gcd(13, 169) << "\n";
  std::cout << gcd(143, 39) << "\n";
  std::cout << gcd(8, 60) << "\n";
}

int sp25()
{
  int N = 0;
  std::cin >> N;

  for (int i = 0; i < N; i++) {
    int va, vb, target;
    std::cin >> va >> vb >> target;
    std::set<IntPair, CmpIntPair> states;
    int g = gcd(va, vb);
    if (target % g != 0) {
      std::cout << "-1\n";
      continue;
    }
    if (target > va && target > vb) {
      std::cout << "-1\n";
      continue;
    }
    if (vb > va) {
      int tmp = va;
      va = vb;
      vb = tmp;
    }
  }
  return 0;
}