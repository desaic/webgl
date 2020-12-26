void extEuclid(long long a, long long b, long long & x, long long & y) {
    long long q = a / b;
    long long r = a % b;
    if (r == 0) {
        x = 0;
        y = 1;
        return;
    }
    long long x1, y1;
    extEuclid(b, r, x1, y1);
    x = y1;
    y = x1 - y1 * q;
}

long long modInverse(long long  a, long long  p) {
    long long x, y;
    extEuclid(p, a, x, y);
    return (y%p+p) % p;
}