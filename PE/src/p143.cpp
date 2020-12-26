#include <cmath>
#include <iostream>
#include <set>
int computeQ(int p, int a) {
    int delta = 4 * a*a - 3 * p*p;
    int s = (int)std::sqrt(delta);
    if (s*s != delta) {
        return -1;
    }
    int q = -p + s;
    if (q % 2 != 0) {
        return -1;
    }
    q /= 2;

    return q;
}

int computeA(int p, int q) {
    uint64_t prod = (uint64_t )p*p + q * (uint64_t)q + p * (uint64_t)q;
    uint64_t s = (uint64_t)std::sqrt(prod);
    if (s*s != prod) {
        return -1;
    }
    return s;
}

void p143() {
    int maxP = 120000;
    uint64_t sum = 0;
    std::set<int> sumSet;
    for (int p = 1; p < maxP ; p++) {
        for (int q = 1; q <= p; q++) {
            if (p + q >= maxP) {
                break;
            }
            int a = computeA(p, q);
            if (a < 0) {
                continue;
            }
            for (int r = 1; r <= q; r++) {
                if (p + q + r > maxP) {
                    break;
                }
                int b = computeA(q, r);
                if (b < 0) {
                    continue;
                }

                int c = computeA(r, p);
                if (c < 0) {
                    continue;
                }
                std::cout << a << " " << b << " " << c << "\n";
                std::cout << p << " " << q << " " << r << "\n";
                std::cout << (p + q + r) << "============\n";
                int sumLocal = p + q + r;
                if (sumSet.find(sumLocal) != sumSet.end()) {
                    continue;
                }
                sumSet.insert(sumLocal);
                sum += sumLocal;
            }
        }
    }

    std::cout << sum << "\n";
    std::cout << "p143\n";
}
