#include <iostream>
#include <deque>
#include <set>
#include <vector>

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

void AddState(int va, int vb, std::set<IntPair, CmpIntPair>& visited,
  std::deque<IntPair> & queue)
{
  IntPair s(va, vb);
  if (visited.find(s) != visited.end()) {
    return;
  }
  visited.insert(s);
  queue.push_back(s);
}

int pour(int va, int vb, int target)
{
  if (va < vb) {
    int tmp = va;
    va = vb;
    vb = tmp;
  }
  if (target > va) {
    return -1;
  }
  if (target == va) {
    return 1;
  }
  if (va == vb) {
    if (va == target) {
      return 1;
    }
    else {
      return -1;
    }
  }

  int g = gcd(va, vb);
  if (target % g != 0) {
    return -1;
  }
  std::set<IntPair, CmpIntPair> visited;
  std::deque<IntPair> queue;
  queue.push_back( IntPair(0, 0));
  visited.insert(IntPair(0, 0));
  int step = 0;
  while (!queue.empty()) {
    std::deque<IntPair> nextqueue;
    for (size_t i = 0; i < queue.size(); i++) {
      IntPair state = queue[i];
      visited.insert(state);
      if (state.i1 == target || state.i2 == target) {
        return step;
      }
      if (state.i1 > 0) {
        AddState(0, state.i2, visited, nextqueue);
        //pour a into b until a is empty or b is full
        if (state.i2 < vb) {
          int capacity = vb - state.i2;
          int diff = capacity;
          if (capacity > state.i1) {
            diff = state.i1;
          }
          AddState(state.i1 - diff, state.i2 + diff, visited, nextqueue);
        }
      }
      if (state.i2 > 0) {
        AddState(state.i1, 0, visited, nextqueue);
        if (state.i1 < va) {
          int capacity = va - state.i1;
          int diff = capacity;
          if (capacity > state.i2) {
            diff = state.i2;
          }
          AddState(state.i1 + diff, state.i2 - diff, visited, nextqueue);
        }
      }
      if (state.i1 < va) {
        AddState(va, state.i2, visited, nextqueue);
      }
      if (state.i2 < vb) {
        AddState(state.i1, vb, visited, nextqueue);
      }
    }
    queue = nextqueue;
    step++;
  }
  return -1;
}

int sp25()
{
  int N = 0;
  std::cin >> N;

  for (int i = 0; i < N; i++) {
    int va, vb, target;
    std::cin >> va >> vb >> target;
    int p = pour(va, vb, target);
    std::cout << p<<"\n";
  }
  return 0;
}