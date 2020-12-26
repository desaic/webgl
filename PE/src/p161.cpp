#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
#include <unordered_map>

template<typename T> 
struct Grid2D{
  std::vector<T> a;
  size_t width;
  size_t height;
  Grid2D(size_t rows, size_t cols) {
    allocate(rows, cols);
  }
  void allocate(size_t rows, size_t cols) {
    width = cols;
    height = rows;
    a.resize(rows*cols);
  }
  T & operator()(int row, int col) {
    return a[row * width + col];
  }

  void print(std::ostream & o) {
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        o << (*this)(i, j)<<" ";
      }
      o << "\n";
    }
    o << "\n";
  }
};

const int NUM_SHAPES = 6;
const int NUM_BLOCKS = 2;
//row and column relative to the left most square on the top row
int shapes[NUM_SHAPES][NUM_BLOCKS][2]={
  {{0,1},{1,0}},
  {{0,1},{1,1}},
  {{1,0}, {1,1}},
  {{1,-1}, {1,0}},
  {{1,0},{2,0}},
  {{0,1},{0,2}}
};

inline bool inbound(int row, int col, int rows, int cols) {
  return row >= 0 && row < rows && col >= 0 && col < cols;
}

///count number of tilings where first empty spot is at row i, col j.
size_t CountTiling(Grid2D<int> & grid, int i , int j) {
  size_t total = 0;
  for (size_t s = 0; s < NUM_SHAPES; s++) {
    //check all spaces are available
    bool available = true;
    for (int b = 0; b < NUM_BLOCKS; b++) {
      int row = shapes[s][b][0] + i;
      int col = shapes[s][b][1] + j;
      if (!inbound(row, col, grid.height, grid.width)) {
        available = false;
        break;
      }
      int id = grid(row, col);
      if (id != 0) {
        available = false;
        break;
      }      
    }
    if (available) {
      grid(i, j) = s + 1;
      for (int b = 0; b < NUM_BLOCKS; b++) {
        int row = shapes[s][b][0] + i;
        int col = shapes[s][b][1] + j;
        grid(row, col) = s + 1;
      }
      //find next empty cell. if none, return 1.
      int newRow = i;
      int newCol = j;
      bool hasEmpty = false;
      for (; newRow < grid.height; newRow++) {
        if (newRow > i) {
          newCol = 0;
        }
        for (; newCol < grid.width; newCol++) {
          if (grid(newRow, newCol) == 0) {
            hasEmpty = true;
            break;
          }
        }
        if (hasEmpty) {
          break;
        }
      }
      if (!hasEmpty) {
        total++;
      }
      else {
        total += CountTiling(grid, newRow, newCol);
      }

      //remove this block and back track.
      grid(i, j) = 0;
      for (int b = 0; b < NUM_BLOCKS; b++) {
        int row = shapes[s][b][0] + i;
        int col = shapes[s][b][1] + j;
        grid(row, col) =0;
      }
    }
  }
  return total;
}

int setBit(unsigned int i, int b) {
  return i | (1 << b);
}

int getBit(unsigned int i, int b) {
  return (i >> (b )) & 1;
}

void buildTilingMap(Grid2D<int> & grid, int i, int j, 
  std::unordered_map<unsigned, size_t> & tilingCnt, int l)
{
  for (size_t s = 0; s < NUM_SHAPES; s++) {
    if (l < 2) {
      std::cout << l << " " << s << "\n";
    }
    //check all spaces are available
    bool available = true;
    for (int b = 0; b < NUM_BLOCKS; b++) {
      int row = shapes[s][b][0] + i;
      int col = shapes[s][b][1] + j;
      if (!inbound(row, col, grid.height, grid.width)) {
        available = false;
        break;
      }
      int id = grid(row, col);
      if (id != 0) {
        available = false;
        break;
      }
    }
    if (available) {
      grid(i, j) = s + 1;
      for (int b = 0; b < NUM_BLOCKS; b++) {
        int row = shapes[s][b][0] + i;
        int col = shapes[s][b][1] + j;
        grid(row, col) = s + 1;
      }
      
      int newRow = i;
      int newCol = j;
      bool hasEmpty = false;
      for (; newRow < grid.height - 2; newRow++) {
        if (newRow > i) {
          newCol = 0;
        }
        for (; newCol < grid.width; newCol++) {
          if (grid(newRow, newCol) == 0) {
            hasEmpty = true;
            break;
          }
        }
        if (hasEmpty) {
          break;
        }
      }
      if (!hasEmpty) {
        //construct hash key based on the two remaining rows.
        unsigned int key = 0;
        for (int kr = grid.height - 2; kr < grid.height; kr++) {
          for (int kcol = 0; kcol < grid.width; kcol++) {
            if (grid(kr, kcol) > 0) {
              int b = (kr - grid.height + 2) * grid.width + kcol;
              key = setBit(key, b);
            }
          }
        }
        auto it = tilingCnt.find(key );
        if (it != tilingCnt.end()) {
          it->second++;
        }
        else {
          tilingCnt[key] = 1;
          std::cout << tilingCnt.size() << "\n";
//          grid.print(std::cout);
        }
      }
      else {
        buildTilingMap(grid, newRow, newCol, tilingCnt, l+1);
      }

      //remove this block and back track.
      grid(i, j) = 0;
      for (int b = 0; b < NUM_BLOCKS; b++) {
        int row = shapes[s][b][0] + i;
        int col = shapes[s][b][1] + j;
        grid(row, col) = 0;
      }
    }
  }
}

