#include <vector>
#include <iostream>
#include "Fraction.h"
#include <set>
#include <queue>
#include "bigInt.h"
#include "bitUtil.h"
#include "PrimeTable.hpp"

typedef std::vector<uint8_t> Choice;

std::set<std::string> primeCombos(int prime, int maxNum) {
    int maxMult = maxNum / prime;
    int nComb = std::pow(2, maxMult) - 1;
    std::set<std::string> combos;
    std::string zeroStr("0");
    combos.insert(zeroStr);
    for (int i = 1; i < maxMult; i++) {
        zeroStr = zeroStr.append("0");
        combos.insert(zeroStr);
    }
    BigInt::Rossi zero(0);
    BigInt::Rossi p2(prime * prime);
    for(int c = 0; c<nComb; c++){
        int comb = c + 1;
        BigInt::Rossi num (0), denom(1);
        for (int d = 0; d < maxMult; d++) {
            int bit = getBit(comb, d);
            if (bit > 0) {
                BigInt::Rossi a2 ((d + 1)*(d + 1));
                if (num == zero) {
                    num = BigInt::Rossi(1);
                    denom = BigInt::Rossi(a2);
                }
                else {
                    num = num * a2 + denom;
                    denom = denom * a2;
                }
            }
        }

        bool divides = (num % p2 == zero);
        if (divides) {
            num = num/p2;
            while (denom % p2 == zero) {
                if (num % p2 != zero) {
                    divides = false;
                    break;
                }
                num = num/p2;
                denom = denom/p2;
            }
        }
        
        if (divides) {
            std::string prefixStr;
            for (int d = 0; d < maxMult; d++) {
                int bit = (getBit(comb, d)>0);
                prefixStr = prefixStr + char(bit + '0');
                combos.insert(prefixStr);
            }
        }
    }
    return combos;
}

long long subsetSum(std::vector<Fraction> s, Fraction target) {
    long long cnt = 0;
    std::map<Fraction, int> f;
    f[Fraction(0, 1)]=1;
    for (int i = 0; i < s.size(); i++) {
        std::map<Fraction, int> newf = f;
        Fraction frac = s[i];
        for(auto it = f.begin(); it!=f.end(); it++){
            Fraction newSum = (it->first) + frac;
            newf[newSum] = it->second;
        }
        f = newf;
        std::cout << i << " " << f.size() << "\n";
    }
    //std::vector<int> c(sum+1, 0);
    //c[0] = 1;
    //c[s[0]] = 1;
    //int searchRange = 5;
    //for (int i = 1; i < s.size(); i++) {
    //    for (long long j = c.size()-1; j >=0; j--) {
    //        if (c[j] > 0) {
    //            int newSum = j + s[i];
    //            bool found = false;
    //            for (long long k = newSum - searchRange; k < newSum + searchRange; k++) {
    //                if (k<0 || k>c.size() - 1) {
    //                    continue;
    //                }
    //                if (c[k] > 0) {
    //                    found = true;
    //                    c[k]++;
    //                    break;
    //                }                
    //            }
    //            if (!found) {
    //                c[newSum] ++;
    //            }
    //        }
    //    }
    //}
    //for (int i = target - searchRange; i < target + searchRange; i++) {
    //    std::cout << i << " " << c[i] << "\n";
    //}
    //cnt = c[target];
    return cnt;
}

//@param i current number to multiply
bool checkPrimeCombo(int i, const Choice &c, const std::vector< std::set<std::string> > & pc,
    const std::vector<int> & primes) {
    bool allMatch = true;
    for (int j = 2; j < pc.size(); j++) {
        int p = primes[j];
        int maxMult = i / p;
        if (maxMult < 1) {
            continue;
        }
        bool hasMatch = false;

        std::string prefix ;
        for (int m = 0; m < maxMult; m++) {
            int choiceBit = c[p*(m + 1)];
            prefix = prefix + char(choiceBit + '0');
        }

        if (pc[j].find(prefix) == pc[j].end()) {
            allMatch = false;
            break;
        }
    }
    return allMatch;
}

