#include <iostream>
#include <vector>
#include "BigInteger.hpp"
#include "PrimeTable.hpp"
#include "PowerMod.hpp"
void p146() {
    std::cout << isPrime(423706732511L)<<"\n";
    std::cout << isPrime(1033518113) << "\n";
    std::cout << isPrime(27683185099282663L) << "\n";
    std::vector<int> primes;
    const uint64_t maxN = 150000000;
    fillPrimes(1e4, primes);
    
    int offsets[] = { 1,3,5,7,9,11,13,15,17,19,21,23,25,27 };
    int ans[] =     { 1,1,0,1,1, 0,1 , 0, 0, 0, 0, 0, 0, 1};
    
    size_t m = sizeof(offsets) / sizeof(offsets[0]);

    size_t sum = 0;
    for (size_t n = 10; n < maxN; n+=10) {
        bool valid = true;
        if (n % 10000 == 0) {
            std::cout << n << "...\n";
        }
        if ((n % 7 != 3) && (n % 7 != 4)) {
            continue;
        }
        for (size_t j = 1; j < m; j++) {
            if (ans[j] == 0) {
                continue;
            }
            if (n%offsets[j] == 0) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            continue;
        }
        size_t n2 = n * n;
        
        for (size_t j = 0; j < m; j++) {
            size_t p = n2 + offsets[j];
            if ((ans[j] == 1) && (divides(p, primes))) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            continue;
        }
        for (size_t j = 0; j < m; j++) {
            size_t p = n2 + offsets[j];
            if (isPrime(p) != ans[j]) {
                valid = false;
                break;
            }
        }
        if (valid) {
            std::cout << n << "\n";
            sum += n;
        }
    }
    std::cout << sum << "\n";
    system("pause");
}