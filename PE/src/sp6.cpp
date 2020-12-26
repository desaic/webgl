#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

class BigInt {
public:
  BigInt() :d(1, 0) {}
  BigInt(unsigned num) {
    if (num == 0) {
      d.resize(1, 0);
      return;
    }
    while (num >= 10) {
      d.push_back(num % 10);
      num = num / 10;
    }
    if (num > 0) {
      d.push_back(num);
    }
  }

  BigInt operator+ (const BigInt& b) const {
    BigInt sum=*this;
    sum.Add(b);
    return sum;
  }

  ///\param i0 starting index to add b to.
  void Add(const BigInt& b, size_t i0 = 0){
    if (d.size() < (i0 + b.d.size()) ) {
      d.resize(b.d.size() + i0, 0);
    }

    for (size_t i = 0; i < b.d.size(); i++) {
      size_t dst = i + i0;
      d[dst] += b.d[i];
      unsigned char carry = d[dst] / 10;
      d[dst] %= 10;
      size_t j = dst + 1;
      while (carry > 0) {
        if (j >= d.size()) {
          d.push_back(1);
          break;
        }
        d[j] ++;
        if (d[j] >= 10) {
          d[j] -= 10;
        }
        else {
          break;
        }
        j++;
      }
    }
  }

  //not correct implementation. does not handle negative result.
  BigInt operator- (const BigInt& b) const {
    BigInt diff;
    diff.d = d;
    if (diff.d.size() < b.d.size()) {
      diff.d.resize(b.d.size(), 0);
    }

    for (size_t i = 0; i < b.d.size(); i++) {
      unsigned char borrow = 0;
      if (diff.d[i] < b.d[i]) {
        borrow = 1;
        diff.d[i] += (10 - b.d[i]);
      }
      else {
        diff.d[i] -= b.d[i];
      }      
      
      size_t j = i + 1;
      while (borrow > 0) {
        if (j >= diff.d.size()) {
          break;
        }
        if (diff.d[j] == 0) {
          diff.d[j] = 9;
        }else{
          diff.d[j] --;
          break;
        }
        j++;
      }
    }
    diff.trim();
    return diff;
  }

  BigInt MultDigit(unsigned char x)const {
    BigInt prod;
    if (x == 0) {
      prod.d.resize(1, 0);
      return prod;
    }
    prod.d = d;
    for (size_t i = 0; i < d.size(); i++) {
      unsigned digitProd = unsigned(d[i]) * unsigned(x-1);
      BigInt b(digitProd);
      prod.Add(b, i);
    }
    return prod;
  }

  //not correct implementation. does not handle negative result.
  BigInt operator*(const BigInt& b) const {
    BigInt prod;
    
    for (size_t i = 0; i < b.d.size(); i++) {
      BigInt digitProd = MultDigit(b.d[i]);
      prod.Add(digitProd, i);
    }
    return prod;
  }

  //digits
  std::vector<unsigned char> d;
  //trim leading 0s
  void trim() {
    
    if (d.size() <= 1) {
      return;
    }

    size_t i = 0;
    for (; i < d.size(); i++) {
      unsigned char c = d[d.size() - i - 1];
      if (c > 0) {
        break;
      }
    }
    if (i >= d.size()) {
      i = d.size() - 1;
    }
    if (i > 0 ) {
      d.resize(d.size() - i);
    }
  }

  void parse(std::istream& is) {
    d.clear();
    while (1) {
      char c = is.peek();
      if (is.eof()) {
        break;
      }
      if (c >= '0' && c <= '9') {
        is >> c;
        unsigned char val = int(c - '0');
        d.push_back(val);
      }
      else {
        break;
      }
    }
    std::reverse(d.begin(), d.end());
  }

  string ToString() {
    stringstream ss;
    for (size_t i = 0; i < d.size(); i++) {
      ss << int(d[ d.size() - i - 1 ]);
    }
    return ss.str();
  }
};

std::string RepeatChar(char c, size_t count) {
  std::string s;
  s.resize(count, c);
  return s;
}

size_t max(size_t* a, size_t len) {
  if (len == 0) {
    return 0;
  }
  size_t m = a[0];
  for (size_t i = 0; i < len; i++) {
    if (m < a[i]) {
      m = a[i];
    }
  }
  return m;
}

