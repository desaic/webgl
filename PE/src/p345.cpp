#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include "bitUtil.h"
void p345() {
    std::ifstream in("mat345.txt");
    int nRows, nCols;
    in >> nRows >> nCols;
    std::cout << nRows << " " << nCols << "\n";
    std::vector<int> mat(nRows * nCols);

    std::cout << getBit(20, 2) << "\n";
    int b = setBit(20, 5);
    std::cout << b << "\n";
    for (int row = 0; row < nRows; row++) {
        for (int col = 0; col < nCols; col++) {
            in >> mat[row*nCols + col];
        }
    }
    //map from set to max sum
    std::vector<int> m( 1<<nRows , 0);
    std::vector<int> keylist(1,0);
    
    for (int row = 0; row < nRows; row++) {
        std::vector<int> newKeyList;
        for (size_t k = 0; k < keylist.size(); k++) {
            int key = keylist[k];
            for (int col = 0; col < nCols; col++) {
                if (getBit(key, col) > 0) {
                    continue;
                }
                int newKey = setBit(key, col);
                int newSum = m[key] + mat[row * nCols + col];
                if (m[newKey] == 0) {
                    newKeyList.push_back(newKey);
                }
                m[newKey] = std::max(m[newKey], newSum);
            }
        }
        keylist = newKeyList;
    }
    int maxSum = m[(1<<nRows)-1];
    std::cout << maxSum<< " 345\n";
    system("pause");
}
