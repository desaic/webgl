#include <array>
#include <iostream>
#include <vector>
using namespace std;

struct PrimeRing {
  int Q;
  int root;
  int rootPow;
  int rootInv;
};
int ExpMod(int a, int pow, int q);
int64_t ExEuc(int64_t a, int64_t b);
void InitSpecialPrimeRing(PrimeRing& r)
{
  r.Q = 998244353;
  r.root = ExpMod(3, 119, r.Q);
  r.rootPow = 23;
  r.rootInv = int(ExEuc(r.root, r.Q));
}

std::vector<int> polyMod(const std::vector<int>& f,
  const std::vector<int>& g, const PrimeRing& pr);
std::vector<int> fastMult(const std::vector<int>& a, const std::vector<int>& b,
  const PrimeRing& pr);

///@return c s.t. ac = 1 mod b
int64_t ExEuc(int64_t a, int64_t b) {
  int64_t r0 = a;
  int64_t r1 = b;
  int64_t s0 = 1;
  int64_t s1 = 0;
  int64_t t0 = 0;
  int64_t t1 = 1;
  while (r1 > 0) {
    int64_t q = r0 / r1;
    int64_t r2 = r0 - q * r1;
    r0 = r1;
    r1 = r2;

    int64_t s2 = s0 - q * s1;
    s0 = s1;
    s1 = s2;
    int64_t t2 = t0 - q * t1;
    t0 = t1;
    t1 = t2;
  }
  s0 = s0 % b;
  if (s0 < 0) {
    s0 += b;
  }
  return s0;
}

inline int multMod(int a, int b, int q)
{
  return int((int64_t(a) * b) % q);
}
#define MULT_MOD(a,b,q) ((int64_t(a)*(b))%(q))
/// v1 * v2 mod q
std::vector<int> vecMulMod(const std::vector<int>& v1,
  const std::vector<int>& v2,
  const std::vector<int> & binv, int q)
{
  size_t N = v1.size();
  std::vector<int> result(N, 0);
  std::vector<int64_t> interm(N, 0);
  std::vector<int> matRow = v2;
  for (size_t row = 0; row < N; row++) {
    std::vector<int> newRow (matRow.size(),0);
    for (size_t col = 0; col < N; col++) {
      interm[col] += MULT_MOD(v1[row], matRow[col], q);
    }

    int last = matRow[N - 1];
    newRow[0] = MULT_MOD(last , binv[0], q);
    for (size_t col = 1; col < N; col++) {
      int anbi = MULT_MOD(last, binv[col], q);
      newRow[col] = (matRow[col - 1] + q - anbi) % q;
    }

    matRow = newRow;
  }
  for (size_t i = 0; i < N; i++) {
    result[i] = interm[i] % q;
  }
  return result;
}

int ExpMod(int a, int pow, int q) {
  int x = pow;
  int result = 1;
  int p = a;
  do {
    int bit = x % 2;
    if (bit) {
      result = multMod(p, result, q);
    }
    x = x / 2;
    if (x <= 0) {
      break;
    }
    p = multMod(p, p, q);
  } while (x > 0);
  return result;
}

std::vector<int> vecExpMod(const std::vector<int> &b, size_t pow, size_t q) {
  size_t x = pow-1;
  
  std::vector<int> result(b.size());
  std::vector<int> p(b.size());

  for (size_t i = 0; i < b.size(); i++) {
    result[i] = b[i];
    p[i] = b[i];
  }

  std::vector<int> binv(b.size());
  int bi = int(ExEuc(b.back(), q));
  binv[0] = bi;
  for (size_t i = 1; i < binv.size(); i++) {   
    binv[i] = MULT_MOD(bi, b[i-1],q);
  }

  do {
    int bit = x % 2;
    if (bit) {
      result = vecMulMod(p, result, binv, int(q));
    }
    x = x / 2;
    if (x <= 0) {
      break;
    }
    p = vecMulMod(p, p, binv, int(q));
    std::cout <<x<<"\n";
  } while (x > 0);
  return result;
}

