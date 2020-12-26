#include <vector>
#include <iostream>
#include <fstream>
#include "PrimeTable.hpp"
#include "PowerMod.hpp"
#include "ExtEuclid.h"
#include "FileUtil.h"
void p381() {
    int a = 240, b = 46;
    long long x, y;
    extEuclid(a, b, x, y);
    std::cout << x << " " << y << " "<<(a*x + b * y) << "\n";
    std::vector<int> primes;
    readIntVec("prime1e8.bin", primes);
    int maxNum = 1e8;
    long long sum = 0;
    for (size_t i = 2; i < primes.size(); i++) {
        int p = primes[i];
        if (p >= maxNum) {
            break;
        }
        size_t localSum = 0;
        long long a = 1;
        for (int j = 3; j <= 5; j++) {
            int b = p - j + 1;
            int inv = modInverse(b, p);
            a = (a*inv) % p;
            localSum += a;
        }
        sum += (localSum % p);
    }
    std::cout << sum << "\n";
    std::cout << 381 << "\n";
    system("pause");
}