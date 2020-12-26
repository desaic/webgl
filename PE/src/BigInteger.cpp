#include "BigInteger.hpp"
#include <iostream>
std::ostream & operator<<(std::ostream & os, const BigInteger & bi)
{
    // TODO: insert return statement here
    if (bi.sign < 0) {
        os << '-';
    }
    os << bi.val;
    return os;
}

void testBigInteger() {
    BigInteger a(1357);
    BigInteger b(2468);
    BigInteger c(3579);
    BigInteger d(1357);
    BigInteger e(-1357);
    BigInteger f(-2468);
    std::cout << "test compare :\n";
    std::cout << a.compare(b) << " -1\n";
    std::cout << b.compare(a) << " 1\n";
    std::cout << a.compare(a) << " 0\n";

    std::cout << e.compare(e) << " 0\n";
    std::cout << d.compare(e) << " 1\n";
    std::cout << e.compare(f) << " 1\n";
    std::cout << f.compare(e) << " -1\n";

    BigInteger g = BigInteger(1000);
    BigInteger h = BigInteger(-1);
    BigInteger i = g.add(h);
    std::cout << i << " 999\n";
    std::cout << a.add(b) << " 3825\n";
    std::cout << a.add(f) << " -1111\n";
    std::cout << f.add(a) << " -1111\n";

    BigInteger j= BigInteger(-1992);
    BigInteger k = BigInteger(-9);
    BigInteger l = k.add(j);
    std::cout << l << " -2001\n";

    std::cout << a.prod(b) << " 3349076\n";
    std::cout << c.prod(f) << " -8832972\n";
    std::cout << d.prod(e) << " -1841449\n";
}

void BigInteger::addDigit(int d, int i) {
    if (i >= size()) {
        return;
    }
    i = size() - i - 1;
    int d0 = val[i] - '0';
    d0 += d;
    int newDigit = d0 % 10;
    int carry = (d0>9);
    val[i] = newDigit + '0';
    i--;
    while (carry > 0 && i >= 0) {
        d0 = val[i] - '0';
        d0 += carry;
        if (d0 > 9) {
            carry = 1;
            d0 -= 10;
        }else{
            carry = 0;
        }
        val[i] = d0 + '0';
        i--;
    }
}

void BigInteger::subDigit(int d, int i) {
    if (i >= size()) {
        return;
    }
    i = size() - i - 1;
    int firstNonZero = 0;
    while (firstNonZero < size() && val[firstNonZero] == '0') {
        firstNonZero++;
    }
    while (d>0 && i>=0) {
        int d0 = val[i] - '0';
        d0 -= d;
        if (d0 < 0) {
            d0 += 10;
            d = 1;
        }
        else {
            d = 0;
        }
        val[i] = d0 + '0';
        i--;
    }
}

int BigInteger::compare(BigInteger b) {
    if (sign > b.sign) {
        return 1;
    }
    if (sign < b.sign) {
        return -1;
    }

    if (size() > b.size()) {
        return sign;
    }
    if (size() < b.size()) {
        return -sign;
    }

    for (int i = 0; i < size(); i++) {
        if (val[i] > b.val[i]) {
            return sign;
        }
        if (val[i] < b.val[i]) {
            return -sign;
        }
    }
    return 0;
}

int compareStr(std::string a, std::string b) {
    if (a.size() > b.size()) {
        return 1;
    }
    if (a.size() < b.size()) {
        return -1;
    }

    for (int i = 0; i < a.size(); i++) {
        if (a[i] > b[i]) {
            return 1;
        }
        if (a[i] < b[i]) {
            return -1;
        }
    }
    return 0;
}

BigInteger BigInteger::add(BigInteger b) {
    BigInteger c;
    //maximum number of digits
    size_t nDigits = std::max(size(), b.size()) + 1;
    c.val.resize(nDigits, '0');
    for (size_t i = 0; i < size(); i++) {
        int idx = nDigits - i - 1;
        c.val[idx] = val[size() - i - 1];
    }

    if (sign == b.sign) {
        c.sign = sign;
        //add
        for (size_t i = 0; i < b.size(); i++) {
            c.addDigit(b.val[i] - '0', b.size() - i - 1);
        }
    }
    else {
        //subtract
        int cmp = compareStr(val, b.val);
        if (cmp > 0) {
            c.sign = sign;
            for (size_t i = 0; i < b.size(); i++) {
                c.subDigit(b.val[i] - '0', b.size() - i - 1);
            }
        }
        else if (cmp == 0) {
            c.val = "0";
        }
        else {
            c = b.add(*this);
        }
    }
    c.rmLeadingZero();
    return c;
}

void BigInteger::rmLeadingZero() {
    size_t i = 0;
    for (; i < val.size(); i++) {
        if (val[i] != '0') {
            break;
        }
    }
    if (i == val.size()) {
        return;
    }
    val = val.substr(i);
}

BigInteger BigInteger::prod(BigInteger b) {
    BigInteger p;
    int maxDigits = size() + b.size();
    p.val.resize(maxDigits, '0');
    p.sign = sign * b.sign;
    for (int i = 0; i < b.size(); i++) {
        if (b.val[i] == 0) {
            continue;
        }
        char db = b.val[i] - '0';
        for (int j = 0; j < size(); j++) {
            if (val[i] == 0) {
                continue;
            }
            char da = val[j] - '0';
            char ab = da * db;
            char d0 = ab % 10;
            int digitIdx = (b.size() - i - 1) + (size() - j - 1);
            p.addDigit(d0, digitIdx);
            if (ab > 0) {
                p.addDigit(ab/10, digitIdx + 1);
            }
        }
    }
    p.rmLeadingZero();
    return p;
}

BigInteger BigInteger::div(BigInteger b, BigInteger & r)
{
    BigInteger q;
    q.sign = sign * b.sign;
    
    int bShift = q.size() - b.size();
    q.val.resize(std::max(1,bShift+1), '0');
    char bDigit = b.val[0];
    r.sign = 1;
    r.val = val;
    for (; bShift >= 0; bShift--) {
        int cmp = compareStr(b.val, r.val);
        if (cmp > 0) {
            break;
        }
        int rIdx = b.size() - 1 + bShift;
        char rDigit = r.val[r.size() - rIdx - 1] - '0';
        char qDigit = rDigit / bDigit;

    }
    return q;
}
