#include <iostream>
#include <vector>
#include <deque>
#include <set>
#include <thread>

struct Vec2i {
  int64_t v[2];
  Vec2i(int64_t x, int64_t y) {
    v[0] = x;
    v[1] = y;
  }
  Vec2i(const Vec2i & b) {
    v[0] = b.v[0];
    v[1] = b.v[1];
  }

  int64_t & operator[](size_t i) {
    return v[i];
  }

  /// Allow indexed access to underlying const values.
  const int64_t & operator[](size_t i) const {
    return v[i];
  }

  Vec2i operator+(const Vec2i & b) {
    return Vec2i(v[0] + b[0], v[1] + b[1]);
  }

  bool operator==(const Vec2i &b) const {
    return v[0] == b[0] && v[1] == b[1];
  }

  bool operator<(const Vec2i& b) const
  {
    if (v[0] < b[0]) {
      return true;
    }
    if (v[0] == b[0]) {
      return v[1] < b[1];
    }
    return false;
  }
};

void reduce(Vec2i & v, int basea, int baseb) {
  int64_t q = v.v[1] / 2;
  v.v[0] += q * baseb;
  v.v[1] = v.v[1] % 2;
}

int64_t eval(Vec2i & v, int basea, int baseb) {
  int64_t val = v[0] * basea + v[1] * baseb;
  return val;
}

std::vector<int64_t> naiveUlam(int basea, int baseb, int64_t maxK) {
  std::vector< std::vector<Vec2i> > comps;
  comps.resize(maxK * 2);
  std::vector<int64_t> nums(2, 0);
  nums[0] = basea;
  nums[1] = baseb;
  comps[basea].push_back(Vec2i(basea, 0));
  comps[baseb].push_back(Vec2i(0, baseb));
  comps[basea + baseb].push_back(Vec2i(basea, baseb));
  for (int64_t k = baseb + 1; k < maxK; k++) {
    if (comps[k].size() == 1) {
      int64_t newNum = k;
      if (comps[k][0][0] == basea || comps[k][0][0] == 2 + 2 * baseb
        || comps[k][0][1] == basea || comps[k][0][1] == 2 + 2 * baseb) {

      }
      else {
        std::cout << newNum << "\n";
      }
      for (size_t j = 0; j < nums.size(); j++) {
        int64_t sum = nums[j] + k;
        if (comps.size() <= sum) {
          comps.resize(sum);
        }
        comps[sum].push_back(Vec2i(nums[j], k));
      }
      nums.push_back(newNum);
    }
  }
  return nums;
}

struct CircBuf {
  CircBuf():buf(nullptr), bufsize(0){}
  CircBuf(size_t size) {
    alloc(size);
  }
  void alloc(size_t size) {
    if (buf == nullptr) {
      delete[] buf;
    }
    buf = new int64_t[size];
    head = 0;
    tail = 0;
    bufsize = size;
  }
  int64_t & operator[](size_t idx) {
    return buf[(idx + head) % bufsize];
  }
  int64_t operator[](size_t idx)const {
    return buf[(idx + head) % bufsize];
  }

  inline void push_back(int64_t num) {
    buf[tail] = num;
    tail = (tail == bufsize-1)?0:(tail+1);
  }
  inline void pop_front() {
    head = (head == bufsize - 1)?0:head+1;
  }
  inline int64_t front() {
    return buf[head];
  }
  inline int64_t back() {
    return tail == 0 ? (buf[bufsize - 1]) : buf[tail - 1];
  }
  inline size_t size() {
    if (tail >= head) {
      return tail - head;
    }
    return tail + bufsize - head;
  }
  ~CircBuf() {
    if (buf != nullptr) {
      delete[]buf;
    }
    buf = nullptr;
  }
  int64_t * buf;
  size_t head, tail, bufsize;

};

std::vector<int64_t> results;

void p167Thread(int N) {
  //component of a number
  int basea = 2;
  int baseb = 2*N+1;
  int64_t maxK = 1e11;
  int64_t testK = 10000;
  
  std::vector<int64_t> nums(2, 0);
    
  nums = naiveUlam(basea, baseb, testK);

  std::vector<int64_t> basecase;
  int64_t specialNum = 2 * baseb + basea;
  basecase = naiveUlam(basea, baseb, 3*specialNum );
  int baseCaseK = 100;

  CircBuf oldnum(2*basecase.size());
  for (size_t i = 0; i < basecase.size(); i++) {
    oldnum.push_back(basecase[i]);
  }
  int64_t maxnum = oldnum.back();
  int64_t minnum = oldnum[0];
  while (minnum + specialNum <= maxnum) {
    oldnum.pop_front();
    minnum = oldnum[0];
  }
  maxK -= basecase.size();
  for (int64_t k = 0; k < maxK; k++) {
    if (k % 1000000000 == 0) {
  //   std::cout <<N<<" "<< k << "\n";
    }
    int64_t maxnum = oldnum.back();
    int64_t cand = maxnum + basea;
    int64_t comp = cand - specialNum;
    bool found = false;
    while(true) {
      minnum = oldnum.buf[oldnum.head];
      if (minnum == comp) {
        found = true;
        break;
      }
      if (minnum > comp) {
        break;
      }
      oldnum.pop_front();
    }
    if (found) {
      oldnum.pop_front();
      oldnum.push_back(oldnum.buf[oldnum.head] + specialNum);
    }
    else {
      oldnum.push_back(maxnum + basea);
    }
    
  }
  std::cout <<nums[999]<<" " << oldnum.back()<<"\n";
  if (results.size() <= N) {
    results.resize(N + 1);
  }
  results[N] = oldnum.back();
}

void p167() {
  int64_t sum = 0;
  std::vector<std::thread> threads(9);
  results.resize(11, 0);
 // p167Thread( 3);
  for (int N = 2; N <= 6; N++) {
    threads[N - 2] = std::thread(p167Thread, N);
  }

  for (int N = 2; N <= 6; N++) {
    threads[N - 2].join();
  }

  for (int N = 7; N <= 10; N++) {
    threads[N - 2] = std::thread(p167Thread, N);
  }

  for (int N = 7; N <= 10; N++) {
    threads[N - 2].join();
  }

  for (int N = 2; N <= 10; N++) {
    sum += results[N];
  }
  std::cout << sum << "\n";
}