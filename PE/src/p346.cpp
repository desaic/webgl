#include <algorithm>
#include <iostream>
#include <vector>
#include <set>
void p346() {
    std::set<long long> used;
    int maxBase = 1e6;
    long long sum = 1;
    used.insert(1);
    long long maxNum = 1e12;
     for (long long base = 2; base <= maxBase; base++) {
        long long p = base*base;
        long long num = p + base + 1;
        do {
            if (num<maxNum && (used.find(num) == used.end())) {
                //std::cout << num << "\n";
                sum += num;
                used.insert(num);
            }
            p *= base;
            num += p;
        } while (num < maxNum);
    }
    std::cout << sum << "\n";
    std::cout << "346\n";
    system("pause");
}