/// P(x) = a[0] + a[1]x + a[2]x^2+...
std::vector<int> PolyMultMod(const std::vector<int>& a, const std::vector<int>& b, size_t q) {
  size_t outsize = a.size() + b.size() - 1;
  std::vector<int64_t> outTmp(outsize, 0);
  for (size_t ai = 0; ai < a.size(); ai++) {
    if (a[ai] == 0) {
      continue;
    }
    for (size_t bi = 0; bi < b.size(); bi++) {
      int prod = MULT_MOD(a[ai], b[bi], q);
      outTmp[ai + bi] += prod;
    }
  }
  std::vector<int> pout(outTmp.size());
  for (size_t i = 0; i < outTmp.size(); i++) {
    pout[i] = outTmp[i] % int(q);
  }
  return pout;
}

///binv modulo inverse of b mod q
std::vector<int> PolyRemainder(const std::vector<int>& a, const std::vector<int>& b,
  size_t Q) {
  std::cout << "poly mod\n";
  if (a.size() < b.size()) {
    return a;
  }
  std::vector<int> R(b.size() - 1);
  std::vector<int> tmpR = a;
  int binv = int(ExEuc(b.back(), Q));
  size_t bsize = b.size();
  for (int i = int(a.size()) - 1; i >= int(b.size()) - 1; i--) {
    int quotient = MULT_MOD(tmpR[i], binv, Q);
    if (quotient == 0) {
      continue;
    }
    tmpR[i] = 0;
    for (size_t j = 0; j < bsize-1; j++) {
      int subtract = MULT_MOD(quotient,  b[bsize-j-2], Q);
      tmpR[i-j-1] = (tmpR[i-j-1] + (Q - subtract))%int(Q);
    }
  }
  for (size_t i = 0; i < R.size(); i++) {
    R[i] = tmpR[i];
  }
  return R;
}

std::vector<int> PolyExpMod(const std::vector<int>& b, size_t pow, const PrimeRing & pr) {
  size_t x = pow;

  std::vector<int> result(1,1);
  std::vector<int> p(2, 0);
  result[0] = 1;
  p[1] = 1;

  std::vector<int> poly(b.size() + 1);
  poly[b.size()] = 1;
  for (size_t i = 0; i < b.size(); i++) {
    poly[b.size() - i - 1] = pr.Q - (b[i] % pr.Q);
  }

  do {
    int bit = x % 2;
    if (bit) {
      result = fastMult(p, result, pr);
      result = polyMod(result, poly, pr);
    }
    x = x / 2;
    if (x <= 0) {
      break;
    }
    p = fastMult(p, p, pr);
    p = polyMod(p, poly, pr);
    //std::cout << x << "\n";
  } while (x > 0);
  return result;
}

void TestExEuc() {
  int64_t a = 123456789;
  int64_t b = 998244353;
  int64_t c = ExEuc(a, b);
  std::cout << a << " " << b << " " << c<<"\n";
  std::cout << (a * c) % b << "\n";
}

void TestVecExpMod() {
  std::vector<int> bvec = { 3,7,1,2,3,5,10 };
  std::vector<int> p;
  const int Q = 998244353;
  p = vecExpMod(bvec, 8, Q);
  for (size_t i = 0; i < p.size(); i++) {
    std::cout << p[i] << " ";
  }
  std::cout << "\n";
}

#include <deque>
void BruteForce(size_t K,
  const std::vector<int64_t>& avec,
  const std::vector<int>& bvec) {
  if (K < avec.size()) {
    std::cout << avec[K];
    return;
  }
  const int Q = 998244353;
  std::deque<int64_t> seq(avec.size());
  for (size_t i = 0; i < avec.size(); i++) {
    seq[i] = avec[avec.size() - i - 1];
  }

  for (size_t i = 0; i < (K - avec.size() + 1); i++) {
    int64_t newNum = 0;
    for (size_t j = 0; j < seq.size(); j++) {
      newNum += (seq[j] * bvec[j])%Q;
      newNum %= Q;
    }
    seq.pop_back();
    seq.push_front(newNum);
  }
  std::cout << seq.front() << "\n";
}

