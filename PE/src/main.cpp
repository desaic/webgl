#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <string>
#include "mpir.h"

extern void p171();
extern int sp12();

void TestBigInt()
{
  mpz_t a,b,c;
  mpz_init_set_str(a, "123456789101112131415161718192F", 16);
  mpz_init_set_str(b, "8765432108988878685848382818079", 10);
  mpz_init(c);
  //mpz_add(c, a, b);
  //floor div
  mpz_fdiv_r(c, b, a);
  int strSize = mpz_sizeinbase(c, 10) + 2;
  std::string buf(strSize, 0);
  mpz_get_str(&(buf[0]), 10, c);
  std::cout << buf << "\n";
}

int main(int argc, char * argv[]) {
  //TestBigInt();
  sp12();
  return 0;
}
