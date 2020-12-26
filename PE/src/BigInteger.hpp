#pragma once
#include <string>
#include <algorithm>
#include <iostream>
class BigInteger {
public:
    BigInteger() { val = "0"; }
    BigInteger(size_t i) { val = std::to_string(i); }
    BigInteger(int i) {
        if (i < 0) { 
            sign = -1; 
            i = -i;
        }       
        val = std::to_string(i); 
    }
    BigInteger(long long i) { 
        if (i < 0) {
            sign = -1;
            i = -i;
        }
        val = std::to_string(i);
    }
    size_t size() { return val.size(); }
    //ignores overflow for efficiency.
    void addDigit(int d, int i);

    void subDigit(int d, int i);

    int compare(BigInteger b);

    BigInteger add(BigInteger b);

    BigInteger prod(BigInteger b);

    BigInteger div(BigInteger b, BigInteger & r);

    void rmLeadingZero();

    int sign = 1;

    std::string val;

    friend std::ostream& operator<<(std::ostream& os, const BigInteger& dt);
};

void testBigInteger();

int compareStr(std::string a, std::string b);