void Solve(size_t K,
  const std::vector<int64_t>& avec,
  const std::vector<int>& bvec) {
  const int Q = 998244353;
  if (K < avec.size()) {
    std::cout << avec[K];
    return;
  }

  std::vector<int> p = vecExpMod(bvec, K - avec.size() + 1, Q);
  int64_t sum = 0;
  for (size_t i = 0; i < avec.size(); i++) {
    sum += MULT_MOD(p[i] , avec[avec.size() - i - 1], Q);
    sum %= Q;
  }
  std::cout << sum << "\n";
}

void SolvePoly(size_t K,
  const std::vector<int64_t>& avec,
  const std::vector<int>& bvec) {
  
  if (K < avec.size()) {
    std::cout << avec[K];
    return;
  }
  PrimeRing pr;
  InitSpecialPrimeRing(pr);
  std::vector<int> p = PolyExpMod(bvec, K, pr);
  int64_t sum = 0;
  for (size_t i = 0; i < avec.size(); i++) {
    sum += MULT_MOD(p[i], avec[i], pr.Q);
    sum %= pr.Q;
  }
  std::cout << sum << "\n";
}

int reverse(int num, int lg_n) {
  int res = 0;
  for (int i = 0; i < lg_n; i++) {
    if (num & (1 << i))
      res |= 1 << (lg_n - 1 - i);
  }
  return res;
}

void BitReversePerm(std::vector<int>& a) {
  int n = int(a.size());
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n /2 ;
    for (; j & bit; bit /= 2) {
      j ^= bit;
    }
    j ^= bit;

    if (i < j) {
      swap(a[i], a[j]);
    }
  }
}

//does not work for more than 2^31 length vectors
void ntt(std::vector<int> & a,
  int Q, int root, int rootPow) 
{
  BitReversePerm(a);
  int n = int(a.size());
  int rootOrd = 1 << rootPow;
  for (int k = 2; k <= n; k *= 2) {
    int wk = root;
    for (int i = k; i < rootOrd; i*=2) {
      wk = multMod(wk, wk, Q);
    }
    for (int i = 0; i < n; i += k) {
      int w = 1;
      for (int j = 0; j < k / 2; j++) {
        int y0 = a[i + j];
        int y1 = multMod(a[i + j + k / 2], w, Q);
        a[i + j] = (y0 + y1) % Q;
        int diff = (y0 - y1);
        if (diff < 0) {
          diff += Q;
        }
        a[i + j + k / 2] = diff;
        w = multMod(w, wk, Q);
      }
    }
  }
}

//does not work for more than 2^31 length vectors
void intt(std::vector<int>& a,
  int Q, int root_inv, int rootPow)
{
  ntt(a, Q, root_inv, rootPow);
  int ni = int(ExEuc(int64_t(a.size()), Q));
  for (size_t i = 0; i < a.size(); i++) {
    a[i] = multMod(a[i], ni, Q);
  }
}

void TestSolve() {
  std::vector<int64_t> avec = { 4, 8, 7, 6, 3, 5, 2 };
  std::vector<int> bvec = { 3,7,1,2,3,5,10 };

  for (int i = 8; i < 1000; i++) {
    std::cout << i << "=======\n";
    SolvePoly(i, avec, bvec);
    Solve(i, avec, bvec);
    BruteForce(i, avec, bvec);
    
  }
}

void TestSpeed() {
  std::vector<int64_t> avec(15000, 2);
  std::vector<int> bvec (15000,0);
  for (size_t i = 0; i < bvec.size(); i++) {
    bvec[i] = int(i);
  }
  SolvePoly(1000000000000000LL, avec, bvec);
}

void TestRoot(int root, int rootPow, int Q) {
  int prod = root;
  for (int i = 0; i < rootPow; i++) {
    prod = multMod(prod, prod, Q);
    std::cout << prod << "\n";
  }
}

void TestBitReversePerm() {
  std::vector<int> a(32);
  for (int i = 0; i < a.size(); i++) {
    a[i] = i;
  }
  BitReversePerm(a);
  for (size_t i = 0; i < a.size(); i++) {
    std::cout << a[i] << "\n";
  }
}

