#include <iostream>
#include <vector>

std::vector<int> diff(const std::vector<int> & v) {
  std::vector<int> d(v.size() - 1);
  for (size_t i = 0; i < d.size(); i++) {
    d[i] = v[i+1] - v[i];
  }
  return d;
}

struct Poly {
  int order;
  int step;
  std::vector<int> y0; 
  Poly() :order(0), step(0), y0(0) {}
};

bool GetStep(const std::vector<int> & nums, int & step) {
  if (nums.size() <= 1) {
    step = 0;
    return true;
  }
  if (nums.size() == 2) {
    step = nums[1] - nums[0];
    return true;
  }
  int step0 = nums[1] - nums[0];
  for (size_t i = 1; i < nums.size() - 1; i++) {
    int diff = nums[i + 1] - nums[i];
    if (diff != step0) {
      return false;
    }
  }
  step = step0;
}

Poly CalcMinPoly(const std::vector<int>& nums) {
  Poly poly;
  std::vector<int> d = nums;
  while (1) {
    poly.y0.push_back(d[0]);
    if (d.size() <= 1) {
      break;
    }
    poly.order++;
    int step = 0;
    bool equalStep = GetStep(d, step);
    if (equalStep) {
      poly.step = step;
      break;
    }
    d = diff(d);
  }
  return poly;
}

void EvalPoly(const Poly & p, int N, std::vector<int> & v)
{
  if (p.order == 0) {
    v = std::vector<int>(N, p.y0[0]);
    return;
  }
  std::vector<int> d(N, p.step);
  std::vector<int> d1(N, 0);
  for (size_t i = 0; i < p.order; i++) {
    d1[0] = p.y0[p.y0.size() - i - 1];
    for (size_t j = 0; j < d1.size()-1; j++) {
      d1[j+1] = d[j] + d1[j];
    }
    d = d1;
  }
  v = d1;
}

int sp8() {
//int main() {
  int numCases;
  std::cin >> numCases;
  for (int c = 0; c < numCases; c++) {
    int numInput, numOutput;
    std::cin >> numInput >> numOutput;
    std::vector<int> input(numInput);
    for (size_t i = 0; i < input.size(); i++) {
      std::cin >> input[i];
    }
    Poly poly = CalcMinPoly(input);
    //std::cout << poly.order << " " << poly.step << " " << poly.y0[0] << "\n";
    std::vector<int> v;
    EvalPoly(poly, (numInput + numOutput), v);
    for (size_t i = numInput; i < v.size(); i++) {
      std::cout << v[i] << " ";
    }
    std::cout << "\n";
  }
  return 0;
}
