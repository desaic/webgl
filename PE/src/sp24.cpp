#include <string>
#include <algorithm>
#include <iostream>
struct BigInteger {
  BigInteger(int v) { val = std::to_string(v); }
  size_t size()const { return val.size(); }
  void addDigit(int d, int i);
  BigInteger mul(BigInteger b);
  void rmLeadingZero();
  std::string val;
};

void BigInteger::addDigit(int d, int i) {
  if (i >= size()) {
    return;
  }
  i = size() - i - 1;
  int d0 = val[i] - '0';
  d0 += d;
  int newDigit = d0 % 10;
  int carry = (d0 > 9);
  val[i] = newDigit + '0';
  i--;
  while (carry > 0 && i >= 0) {
    d0 = val[i] - '0';
    d0 += carry;
    if (d0 > 9) {
      carry = 1;
      d0 -= 10;
    }
    else {
      carry = 0;
    }
    val[i] = d0 + '0';
    i--;
  }
}

void BigInteger::rmLeadingZero() {
  size_t i = 0;
  for (; i < val.size(); i++) {
    if (val[i] != '0') {
      break;
    }
  }
  if (i == val.size()) {
    return;
  }
  val = val.substr(i);
}

BigInteger BigInteger::mul(BigInteger b) {
  BigInteger p(0);
  int maxDigits = size() + b.size();
  p.val.resize(maxDigits, '0');
  for (int i = 0; i < b.size(); i++) {
    if (b.val[i] == 0) {
      continue;
    }
    char db = b.val[i] - '0';
    for (int j = 0; j < size(); j++) {
      if (val[i] == 0) {
        continue;
      }
      char da = val[j] - '0';
      char ab = da * db;
      char d0 = ab % 10;
      int digitIdx = (b.size() - i - 1) + (size() - j - 1);
      p.addDigit(d0, digitIdx);
      if (ab > 0) {
        p.addDigit(ab / 10, digitIdx + 1);
      }
    }
  }
  p.rmLeadingZero();
  return p;
}

int sp24()
{
  int N = 0;
  std::cin >> N;

  for (int i = 0; i < N; i++) {
    int num = 0;
    std::cin >> num;
    BigInteger prod(1);
    for (int j = 2; j <= num; j++) {
      prod = prod.mul(j);
    }
    std::cout << prod.val << "\n";
  }
  return 0;
}