void TestNtt() {
  std::vector<int> a(32);
  for (size_t i = 0; i < a.size(); i++) {
    a[i] = int(i);
  }
  int Q = 998244353;
  int root = ExpMod(3, 119, Q);
  int rootPow = 23;
  ntt(a, Q, root, rootPow);
  for (size_t i = 0; i < a.size(); i++) {
    std::cout << a[i] << "\n";
  }

  int rootInv = int(ExEuc(root, Q));
  intt(a, Q, rootInv, rootPow);
  for (size_t i = 0; i < a.size(); i++) {
    std::cout << a[i] << "\n";
  }

}

int nearestPow(int n) {
  int p = 1;
  while (p < n) {
    p <<= 1;
  }
  return p;
}

void rmTrailingZeros(std::vector<int>& a)
{
  size_t length = a.size();
  for (size_t i = 0; i < a.size(); i++) {
    size_t idx = a.size() - i - 1;
    if (a[idx] == 0) {
      length--;
    }
    else {
      break;
    }
  }
  a.resize(length);
}

std::vector<int> fastMult(const std::vector<int>& a, const std::vector<int>& b,
  const PrimeRing & pr)
{
  if (a.size() == 0 || b.size() == 0) {
    return std::vector<int>(0, 0);
  }
  if (a.size() == 1 && b.size() == 1) {
    return std::vector<int>(1, a[0] * b[0]);
  }
  size_t len = a.size() + b.size() - 1;
  len = nearestPow(int(len));
  std::vector<int>ffta=a, fftb=b;
  ffta.resize(len, 0);
  fftb.resize(len, 0);
  ntt(ffta, pr.Q, pr.root, pr.rootPow);
  ntt(fftb, pr.Q, pr.root, pr.rootPow);

  for (size_t i = 0; i < ffta.size(); i++) {
    ffta[i] = multMod(ffta[i], fftb[i], pr.Q);
  }
  intt(ffta, pr.Q, pr.rootInv, pr.rootPow);
  rmTrailingZeros(ffta);
  return ffta;
}

void TestPolyMult() {
  std::vector<int> a(12321);
  std::vector<int> b(13750);
  for (size_t i = 0; i < a.size(); i++) {
    a[i] = int(i) + 3;
  }
  for (size_t i = 0; i < b.size(); i++) {
    b[i] =int(b.size() - i) + 10;
  }
  PrimeRing pr;
  InitSpecialPrimeRing(pr);
  std::vector<int> prodf = fastMult(a, b, pr);
  std::vector<int> prod = PolyMultMod(a, b, pr.Q);
  
  for (size_t i = 0; i < prod.size(); i++) {
    if (prod[i] != prodf[i]) {
      std::cout << i << " " << prod[i] << " " << prodf[i] << "\n";
    }
  }
}


std::vector<int> rev(const std::vector<int>& f)
{
  std::vector<int> r(f.size());
  for (size_t i = 0; i < r.size(); i++) {
    r[i] = f[f.size() - i - 1];
  }
  return r;
}

std::vector<int> polyAddMod(const std::vector<int>& a,
  const std::vector<int>& b, int Q)
{
  std::vector<int> sum;
  if (a.size() < b.size()) {
    return polyAddMod(b, a, Q);
  }
  sum.resize(a.size());
  for (size_t i = 0; i < b.size(); i++) {
    sum[i] = (a[i] + b[i]) % Q;
  }
  for (size_t i = b.size(); i < a.size(); i++) {
    sum[i] = a[i];
  }
  return sum;
}

//assumes h[0] = 1
std::vector<int> polyInvModX(const std::vector<int>& h,
  int lmax, const PrimeRing& pr)
{
  std::vector<int> a(1, 1);
  for (int l = 1; l < lmax; l*=2) {
    std::vector<int> h0(l,0);
    for (size_t i = 0; i < h0.size(); i++) {
      if (i >= h.size()) {
        break;
      }
      h0[i] = h[i];
    }
    std::vector<int> ah0 = fastMult(a, h0, pr);
    std::vector<int> c(1,0);
    if (ah0.size() > size_t(l)) {
      c.resize(ah0.size() - size_t(l));
    }
    if (ah0.size() > size_t(l)) {
      for (size_t i = 0; i < ah0.size() - size_t(l); i++) {
        c[i] = ah0[i + size_t(l)];
      }
    }
    std::vector<int> h1(1, 0);
    if (h.size() > size_t(l)) {
      h1.resize(h.size() - size_t(l));
      for (size_t i = 0; i < h1.size(); i++) {
        h1[i] = h[i + size_t(l)];
      }
    }
    if (h1.size() > size_t(l) +1) {
      h1.resize(size_t(l) +1);
    }

    std::vector<int> ah1 = fastMult(a, h1, pr);
    ah1 = polyAddMod(ah1, c, pr.Q);
    if (ah1.size() > l+1) {
      ah1.resize(l+1);
    }
    std::vector<int> b = fastMult(a, ah1, pr);
    std::vector<int> newa(2 * l);
    for (size_t i = 0; i < l; i++) {
      newa[i] = a[i];
      newa[i + l] = (pr.Q - b[i]);
    }    
    a = newa;
  }
  return a;
}

