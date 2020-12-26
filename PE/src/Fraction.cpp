#include "Fraction.h"
Fraction operator+(const Fraction & a, const Fraction & b) {
  Fraction c;
  c.d = a.d*b.d;
  c.n = a.sign * a.n * b.d + b.sign * a.d * b.n;
  if (c.n < 0) {
    c.n = -c.n;
    c.sign = -1;
  }
  c.reduce();
  return c;
}

std::ostream& operator<<(std::ostream& stream, const Fraction & f)
{
  stream << f.n << "/" << f.d;
  return stream;
}