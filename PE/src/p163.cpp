#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>

#include <algorithm>
using namespace std;
const int MAX_EDGE = 1000;
const int NUM_EDGE_TYPE = 6;
int edgeToInt(int edgeType, int edgeIdx) {
  return edgeIdx * NUM_EDGE_TYPE + edgeType;
}

void intToEdge(int i, int & edgeType, int & edgeIdx) {
  edgeType = i % NUM_EDGE_TYPE;
  edgeIdx = i / NUM_EDGE_TYPE;
}

size_t trigToId(int e1, int e2, int e3) {
  int trig[] = { e1, e2, e3 };
  std::sort(trig, trig + 3);
  size_t id = (trig[0] * MAX_EDGE + trig[1])*MAX_EDGE + trig[2];
  return id;
}

void addAllPairs(const std::vector<int> & edges, std::map<int , set<int> > & adj,
  std::set<size_t> & trips
  ) {
  for (size_t i = 0; i < edges.size(); i++) {
    for (size_t j = 0; j < edges.size(); j++) {
      if (i == j) {
        continue;
      }
      auto it = adj.find(edges[i]);
      if (it == adj.end()) {
        adj[edges[i]] = std::set<int>();
        adj[edges[i]].insert(edges[j]);
      }
      else {
        it->second.insert(edges[j]);
      }
    }
  }

  if (edges.size() < 3) {
    return;
  }

  for (int i = 0; i < int(edges.size()) - 2; i++) {
    for (int j = i+1; j<int(edges.size()) - 1; j++) {
      for (int k = j + 1; k<int(edges.size()); k++) {
        size_t id = trigToId(edges[i], edges[j], edges[k]);
        trips.insert(id);
      }
    }
  }

}

void p163() {
  cout << 163<< "\n";
  //iterate over hex centers
  int N = 36;
  //intersection map of edges

  //edge id
  //   0 /  \ 1
  //    /    \
  //   / 3   4\
  //  /        \
  // /____|5____\
  //       2
  //edge ordering: first up to lower, then left to right
  map<int, set<int>> adj;
  set<size_t> trips;
  //last row has no new triangles.
  for (int row = 0; row <= N; row++) {
    for (int col = 0; col <= row; col++) {
      //hex center
      std::vector<int> edges;
      if (col < N) {
        edges.push_back(edgeToInt(0, col));
      }
      if (row-col < N) {
        edges.push_back(edgeToInt(1, row - col));
      }
      if (row > 0) {
        edges.push_back( edgeToInt(2, row - 1));
      }
      int eid = 2 * row - col - 1;
      if (eid >= 0 && eid < 2 * N - 1) {
        edges.push_back(edgeToInt(3, eid));
      }

      eid = row + col - 1;
      if (eid >= 0 && eid < 2 * N - 1) {
        edges.push_back(edgeToInt(4, eid));
      }

      eid = 2*col - row + N - 1;
      if (eid >= 0 && eid < 2 * N - 1) {
        edges.push_back(edgeToInt(5, eid));
      }

      addAllPairs(edges, adj, trips);
      //5 neightbors of each hex center
      if (col < row) {
        //child edge set
        std::vector<int> ce(2,0);
        ce[0] = edgeToInt(2, row - 1);
        ce[1] = edgeToInt(5, 2 * col - row + N);
        addAllPairs(ce, adj, trips);
      }

      if (col < row && row < N) {
        std::vector<int> ce(3, 0);
        ce[0] = edgeToInt(3, 2 * row - col - 1);
        ce[1] = edgeToInt(4, row + col);
        ce[2] = edgeToInt(5, 2 * col - row + N);
        addAllPairs(ce, adj, trips);
      }

      if (row < N) {
        std::vector<int> ce(2, 0);
        ce[0] = edgeToInt(1, row - col);
        ce[1] = edgeToInt(4, row + col);
        addAllPairs(ce, adj, trips);
      }

      if (row < N) {
        std::vector<int> ce(3, 0);
        ce[0] = edgeToInt(3, 2 * row - col);
        ce[1] = edgeToInt(4, row + col);
        ce[2] = edgeToInt(5, 2 * col - row + N - 1);
        addAllPairs(ce, adj, trips);
      }

      if (row < N) {
        std::vector<int> ce(2, 0);
        ce[0] = edgeToInt(0, col);
        ce[1] = edgeToInt(3, 2 * row - col);
        addAllPairs(ce, adj, trips);
      }
    }
  }
  size_t trigCnt = 0;
  size_t numEdges = adj.size();
  for (auto e1 = adj.begin(); e1 != adj.end(); e1++) {
    std::set<int> nbrs = e1->second;
    std::vector<int> n;
    for (auto e2: nbrs) {
      if (e2 < e1->first) {
        continue;
      }
      n.push_back(e2);
    }

    for (int i = 0; i < int(n.size() )- 1; i++) {
      int e2 = n[i];
      for (int j = i + 1; j < n.size(); j++) {
        int e3 = n[j];
        if (trips.find(trigToId(e1->first, e2, e3)) != trips.end()) {
          continue;
        }
        if (adj[e2].find(e3) != adj[e2].end()) {
          trigCnt++;
        }
      }
    }

  }
  cout << "num edges " << numEdges << "\n";
  cout << "num trigs" << trigCnt << "\n";
}
