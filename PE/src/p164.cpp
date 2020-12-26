#include <vector>
#include <iostream>
void p164() {
    int nDigit = 20;
    std::vector<long long> c(100, 0);
    for (int i = 10; i <= 90; i++) {
        int d0 = i / 10;
        int d1 = i % 10;
        if (d0 + d1 <= 9) {
            c[i] = 1;
        }
    }
    for (int digit = 2; digit < nDigit; digit++) {
        std::vector<long long> newc(100, 0);
        for (int d = 0; d <= 9; d++) {
            for (int prevDigits = 0; prevDigits <= 90; prevDigits++) {
                int d0 = prevDigits / 10;
                int d1 = prevDigits % 10;
                if (d + d0 + d1 > 9) {
                    continue;
                }
                int newDigit = d1 * 10 + d;
                newc[newDigit] += c[prevDigits];
            }
        }
        c = newc;
    }
    long long sum = 0;
    for (int i = 0; i <= 90; i++) {
        sum += c[i];
    }
    std::cout << "sum " << sum << "\n";
    std::cout << 164 << "\n";
    system("pause");
}