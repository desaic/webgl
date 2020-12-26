#include <algorithm>
#include <iostream>
#include <vector>

struct Mat {
    std::vector<int> data;
    int nRow, nCol;
    int & operator()(int row, int col) {
        return data[row * nCol + col];
    }
    Mat() :nRow(0), nCol(0) {}
    Mat(int r, int c) :nRow(r), nCol(c) {
        data.resize(r*c);
    }
};

inline int linIdx(int row, int col, int nCol) {
    return row * nCol + col;
}

void p150() {
    int mod = 1 << 20;
    int nRow = 1000;
    int nCol = 1000;
    Mat m(nRow, nCol);
    //lower left triangle
    int k = 0;
    long long t = 0;
    for (int row = nRow - 1; row >=0; row--) {
        for (int col = 0; col < nCol; col++) {
            if (row + col >= nRow) {
                break;
            }

            t = (615949 * t + 797807) % mod;
            long long sk = t - (1 << 19);
            m(row + col, col) = (int)sk;
        }
    }
    long long minSum = 0;
    Mat rowSum(nRow, nCol);
    for (int row = 0; row < nRow; row++) {
        rowSum(row, 0) = m(row, 0);
        for (int col = 1; col <= row; col++) {
            rowSum(row, col) = rowSum(row, col - 1) + m(row, col);
        }
    }
    for (int row = 0; row < nRow; row++) {
        for (int col = 0; col <= row; col++) {
            long long sum = 0;
            for (int s = 0; s < nRow; s++) {
                if (row + s >= nRow) {
                    break;
                }

                int c0 = 0;
                if (col > 0) {
                    c0 = rowSum(row + s, col - 1);
                }
                int c1 = rowSum(row+s, col+s);
                sum += c1 - c0;
                minSum = std::min(sum, minSum);
            }
        }
    }
    std::cout << minSum << "\n";
    std::cout << "150\n";
    system("pause");
}