void p152() {
    Fraction a(9, 15);
    Fraction b(1, 4);
    Fraction c = a + b;
    std::vector<int> primes;
    fillPrimes(80, primes);
    BigInt::Rossi bi(1);
    for (int i = 2; i < 80; i++) {
        bi = bi * BigInt::Rossi(i);
    }
    std::cout << bi.toStrDec() << "\n";
    int primeIdx = 7;
    long long cnt = 0;
    int maxNum = 80;
    Choice choice(maxNum + 1, 0);
    choice[2] = 1;
    //integral adding from 1/80^2
    //decimal approximate.
    std::vector<double> integral(choice.size()+2, 0);
    double s = 0;
    int maxPrime = 17;
    for (size_t i = maxNum; i >= 2; i--) {
        s += (1.0 / i / i);
        integral[i] = s;
    }

    std::vector< std::set<std::string> > pc;
    pc.resize(primeIdx);
    for (int i = 2; i < primeIdx; i++) {
        int p = primes[i];
        pc[i] = primeCombos(p, maxNum);
    }

    std::vector<int> validPrimes;
    validPrimes.push_back(2);
    validPrimes.push_back(3);
    for (int i = 2; i < pc.size(); i++) {
        if (pc[i].size() > 1) {
            validPrimes.push_back(primes[i]);
        }
    }

    std::vector<Fraction> intSet;
    for (int i = 3; i <= maxNum; i++) {
        int num = i;
        for (int j = 0; j < validPrimes.size(); j++) {
            int p = validPrimes[j];
            while (num%p == 0) {
                num /= p;
            }
        }
        if (num != 1) {
            continue;
        }
        std::cout << i << "\n";
        Fraction f (1, i * i);
        intSet.push_back(f);
    }
    //cnt = subsetSum(intSet, Fraction(1,4));
    //std::cout << cnt << "\n";
    //system("pause");

    std::queue<Choice >q;
    q.push(choice);
    
    for (int i = 3; i <= maxNum; i++) {
        std::queue<Choice> newq;
        int num = i;
        while (!q.empty()) {
            Choice c = q.front();
            q.pop();
            double prefixSum = 0;
            for (int j = 2; j < i; j++) {
                if (c[j] == 1) {
                    prefixSum += 1.0 / j / j;
                }
            }
            bool badPrime = false;
            for (int p = primeIdx; p < primes.size(); p++) {
                if (i%primes[p] == 0) {
                    badPrime = true;
                    break;
                }
                if (i < primes[p]) {
                    break;
                }
            }

            //bool allMatch = checkPrimeCombo(i,c,pc, primes);
            ////check if prime combos still possible
            //if (!allMatch) {
            //    continue;
            //}
            if (checkPrimeCombo(i, c, pc, primes) && prefixSum + integral[i + 1] >= 0.5 - 1e-7 ) {
                newq.push(c);
            }
            if (badPrime) {
                continue;
            }

            if ( prefixSum + integral[i] >= 0.5 - 1e-7 && prefixSum + (1.0/i/i)<0.5 + 1e-7) {
                c[i] = 1;
                if (checkPrimeCombo(i, c, pc, primes)) {
                    newq.push(c);
                }                
            }
        }
        q = newq;
        std::cout << i<<" "<<q.size() << "\n";
        if (q.size() > 1e9) {
            break;
        }
    }
    double target = 0.5;
    cnt = 0;
    while (!q.empty()) {
        Choice c = q.front();
        q.pop();
        double sum = 0;
        for (size_t i = 0; i < c.size(); i++) {
            if (c[i] > 0) {
                sum += 1.0 / i / i;
            }
        }
        //std::cout << sum << "\n";
        if (std::abs(sum - target)<1e-10) {
            std::cout << sum << " ";
            for (size_t i = 0; i < c.size(); i++) {
                if (c[i] > 0) {
                    std::cout << i << " ";
                }
            }
            std::cout << "\n";
            cnt++;
        }
    }
    std::cout << cnt << "\n";
    std::cout << "152\n";
    system("pause");
}