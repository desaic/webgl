#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include "FileUtil.h"
void p500() {
    std::vector<int> primes;
    readIntVec("prime1e8.bin", primes);

    std::priority_queue<long long, std::vector<long long>, std::greater<long long> > q;

    int cnt = 0;
    int limit = 500500;
    int pidx = 0;
    int mod = 500500507;
    long long num = 1;
    while (cnt < limit) {
        if (q.empty()) {
            long long factor = primes[pidx];
            pidx++;
            cnt++;
            num *= factor;
            num %= mod;
            q.push(factor*factor);
        }
        else {
            long long minq = q.top();
            long long nextPrime = primes[pidx];
            if (minq < nextPrime) {
                q.pop();
                cnt++;
                q.push(minq*minq);

                num *= minq;
                num %= mod;
            }
            else {
                pidx++;
                q.push(nextPrime * nextPrime);
                cnt++;
                num *= nextPrime;
                num %= mod;
            }
        }
    }
    std::cout << num << "\n";
    std::cout << "500\n";
    system("pause");
}