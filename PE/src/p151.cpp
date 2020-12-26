#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
void p151() {
    std::vector<int> state(4, 1);
   // state[0] = 2;
    //count of ways to reach each state.
    std::map<std::vector<int>, double>c;
    c[state] = 1;
    double e = 0;
    for (int day = 2; day <= 14; day++) {
        std::map<std::vector<int>, double> newc;
        for (auto it = c.begin(); it != c.end(); it++) {
            std::vector<int> s = it->first;
            int totalSheets = 0;
            for (size_t i = 0; i < s.size(); i++) {
                totalSheets += s[i];
            }
            for (size_t i = 0; i < s.size(); i++) {
                if (s[i] == 0) {
                    continue;
                }
                std::vector<int> news = s;
                news[i] --;
                for (size_t j = 0; j < i; j++) {
                    news[j]++;
                }
                auto newIt = newc.find(news);
                if (newIt == newc.end()) {
                    newc[news] = it->second * (s[i]/(double)totalSheets);
                }
                else {
                    newIt->second += it->second * (s[i] / (double)totalSheets);
                }
            }
        }
        c = newc;
        double totalCount = 0;
        double oneCount = 0;
        for (auto it = c.begin(); it != c.end(); it++) {
            std::vector<int> s = it->first;
            totalCount += it->second;
            long long  pieceCount = 0;
            for (size_t i = 0; i < s.size(); i++) {
                pieceCount += s[i];
            }
            if (pieceCount == 1) {
                oneCount+=it->second;
            }
        }
        e += oneCount / (double)totalCount;
    }
    std::cout.precision(17);
    std::cout << e<< "\n";
    std::cout << "151\n";
    system("pause");
}