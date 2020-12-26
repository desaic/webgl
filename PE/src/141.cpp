#include <iostream>
#include <vector>
#include "PrimeTable.hpp"

int gcd(int a, int b) {
    int r = a % b;

    while (r > 0) {
        a = b;
        b = r;
        r = a % b;
    }
    return b;
}

uint32_t icbrt64(uint64_t x) {
    int s;
    uint32_t y;
    uint64_t b;

    y = 0;
    for (s = 63; s >= 0; s -= 3) {
        y += y;
        b = 3 * y*((uint64_t)y + 1) + 1;
        if ((x >> s) >= b) {
            x -= b << s;
            y++;
        }
    }
    return y;
}

struct Combo {
    
    //size of each bin
    std::vector<int> N;

    //current combination
    std::vector<int> c;

    void init() {
        c.resize(N.size(), 0);
        if (c.size() > 0) {
            c[0] = -1;
        }
    }

    bool hasNext() {
        for (size_t i = 0; i < c.size(); i++) {
            if (c[i] < N[i]) {
                return true;
            }
        }
        return false;
    }

    //compute next combination
    void next() {
        c[0] ++;
        int i = 0;
        while (i < c.size() && c[i] > N[i]) {
            c[i] = 0;
            if (i + 1 < c.size()) {
                c[i + 1] ++;
            }
            i++;
        }
    }
};

int prod(const std::vector<int> factor, const std::vector<int> & e,
    int ub) {
    int p = 1;
    for (size_t i = 0; i < e.size(); i++) {
        for (int j = 0; j < e[i]; j++) {
            p *= factor[i];
            if (p >= ub) {
                return p;
            }
        }
    }
    return p;
}

void p141V0() {

    int maxK = 1e6;
    int64_t k = 102;
    int64_t sum = 0;
    for (k = 2; k <= maxK; k++) {
        int64_t k2 = k * k;
        std::vector<int> factor, exponent;
        for (int64_t c = 1; c < k; c++) {
            int64_t c2 = c * c;
            if (c2 > k) {
                break;
            }
            if (k2%c != 0) {
                continue;
            }
            for (int64_t s = 1; s < k; s++) {
                if (c*s*(c*c*c*s + c) > k2) {
                    break;
                }
                if (k2 % (c*s) != 0) {
                    continue;
                }
                int64_t q = k2 / c / s - c;
                if (q%s != 0) {
                    continue;
                }
                q /= s;
                uint64_t b = icbrt64(q);
                if (b*b*b != q) {
                    continue;
                }
                if (b <= c) {
                    continue;
                }
                std::cout << k << " " << b << " " << c << " " << s << "\n";
                sum += k * k;
                goto end_loop_k;
            }
        }
    end_loop_k:;

    }
    std::cout << sum << "\n";
    system("pause");
}

void p141() {

    std::vector<int> plist;
    fillPrimes(1e6, plist);
    int maxK = 1e6;
    int64_t k = 102;
    int64_t sum = 0;
    for (k = 2; k <= maxK; k++) {
        int64_t k2 = k * k;
        std::vector<int> factor, exponent;
        factorize(k, plist, factor, exponent);
        Combo comb;
        for (size_t i = 0; i < exponent.size(); i++) {
            exponent[i] *= 2;
        }
        comb.N = exponent;
        comb.init();
        while (comb.hasNext()) {
            comb.next();
            int c = prod(factor, comb.c, k);
            if (c >= k) {
                continue;
            }

            Combo combs;
            combs.N = exponent;
            for (size_t i = 0; i < comb.c.size(); i++) {
                combs.N[i] -= comb.c[i];
            }
            combs.init();
            while (combs.hasNext()) {
                combs.next();
                int64_t s = prod(factor, combs.c, k);
                if (c*s*(c*c*c*s + c) > k2) {
                    continue;
                }

                int64_t q = k2 / c / s - c;
                if (q%s != 0) {
                    continue;
                }
                q /= s;
                uint64_t b = icbrt64(q);
                if (b*b*b != q) {
                    continue;
                }
                if (b <= c) {
                    continue;
                }
                std::cout << k << " " << b << " " << c << " " << s << "\n";
                sum += k * k;
                goto end_loop_k;
            }
        }
    end_loop_k:;
    }
    std::cout << sum << "\n";
    system("pause");
}
