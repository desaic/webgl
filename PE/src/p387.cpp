#include "PrimeTable.hpp"
#include "PowerMod.hpp"

#include <iostream>
#include <vector>

void p387() {
    
    std::vector<int> primes;
    fillPrimes(1e4, primes);
    bool ans = isPrime(2011);
    const int nDigit = 13;
    long long ub = 10;
    for (int i = 0; i < nDigit; i++) {
        ub *= 10;
    }
    std::cout << ans << "\n";
    std::vector<std::vector<long long> > h;
    std::vector<long long> nums(9,0);
    for (unsigned int i = 1; i < 10; i++) {
        nums[i-1] = i ;
    }
    h.push_back(nums);
    long long sum = 0;
    for (int digit = 1; digit < nDigit; digit++) {
        const std::vector<long long> & prevArr = h[digit - 1];
        std::vector<long long > newArr;
        for (size_t i = 0; i < prevArr.size(); i++) {
            int digitSum = 0;
            long long oldNum = prevArr[i];
            while (oldNum > 0) {
                digitSum += oldNum % 10;
                oldNum /= 10;
            }
            for (int d = 0; d < 10; d++) {
                long long newNum = prevArr[i] * 10 + d;
                int newSum = digitSum + d;
                if (newNum % newSum != 0) {
                    continue;
                }
                long long quotient = newNum / newSum;
                if (isPrime(quotient)) {
                    for (int e = 1; e < 10; e+=2) {
                        long long n1 = newNum * 10 + e;

                        if (isPrime(n1)) {
                            sum += n1;
                            std::cout << n1 << "\n";
                        }
                    }
                }
                newArr.push_back(newNum);
            }
        }
        h.push_back(newArr);
    }
    std::cout << sum << "\n";
    std::cout << "387\n";
    system("pause");
}