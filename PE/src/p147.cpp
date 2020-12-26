#include <iostream>
#include <string>
#include <algorithm>
//w >=h, row<=h+w
int minCol(int row, int w, int h) {
    if (row <= h) {
        return -row;
    }
    return (-h) + (row - h);
}

int maxCol(int row, int w, int h) {
    if (row <= w) {
        return row;
    }
    return 2*w-row;
}

int minRow(int col, int w, int h) {
    if (col <= 0) {
        return -col;
    }
    return col;
}

uint64_t countRect(int w, int h) {
    uint64_t sum = 0;
    //horizontal and vertical rects
    sum = (w + 1)*(h + 1) * w * h / 4;

    int rowMin = 0;
    int rowMax = w + h - 1;

    //loop over corner vertices:
    for (int row = rowMin; row <= rowMax; row ++) {
        int colMin = minCol(row,w,h);
        int colMax = maxCol(row,w,h);
        for (int col = colMin; col < colMax; col++) {
            //row lower bound
            int rl = minRow(col, w, h);
            //number of candidate rows
            int cr = row - rl;
            int cl = colMax - col;
            if (cr == 0 || cl == 0) {
                continue;
            }
            //subtract triangle outside of the rectangle
            sum += cr * cl;
            if (rl < colMax) {
                int l = colMax - rl;
                sum -= (l*(l + 1)) / 2;
            }
        }
    }
    return sum;
}

void p147() {
    int maxW = 47;
    int maxH = 43;
    uint64_t sum = 0;
    for (int w = 1; w <= maxW; w++) {
        for (int h = 1; h <= maxH; h++) {
            if (h > w) {
                continue;
            }
            size_t s = countRect(w, h);
            std::cout << w << " " << h << " " << s<<"\n";
            sum += s;
            if (w <= maxH && w>h) {
                std::cout << h << " " << w << " " << s<<"\n";
                sum += s;
            }
        }
    }
    std::cout << sum << "\n";
    system("pause");
}