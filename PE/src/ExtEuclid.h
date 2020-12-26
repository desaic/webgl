#pragma once

template<typename T>
T GCD(T a, T b) {
    T r = a % b;
    while (r > 0) {
        a = b;
        b = r;
        r = a % b;
    }
    return b;
}
long long modInverse(long long  a, long long  b);
void extEuclid(long long  a, long long  b, long long  & x, long long  & y);