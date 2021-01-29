#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <queue>
#include <fstream>

template <typename T>
class Matrix {
public:

  Matrix() :s({ 0,0 }) {}

  T& operator()(size_t row, size_t col) {
    return v[row * s[1] + col];
  }

  const T& operator()(size_t row, size_t col) const {
    return v[row * s[1] + col];
  }

  void Allocate(size_t rows, size_t cols) {
    s[0] = rows;
    s[1] = cols;
    v.resize(rows * cols);
  }

  void Fill(T val) {
    std::fill(v.begin(), v.end(), val);
  }

  //rows and cols.
  std::array<size_t, 2> s;
  std::vector <T> v;
  std::string ToString() const {
    std::ostringstream oss;
    for (size_t row = 0; row < s[0]; row++) {
      for (size_t col = 0; col < s[1]; col++) {
        oss << int((*this)(row, col)) << " ";
      }
      oss << "\n";
    }
    return oss.str();
  }
};

typedef Matrix<unsigned short> Matrixu16;
typedef Matrix<unsigned char> Matrixu8;
typedef Matrix<unsigned int> Matrixu32;
typedef Matrix<int> Matrixi32;
typedef std::array<int, 2> Vec2i;

class Node
{
public:
  Node() {
    idx[0] = 0;
    idx[1] = 0;
  }
  Node(int row, int col) {
    idx[0] = row;
    idx[1] = col;
  }
  Node(const std::array<int, 2>&i) {
    idx = i;
  }

  //row then col
  std::array<int,2> idx;

  bool operator==(const Node& b)const {
    return (idx[0] == b.idx[0])&&(idx[1] == b.idx[1]);
  }
};

bool inBound(int x, int lb, int ub) {
  return x >= lb && x < ub;
}

class Graph {
public:

  virtual std::vector<Node> GetNbrs(const Node& N) const {
    std::vector<Node> nbrs;
    const unsigned NUM_NBR = 4;
    int nbrOffset[NUM_NBR][2] = { { -1,0 },{1,0},{0,-1},{0,1} };
    for (unsigned n = 0; n < NUM_NBR; n++) {
      std::array<int, 2> nbrIdx = N.idx;
      nbrIdx[0] += nbrOffset[n][0];
      nbrIdx[1] += nbrOffset[n][1];
      if (!inBound(nbrIdx[0], 0, int(h.s[0]))) {
        continue;
      }
      if (!inBound(nbrIdx[1], 0, int(h.s[1]))) {
        continue;
      }
      Node nbr(nbrIdx);
      if (!HasEdge(N, nbr)) {
        continue;
      }
      nbrs.push_back(nbr);
    }
    return nbrs;
  }

  virtual bool HasEdge(const Node& src, const Node& dst) const {
    int ha = h(src.idx[0], src.idx[1]);
    int hb = h(dst.idx[0], dst.idx[1]);
    return (ha+1>=hb) && (ha-3<=hb);
  }
    
  Matrixu16 h;
};

int linearIdx(int row, int col, int cols) {
  return row * cols + col;
}
std::array<int,2> gridIdx(int linIdx, int cols) {
  return { linIdx / cols, linIdx % cols };
}

int64_t GCD(int64_t a, int64_t b) {
  int64_t r = a % b;
  while (r > 0) {
    a = b;
    b = r;
    r = a % b;
  }
  return b;
}

class Fraction {
public:
  long long d, n;
  int sign;
  Fraction() :d(1), n(0),
    sign(1) {}
  Fraction(long numerator, long denominator) :
    d(denominator), n(numerator), sign(1) {
    reduce();
  }
  void reduce() {
    if (n == 0) {
      d = 1;
      return;
    }
    long long g = GCD(d, n);
    d /= g;
    n /= g;
  }

  bool operator<(const Fraction& rhs) const {
    if (sign != rhs.sign) {
      return sign < rhs.sign;
    }
    return n * rhs.d < d * rhs.n;
  }

