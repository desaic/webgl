#include <algorithm>
#include <iostream>
#include <vector>
#include "PowerMod.hpp"
#include "PrimeTable.hpp"
void p347() {

    std::vector<int> primes;
    //max exponents;
    std::vector<int> e;
    int maxNum = 1e7;
    fillPrimes(maxNum/2, primes);
    e.resize(primes.size(), 1);
    
    for (size_t i = 0; i < primes.size(); i++) {
        long long prod = primes[i];
        while (1) {
            prod *= primes[i];
            if (prod <= maxNum) {
                e[i] ++;
            }
            else {
                break;
            }
        }
    }
    long long sum = 0;
    for (size_t i = 1; i < primes.size(); i++) {
        long long ifactor = primes[i];
        std::vector<long long> maxProd(i, 0);
        for (int iPower = 1; iPower <= e[i]; iPower++) {
            for (size_t j = 0; j < i; j++) {
                long long jfactor = primes[j];
                if (jfactor * ifactor > maxNum) {
                    break;
                }
                for (int jPower = 1; jPower <= e[j]; jPower++) {
                    if (jfactor * ifactor > maxNum) {
                        break;
                    }
                    maxProd[j] = std::max(maxProd[j], ifactor*jfactor);
                    jfactor *= primes[j];
                    
                }
            }
            ifactor *= primes[i];
        }
        for (size_t j = 0; j < maxProd.size(); j++) {
            if (maxProd[j] == 0) {
                break;
            }
            sum += maxProd[j];
        }
    }

    std::cout << sum << "\n";
    std::cout << "347\n";
    system("pause");
}