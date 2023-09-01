#include "Water.h"

int Water::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  u.Allocate(sx + 1, sy + 1, sz + 1);
  phi.Allocate(sx + 1, sy + 1, sz + 1);
  p.Allocate(sx, sy, sz);
}

int Water::AdvectU() {}
int Water::AddBodyForce() {}
int Water::SolveP() {}
int Water::AdvectPhi() {}

int Water::Step() {
  AdvectU();
  AddBodyForce();
  SolveP();
  AdvectPhi();
  return 0;
}
