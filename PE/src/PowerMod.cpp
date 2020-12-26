#include "PowerMod.hpp"
#include "BigInteger.hpp"
#include "uint128_t.h"
#include <iostream>
#include <algorithm>
#include <sstream>
size_t prodMod(size_t a, size_t b, size_t p) {
    //uint128_t ba(a), bb(b), bp(p);
    //ba = ba * bb;
    //ba = ba % bp;
    //return ba.lower();
    //size_t res = 0;
    //a = a % p;
    //while (b > 0) {
    //    if (b % 2 > 0) {
    //        res = (res + a) % p;
    //    }
    //    a = (a * 2) % p;
    //    b /= 2;
    //}
    //return res % p;
    return (a*b) % p;
}

size_t power(size_t x, size_t y, size_t p)
{
    size_t res = 1;      // Initialize result 
    x = x % p;  // Update x if it is more than or 
                // equal to p 
    while (y > 0)
    {
        // If y is odd, multiply x with result 
        if (y & 1) {
            res = prodMod(res, x, p);
        }

        // y must be even now 
        y = y >> 1; // y = y/2 
        x = prodMod(x,x,p);
    }
    return res;
}

bool miillerTest(size_t d, size_t n, size_t a)
{
    // Pick a random number in [2..n-2] 
    // Corner cases make sure that n > 4 
    //int a = 2 + rand() % (n - 4);

    // Compute a^d % n 
    size_t x = power(a, d, n);

    if (x == 1 || x == n - 1)
        return true;

    // Keep squaring x while one of the following doesn't 
    // happen 
    // (i)   d does not reach n-1 
    // (ii)  (x^2) % n is not 1 
    // (iii) (x^2) % n is not n-1 
    while (d != n - 1)
    {
        x = prodMod(x,x,n);
        d *= 2;

        if (x == 1)      return false;
        if (x == n - 1)    return true;
    }

    // Return composite 
    return false;
}

bool isPrime(size_t n)
{
    //// Corner cases 
    //if (n <= 1 || n == 4)  return false;
    //if (n <= 3) return true;

    // Find r such that n = 2^d * r + 1 for some r >= 1 
    size_t  d = n - 1;
    while (d % 2 == 0) {
        d /= 2;
    }

    double l = std::log(n);
    size_t ln = (size_t)(2*l*l);
    size_t k = std::min(n - 1, ln);

    // Iterate given nber of 'k' times 
    for (size_t i = 2; i < k; i++)
        if (!miillerTest(d, n, i))
            return false;

    return true;
}
size_t mul_mod(size_t a, size_t b, size_t m)
{
  long double x;
  size_t c;
  int64_t r;
  if (a >= m) a %= m;
  if (b >= m) b %= m;
  x = a;
  c = x * b / m;
  r = (int64_t)(a * b - c * m) % (int64_t)m;
  return r < 0 ? r + m : r;
}