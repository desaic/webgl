#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <queue>

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
  bool operator<(const Node& b)const {
    if (idx[0] == b.idx[0]) {
      return idx[1] < b.idx[1];
    }
    return idx[0] < b.idx[0];
  }

  bool operator==(const Node& b)const {
    return (idx[0] == b.idx[0])&&(idx[1] == b.idx[1]);
  }
};

class Edge
{
public:
  int len;
  Node src, dst;
  bool operator<(const Edge& b)const {
    return len < b.len;
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

  virtual int GetEdgeLen(const Node& a, const Node& b) const{
    return 1;
  }
    
  Matrixu16 h;
};

int linearIdx(int row, int col, int cols) {
  return row * cols + col;
}
std::array<int,2> gridIdx(int linIdx, int cols) {
  return { linIdx / cols, linIdx % cols };
}


void swapAxis(std::array<int, 3>& pt, int axis0, int axis1)
{
  int tmp = pt[axis0];
  pt[axis0] = pt[axis1];
  pt[axis1] = tmp;
}

std::vector<std::array<int, 3> > BresenhamLine3D(const std::array<int, 3>& src, const std::array<int, 3>& dst)
{
  std::vector<std::array<int, 3> > line;
  int longestAxis = 0;
  int dx[3];
  int maxDx = 0;
  for (int axis = 0; axis < 3; axis++) {
    dx[axis] = dst[axis] - src[axis];
    if (dx[axis] < 0) {
      dx[axis] = -dx[axis];
    }
    if (dx[axis] > maxDx) {
      maxDx = dx[axis];
      longestAxis = axis;
    }
  }

  std::array<int, 3> ptA = src;
  std::array<int, 3> ptB = dst;
  if (longestAxis != 0) {
    swapAxis(ptA, longestAxis, 0);
    swapAxis(ptB, longestAxis, 0);
  }
  int step[3] = { 1,1,1 };
  for (int axis = 0; axis < 3; axis++) {
    dx[axis] = ptB[axis] - ptA[axis];
    if (dx[axis] < 0) {
      dx[axis] = -dx[axis];
      step[axis] = -1;
    }
  }

  int p1 = 2 * dx[1] - dx[0];
  int p2 = 2 * dx[2] - dx[0];
  line.push_back(ptA);
  while (ptA[0] != ptB[0]) {
    ptA[0] += step[0];
    if (p1 >= 0) {
      ptA[1] += step[1];
      p1 -= 2 * dx[0];
    }
    if (p2 >= 0) {
      ptA[2] += step[2];
      p2 -= 2 * dx[0];
    }
    p1 += 2 * dx[1];
    p2 += 2 * dx[2];
    line.push_back(ptA);
  }

  if (longestAxis != 0) {
    for (size_t i = 0; i < line.size(); i++) {
      swapAxis(line[i], longestAxis, 0);
    }
  }
  return line;
}

bool visible(const Matrixu16& h, const Node& src, const Node& dst)
{
  std::array<int, 3>pointA, pointB;
  pointA = { src.idx[0], src.idx[1], h(src.idx[0], src.idx[1]) + 1 };
  pointB = { dst.idx[0], dst.idx[1], h(dst.idx[0], dst.idx[1]) + 1 };
  std::vector<std::array<int, 3> > line = BresenhamLine3D(pointA, pointB);
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


std::vector<Node> dijkstra(const Graph& G, const Node& src, const Node& dst)
{
  std::vector<Node> path;
  std::vector<Node> lights(2);
  lights[0] = src;
  lights[1] = dst;
  path.push_back(src);
  //shortest distances
  //-1 for uninitialized.
  Matrixu32 sd;
  Matrixu8 visited;
  Matrixu32 prev;
  sd.Allocate(G.h.s[0], G.h.s[1]);
  sd.Fill(-1);
  visited.Allocate(sd.s[0], sd.s[1]);
  prev.Allocate(sd.s[0], sd.s[1]);
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
  if (len < 0) {
    return path;
  }
  path.resize(size_t(len+1));
  Node node=dst;
  for (int i = 0; i < len; i++) {
    path[len - i] = node;
    int prevIdx = prev(node.idx[0], node.idx[1]);
    std::array<int, 2>nodeIdx = gridIdx(prevIdx, prev.s[1]);
    node = Node(nodeIdx);
  }
  return path;
}

void TestLine()
{
  std::array<int, 3> ptA, ptB;
  ptA = { 0, -7, -3 };
  ptB = { -5, 2, -1 };
  auto line = BresenhamLine3D(ptA, ptB);

  for (size_t i = 0; i < line.size(); i++) {
    std::cout << "(" << line[i][0] << ", " << line[i][1] << ", " << line[i][2] << ") ";
  }
  std::cout << "\n";
  ptA = { 1, 1, -1 };
  ptB = { -1, 3, 5 };
  line = BresenhamLine3D(ptA, ptB);
  for (size_t i = 0; i < line.size(); i++) {
    std::cout << "(" << line[i][0] << ", " << line[i][1] << ", " << line[i][2] << ") ";
  }
  std::cout << "\n";
}

void TestVisible(const Graph & G, const Node& light) {
  std::vector<Node>lights;
  lights.push_back(light);
  for (unsigned row = 0; row < G.h.s[0]; row++) {
    for (unsigned col = 0; col < G.h.s[1]; col++) {
      Node node(row, col);
      bool visible = IsNodeVisible(G.h, node, lights);
      if (node == light) {
        std::cout << "L ";
        continue;
      }
      if (visible) {
        std::cout << "1 ";
      }
      else {
        std::cout << "0 ";
      }
    }
    std::cout << "\n";
  }
  std::cout << "----------\n";
}

int sp9() {
//int main() {
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
    std::vector<Node> path = dijkstra(G, lights[0], lights[1]);
    TestVisible(G, lights[0]);
    TestVisible(G, lights[1]);
    if (path.size() == 1) {
      std::cout << "Mission impossible!\n";
    }
    else {
      int len = path.size() - 1;
      std::cout << "The shortest path is " << len << " steps long.\n";
      for (size_t i = 0; i < path.size(); i++) {
        std::cout << "(" << path[i].idx[0] << "," << path[i].idx[1] << ") ";
      }
      std::cout << "\n";
    }
  }
  return 0;
}
