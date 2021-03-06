//https://www.spoj.com/problems/SHPATH/
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>

struct Neighbor
{
  int idx = 0;
  int dist = 0;
};

struct Node {
  std::vector<Neighbor> n;
};

struct NodeDist {
  int node = 0;
  int dist = 0;
  NodeDist(int n, int d) :node(n), dist(d) {}
};

struct CompareNode{
  bool operator()(const NodeDist& n1, const NodeDist& n2)
  {
    return n1.dist > n2.dist;
  }
};

size_t EdgeHash(int src, int dst, size_t numNodes)
{
  if (src > dst) {
    int tmp = src;
    src = dst;
    dst = tmp;
  }
  return size_t(src) * numNodes + size_t(dst);
}

int shortestDist(const std::vector<Node>& nodes, int src, int dst)
{
  if(src == dst){
    return 0;
  }
  std::vector<int> d(nodes.size(), -1);
  std::priority_queue<NodeDist, std::vector<NodeDist>, CompareNode> pq;
  std::vector<unsigned char> visited(nodes.size(),0);
  d[src] = 0; 
  size_t numNodes = nodes.size();
  pq.push(NodeDist(src, 0));

  while (!pq.empty()) {
    NodeDist nd = pq.top();
    pq.pop();
    if (visited[nd.node]) {
      continue;
    }
    visited[nd.node] = 1;
    if (nd.node == dst) {
      break;
    }
    for (unsigned n = 0; n < nodes[nd.node].n.size(); n++) {
      int nbr = nodes[nd.node].n[n].idx;
      int dist = nodes[nd.node].n[n].dist;
      int newDist = d[nd.node] + dist;
      if (visited[nbr]) {
        continue;
      }
      if (d[nbr]<0 || d[nbr]>newDist) {
        d[nbr] = newDist;
        if (visited[nbr] == 0) {
          NodeDist nbrDist(nbr, d[nbr]);
          pq.push(nbrDist);
        }
      }
    }
  }
  return d[dst];
}

int sp15()
//int main(int argc, char * argv[])
{
  int s = 0;
  std::cin >> s;

  for (int i = 0; i < s; i++) {
    int n;
    std::cin >> n;
    std::map<std::string, int> names;
    std::vector<Node> nodes(n);
    for (int c = 0; c < n; c++) {
      std::string name;
      std::cin >> name;
      names[name] = c;
      int p = 0;
      std::cin >> p;
      nodes[c].n.resize(p);
      for (int n = 0; n < p; n++) {
        std::cin >> nodes[c].n[n].idx;
        //input is 1-based.
        nodes[c].n[n].idx--;
        std::cin >> nodes[c].n[n].dist;
      }
    }

    int r = 0;
    std::cin >> r;
    for (int j = 0; j < r; j++) {
      std::string name1, name2;
      std::cin >> name1 >> name2;
      int src = 0, dst = 0;
      src = names[name1];
      dst = names[name2];      
      int dist = shortestDist(nodes, src, dst);
      std::cout << dist << "\n";
    }
  }

  return 0;
}
