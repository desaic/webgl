#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
inline unsigned int linIdx(unsigned int row, unsigned int col, unsigned int width) {
    return row * width + col;
}

long long maxSumSeg(const std::vector<int > & a) {
    std::vector<long long> sum(a.size() + 1, 0);
    for (size_t i = 0; i < a.size(); i++) {
        sum[i + 1] = sum[i] + a[i];
    }
    long long minV = 0;
    long long maxSum = a[0];
    for (size_t i = 1; i < sum.size(); i++) {

        long long s = sum[i] - minV;
        maxSum = std::max(s, maxSum);
        minV = std::min(minV, sum[i]);
    }
    return maxSum;
}

void p149() {

    std::vector<int> testVec({ 20,1,-2,3,4, -5,6,7,-3,2,2 ,-5,-6,7,8,9,-20,1,-20,10,10,10,10,10});
    std::cout << maxSumSeg(testVec) << "\n";

    std::cout << 0 << "\n";
    std::vector<int> grid;
    int gridSize = 2000;
    int m = 1000000;
    grid.resize(gridSize*gridSize);
    for (int i = 0; i < 55; i++) {
        long long k = i + 1;
        int sk = (100003 - (200003 - (300007 * k * k))  * k)  % m - 500000 ;
        grid[i] = sk;
    }
    for (size_t i = 55; i < grid.size(); i++) {
        int k = i + 1;
        int sk = (grid[i - 24] + grid[i - 55] + m) % m - 500000;
        grid[i] = sk;
    }
    long long maxSum = 0;
    std::vector<int> v;
    //lower right triangle
    for (int col = 0; col < gridSize; col++) {
        v.clear();
        for (int row = gridSize-1; row>=0; row --) {
            if (col + (gridSize - row - 1) >= gridSize) {
                break;
            }
            unsigned int l = linIdx(row , col + gridSize - 1 - row, gridSize);
            v.push_back(grid[l]);
        }
        long long m = maxSumSeg(v);
        maxSum = std::max(m, maxSum);
    }
    //upper left triangle
    for (int row = 0; row < gridSize; row++) {
        v.clear();
        for (int col = 0; col < gridSize; col++) {
            if (row - col < 0) {
                break;
            }
            unsigned int l = linIdx(row - col, col, gridSize);
            v.push_back(grid[l]);
        }
        long long m = maxSumSeg(v);
        maxSum = std::max(m, maxSum);
    }

    for (int row = 0; row < gridSize; row++) {
        v.clear();
        for (int col = 0; col < gridSize; col++) {
            if (col + row >= gridSize) {
                break;
            }

            unsigned int l = linIdx(col, row + col, gridSize);
            v.push_back(grid[l]);
        }
        long long m = maxSumSeg(v);
        maxSum = std::max(m, maxSum);
    }

    for (int row = 0; row < gridSize; row++) {
        v.clear();
        for (int col = 0; col < gridSize; col++) {
            if (col + row >= gridSize) {
                break;
            }
            unsigned int l = linIdx(row + col, col, gridSize);
            v.push_back(grid[l]);
        }
        long long m = maxSumSeg(v);
        maxSum = std::max(m, maxSum);
    }

  

    for (int row = 0; row < gridSize; row++) {
        v.clear();
        for (int col = 0; col < gridSize; col++) {
            unsigned int l = linIdx(row, col, gridSize);
            v.push_back(grid[l]);
        }
        long long m = maxSumSeg(v);
        maxSum = std::max(m, maxSum);
    }

    for (int row = 0; row < gridSize; row++) {
        v.clear();
        for (int col = 0; col < gridSize; col++) {
            unsigned int l = linIdx(col, row, gridSize);
            v.push_back(grid[l]);
        }
        long long m = maxSumSeg(v);
        maxSum = std::max(m, maxSum);
    }

    std::cout << grid[9] << "\n" << grid[99] << "\n";
    std::cout << maxSum << "\n";
    system("pause");
}
