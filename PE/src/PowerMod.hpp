#pragma once
#include <cmath>

size_t power(size_t  x, size_t  y, size_t  p);

// This function is called for all k trials. It returns 
// false if n is composite and returns false if n is 
// probably prime. 
// d is an odd number such that  d*2<sup>r</sup> = n-1 
// for some r >= 1 
bool miillerTest(size_t  d, size_t  n);

// It returns false if n is composite and returns true if n 
// is probably prime.  k is an input parameter that determines 
// accuracy level. Higher value of k indicates more accuracy. 
bool isPrime(size_t n);
size_t mul_mod(size_t a, size_t b, size_t m);