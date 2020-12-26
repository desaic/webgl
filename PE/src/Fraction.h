#pragma once

#include <ostream>
#include "ExtEuclid.h"
class Fraction {
public:
    long long d, n;
    int sign;
    Fraction():d(1), n(0),
        sign (1){}
    Fraction(long numerator, long denominator) :
        d(denominator), n(numerator), sign(1) {
        reduce();
    }
    void reduce() {
        if (n == 0) {
            d = 1;
            return;
        }
        long long g = GCD(d, n);
        d /= g;
        n /= g;
    }

    bool operator<(const Fraction& rhs) const {
        if (sign != rhs.sign) {
            return sign < rhs.sign;
        }
        return n*rhs.d < d*rhs.n;  //assume that you compare the record based on a
    }
    
    bool operator==(const Fraction& rhs) const {
        if (sign != rhs.sign) {
            return false;
        }
        return (n == rhs.n) && (d == rhs.d);
    }

    friend std::ostream& operator<< (std::ostream& stream, const Fraction& f);
};

Fraction operator+(const Fraction & a, const Fraction & b);

std::ostream& operator<<(std::ostream& stream, const Fraction & f);
