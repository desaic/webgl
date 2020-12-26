#include <iostream>
#include <vector>

void printGrid(const std::vector<std::vector<int> > & grid) {
  for (size_t row = 0; row < grid.size(); row++) {
    for (size_t col = 0; col < grid[row].size(); col++) {
      std::cout << grid[row][col] << " ";
    }
    std::cout << "\n";
  }
  std::cout << "\n";
}

bool inBound(int x, int lb, int ub) {
  return x >= lb && x <= ub;
}

void p166() {
  std::cout << 166 << "\n";
  int64_t MAXN = 99999999;
  int64_t cnt = 0;
  std::vector<std::vector<int> > grid(4);
  for (int i = 0; i < 4; i++) {
    grid[i].resize(4);
  }
  for (int64_t i = 0; i <= MAXN; i++) {
    int64_t x = i;
    for (int row = 0; row < 3; row++) {
      for (int col = 0; col < 3; col++) {
        if (row == 2 && col == 0) {
          continue;
        }
        int digit = x % 10;
        grid[row][col] = digit;
        x /= 10;
      }
    }

    for (int sum = 0; sum <= 36; sum++) {
      int infer = sum - grid[0][0] - grid[0][1] - grid[0][2];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[0][3] = infer;

      infer = sum - grid[1][0] - grid[1][1] - grid[1][2];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[1][3] = infer;

      infer = sum - grid[0][0] - grid[1][1] - grid[2][2];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[3][3] = infer;

      infer = sum - grid[0][3] - grid[1][3] - grid[3][3];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[2][3] = infer;

      infer = sum - grid[2][3] - grid[2][2] - grid[2][1];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[2][0] = infer;


      infer = sum - grid[0][0] - grid[1][0] - grid[2][0];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[3][0] = infer;

      if (grid[3][0] + grid[2][1] + grid[1][2] + grid[0][3] != sum) {
        continue;
      }

      infer = sum - grid[0][1] - grid[1][1] - grid[2][1];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[3][1] = infer;

      infer = sum - grid[0][2] - grid[1][2] - grid[2][2];
      if (infer < 0) {
        continue;
      }
      if (infer > 9) {
        continue;
      }
      grid[3][2] = infer;

      if (grid[3][0] + grid[3][1] + grid[3][2] + grid[3][3] != sum) {
        continue;
      }

      //printGrid(grid);
      cnt++;
    }
  }
  std::cout << cnt << "\n";
}