#include <vector>
#include <iostream>
#include <fstream>
void p493() {
    double P = 1;
    for (int i = 0; i < 20; i++) {
        P *= (60.0 - i) / (70.0 - i);
    }
    P = 1 - P;
    std::cout.precision(17);
    std::cout << 7 * P << "\n";

    std::ifstream in("mat345.txt");
    int nRows, nCols;
    in >> nRows >> nCols;
    std::cout << nRows << " " << nCols << "\n";
    std::vector<int> mat(nRows * nCols);
    for (int row = 0; row < nRows; row++) {
        for (int col = 0; col < nCols; col++) {
            in >> mat[row*nCols + col];
        }
    }
    std::cout << "493\n";
    system("pause");
}