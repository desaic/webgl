#include "bitUtil.h"
int setBit(int a, int bit) {
    a |= (1 << bit);
    return a;
}

int getBit(int a, int bit) {
    int ans = a & (1 << bit);
    return ans;
}