  bool operator>(const Fraction& rhs) const {
    return rhs<(*this);
  }

  bool operator==(const Fraction& rhs) const {
    if (sign != rhs.sign) {
      return false;
    }
    return (n == rhs.n) && (d == rhs.d);
  }
};

Fraction operator+(const Fraction& a, const Fraction& b) {
  Fraction c;
  c.d = a.d * b.d;
  c.n = a.sign * a.n * b.d + b.sign * a.d * b.n;
  if (c.n < 0) {
    c.n = -c.n;
    c.sign = -1;
  }
  c.reduce();
  return c;
}

std::vector<std::array<int, 3> > RayVoxel(const std::array<int, 3>& src, const std::array<int, 3>& dst)
{
  std::vector<std::array<int, 3> > line;
  std::array<int, 3> ptA = src;
  std::array<int, 3> ptB = dst;
  if (src == dst) {
    line.push_back(src);
    return line;
  }
  int step[3] = { 1,1,1 };
  int dx[3];
  
  for (int axis = 0; axis < 3; axis++) {
    dx[axis] = 2*(dst[axis] - src[axis]);
    if (dx[axis] < 0) {
      step[axis] = -1;
      dx[axis] = -dx[axis];
    }
  }
  
  Fraction tMax[3] = { Fraction(1,dx[0]), Fraction(1,dx[1]), Fraction(1,dx[2]) };
  Fraction tDelta[3] = { Fraction(2,dx[0]), Fraction(2,dx[1]), Fraction(2,dx[2]) };
  Fraction MAX_DELTA_T(5000, 1);
  for (int a = 0; a < 3; a++) {
    if (dx[a] == 0) {
      tMax[a] = MAX_DELTA_T;
      tDelta[a] = MAX_DELTA_T;
    }
  }
  line.push_back(src);
  while (ptA != ptB) {
    if (tMax[0] < tMax[1]) {
      if (tMax[0] < tMax[2]) {
        ptA[0] += step[0];
        tMax[0] = tMax[0] + tDelta[0];
      }
      else if (tMax[0] > tMax[2]) {
        ptA[2] += step[2];
        tMax[2] = tMax[2] + tDelta[2];
      }
      //==
      else {
        ptA[0] += step[0];
        tMax[0] = tMax[0] + tDelta[0];
        ptA[2] += step[2];
        tMax[2] = tMax[2] + tDelta[2];
      }
    }
    else if (tMax[1] < tMax[0]) {
      if (tMax[1] < tMax[2]) {
        ptA[1] += step[1];
        tMax[1] = tMax[1] + tDelta[1];
      }
      else if (tMax[1] > tMax[2]) {
        ptA[2] += step[2];
        tMax[2] = tMax[2] + tDelta[2];
      }
      //==
      else {
        ptA[1] += step[1];
        tMax[1] = tMax[1] + tDelta[1];
        ptA[2] += step[2];
        tMax[2] = tMax[2] + tDelta[2];
      }
    }
    //==
    else {
      if (tMax[0] < tMax[2]) {
        ptA[0] += step[0];
        tMax[0] = tMax[0] + tDelta[0];
        ptA[1] += step[1];
        tMax[1] = tMax[1] + tDelta[1];
      }
      else if (tMax[0] > tMax[2]) {
        ptA[2] += step[2];
        tMax[2] = tMax[2] + tDelta[2];
      }
      //==
      else {
        for (unsigned int a = 0; a < 3; a++) {
          ptA[a] += step[a];
          tMax[a] = tMax[a] + tDelta[a];
        }
      }
    }
    line.push_back(ptA);
  }
  return line;
}