//only works for g.back() = 1.
std::vector<int> polyMod(const std::vector<int>& f,
  const std::vector<int> & g, const PrimeRing & pr)
{
  if (f.size() < g.size()) {
    return f;
  }
  if (f.size() == 0) {
    return std::vector<int>(0, 0);
  }

  std::vector<int> r;
  if (f.size() == g.size()) {
    int quo = f.back();
    r.resize(f.size() - 1);
    for (size_t i = 0; i < r.size(); i++) {
      int prod = multMod(g[i], quo, pr.Q);
      r[i] = f[i] - prod;
      if (r[i] < 0) {
        r[i] += pr.Q;
      }
    }
    return r;
  }
  
  int l = int(f.size()) - int(g.size());
  int lpow = nearestPow(l);
  std::vector<int> revf = rev(f);
  std::vector<int> revg = rev(g);
  std::vector<int> revgInv = polyInvModX(revg, lpow, pr);
  if (revgInv.size() > l+1) {
    revgInv.resize(l+1);
  }
  if (revf.size() > l+1) {
    revf.resize(l+1);
  }
  //std::vector<int> debugProd = fastMult(revgInv, revg, pr);
  //for (size_t i = 0; i < debugProd.size(); i++) {
  //  std::cout << debugProd[i] << " ";
  //}
  //std::cout << "\n";
  std::vector<int> revq = fastMult(revgInv, revf, pr);
  if (revq.size() > l+1) {
    revq.resize(l+1);
  }
  std::vector<int> q = rev(revq);
  std::vector<int> gq = fastMult(g, q, pr);
  r.resize(f.size());
  for (size_t i = 0; i < gq.size(); i++) {
    r[i] = f[i] - gq[i];
    if (r[i] < 0) {
      r[i] += pr.Q;
    }
  }
  rmTrailingZeros(r);
  return r;
}

void TestPolyModInv()
{
  std::vector<int> a(30);
  for (size_t i = 0; i < a.size(); i++) {
    a[i] = int(i) + 3;
  }
  a[a.size()-1] = 1;
  PrimeRing pr;
  InitSpecialPrimeRing(pr);
  std::vector<int> ainv = polyInvModX(a, 16, pr);
  
  std::vector<int> prod = fastMult(ainv, a, pr);
  for (size_t i = 0; i < prod.size(); i++) {
    std::cout << prod[i] << " ";
  }
  std::cout << "\n";

  std::vector<int> b(13750);
  for (size_t i = 0; i < b.size(); i++) {
    b[i] = int(b.size() - i) + 10;
  }

  std::vector<int> modRef = PolyRemainder(b, a, pr.Q);
  std::vector<int> rem = polyMod(b, a, pr);
  for (size_t i = 0; i < modRef.size(); i++) {
    if (rem.size() <= i || rem[i] != modRef[i]) {
      std::cout << i << " expect " << modRef[i] << " actual " << rem[i] << "\n";
    }
  }
}

int MatProblem() {
  //TestPolyModInv();
  //TestSpeed();
  size_t N, K;
  std::cin >> N >> K;
  std::vector<int64_t> avec(N);
  std::vector<int> bvec(N);
  for (size_t i = 0; i < N; i++) {
    std::cin >> avec[i];
  }
  for (size_t i = 0; i < N; i++) {
    std::cin >> bvec[i];
  }
  SolvePoly(K, avec, bvec);
  return 0;
}