void countByRow(std::unordered_map<unsigned, size_t> & tilingCnt,
  Grid2D<int> & grid, int col0, size_t mult) {
  
  bool hasEmpty = false;
  int col = col0;
  for (; col < grid.width; col++) {
    if (grid(0, col) == 0) {
      hasEmpty = true;
      break;
    }
  }

  if (!hasEmpty) {
    size_t nRow = std::min(size_t(2), grid.height - 1);
    int key = 0;
    for (int kr = 0; kr < nRow; kr++) {
      for (int kc = 0; kc < grid.width; kc++) {
        int b = kr * grid.width + kc;
        if (grid(kr + 1, kc) > 0) {
          key = setBit(key, b);
        }
      }
    }
    
    auto it = tilingCnt.find(key);
    if (it == tilingCnt.end()) {
    //  grid.print(std::cout);
      tilingCnt[key] = mult;
    }
    else {
      it->second += mult;
      return;
    }
  }
  for (size_t s = 0; s < NUM_SHAPES; s++) {
    //check all spaces are available
    bool available = true;
    for (int b = 0; b < NUM_BLOCKS; b++) {
      //block row and column
      int brow = shapes[s][b][0];
      int bcol = shapes[s][b][1] + col;
      if (!inbound(brow, bcol, grid.height, grid.width)) {
        available = false;
        break;
      }
      int id = grid(brow, bcol);
      if (id != 0) {
        available = false;
        break;
      }
    }
    if (available) {
      grid(0, col) = s + 1;
      for (int b = 0; b < NUM_BLOCKS; b++) {
        int brow = shapes[s][b][0];
        int bcol = shapes[s][b][1] + col;
        grid(brow, bcol) = s + 1;
      }
      countByRow(tilingCnt, grid, col + 1, mult);

      grid(0, col) = 0;
      for (int b = 0; b < NUM_BLOCKS; b++) {
        int brow = shapes[s][b][0];
        int bcol = shapes[s][b][1] + col;
        grid(brow, bcol) = 0;
      }
    }
  }
}

void p161() {
  std::cout << "161\n";
  int width = 9;
  int height = 12;
  Grid2D<int> grid(height, width);
  
  std::unordered_map<unsigned, size_t> tilingCnt;
  
  tilingCnt[0] = 1;
  for (int row = 0; row < height; row++) {
    std::unordered_map<unsigned, size_t> nextCnt;
    
    for (auto it = tilingCnt.begin(); it != tilingCnt.end(); it++) {
      int narrowHeight = std::min(height - row, 3);
      Grid2D<int> narrowGrid(narrowHeight, width);
      int key = it->first;
      bool keyValid = true;
      for (int kr = 0; kr < 2; kr++) {
        for (int kc = 0; kc < width; kc++) {
          int b = kr * width + kc;
          int val = getBit(key, b);
          if (!val) {
            continue;
          }
          if (kr >= narrowHeight) {
            keyValid = false;
            break;
          }
          narrowGrid(kr, kc) = 1;
        }
        if (!keyValid) {
          break;
        }
      }
      if (!keyValid) {
        continue;
      }
      countByRow(nextCnt, narrowGrid,0, it->second);
    }
    tilingCnt = nextCnt;
  }
 // unsigned key = 511;
  //4,9 -- 41813
  size_t sum = tilingCnt[0];
    //  unsigned key = it->first;
  //  size_t lv = it->second;
  //  unsigned rkey = 0;
  //  for (int row = 0; row < 2; row++) {
  //    for (int col = 0; col < width; col++) {
  //      int srcB = row * width + col;
  //      int dstB = (1 - row) * width + col;
  //      int srcVal = getBit(key, srcB);
  //      if (!srcVal) {
  //        rkey = setBit(rkey, dstB);
  //      }
  //    }
  //  }
  //  auto dstIt = tilingCnt.find(rkey);
  //  if (dstIt != tilingCnt.end()) {
  //    sum += lv * it->second;
  //  }
  //}

  //size_t cnt = CountTiling(grid, 0, 0);
  std::cout << "count " << sum<< "\n";
  //std::cout << "count ref" << cnt << "\n";
}