void PrintAdd(BigInt* nums) {
  BigInt sum = nums[0] + nums[1];
  size_t width[3];
  width[0] = nums[0].d.size();
  //add 1 for operator .
  width[1] = nums[1].d.size() + 1;
  width[2] = sum.d.size();
  size_t maxWidth = max(width, 3);
  size_t numSpace = 0;
  numSpace = maxWidth - width[0];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << nums[0].ToString() << "\n";

  numSpace = maxWidth - width[1];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << "+";
  std::cout << nums[1].ToString() << "\n";

  size_t lineWidth = max(&(width[1]), 2);
  numSpace = maxWidth - lineWidth;
  std::cout << RepeatChar(' ', numSpace);
  std::cout << RepeatChar('-', lineWidth) << "\n";

  numSpace = maxWidth - width[2];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << sum.ToString() << "\n";
}

void PrintSub(BigInt* nums) {
  BigInt diff = nums[0] - nums[1];
  size_t width[3];
  width[0] = nums[0].d.size();
  //add 1 for operator .
  width[1] = nums[1].d.size() + 1;
  width[2] = diff.d.size();
  size_t maxWidth = max(width, 3);
  size_t numSpace = 0;
  numSpace = maxWidth - width[0];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << nums[0].ToString() << "\n";

  numSpace = maxWidth - width[1];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << "-";
  std::cout << nums[1].ToString() << "\n";

  size_t lineWidth = max(&(width[1]), 2);
  numSpace = maxWidth - lineWidth;
  std::cout << RepeatChar(' ', numSpace);
  std::cout << RepeatChar('-', lineWidth) << "\n";

  numSpace = maxWidth - width[2];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << diff.ToString() << "\n";
}

void PrintMult(BigInt* nums) {
  BigInt prod = nums[0] * nums[1];
  size_t width[3];
  width[0] = nums[0].d.size();
  //add 1 for operator .
  width[1] = nums[1].d.size() + 1;
  width[2] = prod.d.size();
  size_t maxWidth = max(width, 3);
  size_t numSpace = 0;
  numSpace = maxWidth - width[0];

  numSpace = maxWidth - width[0];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << nums[0].ToString() << "\n";

  numSpace = maxWidth - width[1];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << "*";
  std::cout << nums[1].ToString() << "\n";

  for (size_t i = 0; i < nums[1].d.size(); i++) {
    BigInt digitProd = nums[0].MultDigit(nums[1].d[i]);
    //print a line
    if (i == 0) {
      size_t lineWidth = width[1];
      if (digitProd.d.size() > lineWidth) {
        lineWidth = digitProd.d.size();
      }
      numSpace = maxWidth - lineWidth;
      std::cout << RepeatChar(' ', numSpace);
      std::cout << RepeatChar('-', lineWidth) << "\n";
    }

    numSpace = maxWidth - i - digitProd.d.size();
    std::cout << RepeatChar(' ', numSpace);
    std::cout << digitProd.ToString() << "\n";
    if (nums[1].d.size() == 1) {
      return;
    }

    if (i == nums[1].d.size() - 1) {
      size_t lineWidth = width[2];
      if (digitProd.d.size() + i > lineWidth) {
        lineWidth = digitProd.d.size() + i;
      }
      numSpace = maxWidth - lineWidth;
      std::cout << RepeatChar(' ', numSpace);
      std::cout << RepeatChar('-', lineWidth) << "\n";
    }
  }
  numSpace = maxWidth - width[2];
  std::cout << RepeatChar(' ', numSpace);
  std::cout << prod.ToString() << "\n";
}

int sp6() {
//int main(){
  int numLines;
  std::cin >> numLines;
  std::string line;
  
  for (int l = 0; l < numLines; l++) {
    std::cin >> line;
    std::stringstream ss(line);
    BigInt nums[2];
    nums[0].parse(ss);
    char op;
    ss >> op;
    nums[1].parse(ss);
    if (op == '+') {
      PrintAdd(nums);
    }
    else if (op == '-') {
      PrintSub(nums);
    }
    else {
      PrintMult(nums);
    }
    std::cout << "\n";
  }
  return 0;
}
