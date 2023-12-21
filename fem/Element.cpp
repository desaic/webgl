#include "Element.h"

//z     3        7
//   1        5
//        y
//      2        6
//   0        4     x
unsigned HexEdges[12][2] = {{0, 4}, {2, 6}, {1, 5}, {3, 7}, {0, 2}, {4, 6},
                            {1, 3}, {5, 7}, {0, 1}, {2, 3}, {4, 5}, {6, 7}};
void HexElement::Edge(unsigned ei, unsigned& v1, unsigned& v2) const {
  if (ei < 12) {
    v1 = _v[HexEdges[ei][0]];
    v2 = _v[HexEdges[ei][1]];
  }
}
unsigned HexFaces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                           {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
std::vector<unsigned> HexElement::Face(unsigned fi) const {
  std::vector<unsigned> f;
  if (fi < 6) {
    f.resize(4);
    for (unsigned i = 0; i < 4; i++) {
      f[i] = _v[HexFaces[fi][i]];
    }
  }
  return f;
}

// z    
//    3 
//         y
//       2   
//    0       1   x
unsigned TetEdges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {2, 3}, {3, 1}};
void TetElement::Edge(unsigned ei, unsigned& v1, unsigned& v2) const {
  if (ei < 6) {
    v1 = _v[TetEdges[ei][0]];
    v2 = _v[TetEdges[ei][1]];
  }
}

unsigned TetFaces[4][3] = {{0, 1, 3}, {0, 2, 1}, {0, 3, 2}, {1, 2, 3}};
std::vector<unsigned> TetElement::Face(unsigned fi) const {
  std::vector<unsigned> f;
  if (fi < 4) {
    f.resize(3);
    for (unsigned i = 0; i < 3; i++) {
      f[i] = _v[TetFaces[fi][i]];
    }
  }
  return f;
}