bool visible(const Matrixu16& h, const Node& src, const Node& dst)
{
  std::array<int, 3>pointA, pointB;
  pointA = { src.idx[0], src.idx[1], h(src.idx[0], src.idx[1]) + 1 };
  pointB = { dst.idx[0], dst.idx[1], h(dst.idx[0], dst.idx[1]) + 1 };
  std::vector<std::array<int, 3> > line = RayVoxel(pointA, pointB);
  for (size_t i = 0; i < line.size(); i++) {
    std::array<int, 3> pt = line[i];
    if (h(pt[0], pt[1]) >= pt[2]) {
      return false;
    }
  }
  return true;
}

bool IsNodeVisible(const Matrixu16& h, const Node& n, const std::vector<Node>& lights)
{
  bool valid = false;
  for (unsigned l = 0; l < lights.size(); l++) {
    if (visible(h, lights[l], n)) {
      valid = true;
      break;
    }
  }
  return valid;
}

int
dijkstra(const Graph& G, const Node& src, const Node& dst)
{
  std::vector<Node> path;
  std::vector<Node> lights(2);
  lights[0] = src;
  lights[1] = dst;
  //shortest distances
  //-1 for uninitialized.
  Matrixi32 sd;
  Matrixu8 visited;
  Matrixi32 prev;
  sd.Allocate(G.h.s[0], G.h.s[1]);
  sd.Fill(-1);
  visited.Allocate(sd.s[0], sd.s[1]);
  prev.Allocate(sd.s[0], sd.s[1]);
  prev.Fill(-1);
  visited.Fill(0);

  sd(src.idx[0], src.idx[1]) = 0;
  visited(src.idx[0], src.idx[1]) = 1;
  std::queue<Node> q;
  q.push(src);

  while (!q.empty()) {
    Node node = q.front();
    q.pop();
    std::vector<Node> nbrs = G.GetNbrs(node);
    for (unsigned i = 0; i < nbrs.size(); i++) {
      Node nbr = nbrs[i];
      if (visited(nbr.idx[0], nbr.idx[1])) {
        continue;
      }
      visited(nbr.idx[0], nbr.idx[1]) = 1;
      if (!IsNodeVisible(G.h, nbr, lights)) {
        continue;
      }
      q.push(nbr);
      int oldDist = sd(nbr.idx[0], nbr.idx[1]);
      int newDist = sd(node.idx[0], node.idx[1]) + 1;
      if (oldDist<0 || newDist < oldDist) {
        sd(nbr.idx[0], nbr.idx[1]) = newDist;
        prev(nbr.idx[0], nbr.idx[1]) = linearIdx(node.idx[0], node.idx[1], sd.s[1]);
      }
    }
  }
  int len = sd(dst.idx[0], dst.idx[1]); 
  //construct path for debuging.
  path.resize(size_t(len+1));
  Node node=dst;
  for (int i = 0; i < len; i++) {
    path[len - i] = node;
    int prevIdx = prev(node.idx[0], node.idx[1]);
    std::array<int, 2>nodeIdx = gridIdx(prevIdx, prev.s[1]);
    node = Node(nodeIdx);
    path.push_back(node);
  }
  return len;
}

//int sp9() {
int main() {
  int numCases;
  std::cin >> numCases;
  for (int c = 0; c < numCases; c++) {
    int rows, cols;
    std::cin >> rows >> cols;
    Matrixu16 h;
    h.Allocate(rows, cols);
    for (int row = 0; row < rows; row++) {
      for (int col = 0; col < cols; col++) {
        std::cin >> h(row, col);
      }
    }
    Vec2i p0, p1;
    std::cin >> p0[0] >> p0[1];
    std::cin >> p1[0] >> p1[1];
    Graph G;
    G.h = h;
    std::vector<Node>lights(2);
    lights[0] = Node(p0[0]-1, p0[1] - 1);
    lights[1] = Node(p1[0] - 1, p1[1] - 1);
    int len = dijkstra(G, lights[0], lights[1]);
    if (len < 0) {
      std::cout << "Mission impossible!\n";
    }
    else {
      std::cout << "The shortest path is " << len << " steps long.\n";
    }
  }
  return 0;
}
