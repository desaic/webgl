#include <iostream>
#include <vector>
#include <map>
void p142() {
    std::cout << "p142\n";

    //x = m2+n2
    //y = 2mn
    //x+y=a2, x-y=b2
    //m=(a+b)/2
    //n=(a-b)/2
    int maxM = 2000;

    std::map<uint64_t, std::vector<int > > squareSums;

    for (int m = 2; m < maxM; m++) {
        for (int n = 1; n < m; n++) {
            uint64_t x = m * (uint64_t)m + n * (uint64_t)n;
            
            auto it = squareSums.find(x);
            if (it == squareSums.end()) {
                std::vector<int> mlist(1, m);
                squareSums[x] = mlist;
            }
            else {

                it->second.push_back(m);
            }
        }
    }

    bool found = false;

    for (auto it = squareSums.begin(); it != squareSums.end(); it++) {
        if (it->second.size() > 1) {
            const std::vector<int> & mList = it->second;
            std::vector<uint64_t> nList(mList.size(), 0);
            std::vector<uint64_t> prodList(mList.size(), 0);
            for (size_t i = 0; i < mList.size(); i++) {
                int m = mList[i];
                uint64_t x = it->first;
                uint64_t n = (int)std::sqrt(x - m * (uint64_t)m);
                nList[i] = n;
                prodList[i] = 2 * m * n;
            }
            
            for (size_t i = 0; i < nList.size(); i++) {
                uint64_t y = 2 * nList[i] * mList[i];
                auto it1 = squareSums.find(y);
                if ( it1 == squareSums.end()) {
                    continue;
                }
                for (size_t j = 0; j < it1->second.size(); j++) {
                    uint64_t m1 = it1->second[j];
                    uint64_t n1 = (int)std::sqrt(it1->first - m1 * m1);
                    uint64_t z = 2 * m1 * n1;
                    for (size_t k = 0; k < prodList.size(); k++) {
                        if (k == i) {
                            continue;
                        }
                        if (z == prodList[k]) {
                            found = true;
                            std::cout << "found ";
                            std::cout << it->first << " " << y << " " << z << "\n";
                            uint64_t sum = it->first + y + z;
                            std::cout << sum << "\n";
                            //goto loop_end;
                        }
                    }
                }
            }
        }
    }
//loop_end:;
    system("pause");
}