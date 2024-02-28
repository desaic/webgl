#pragma once
/// reverse communication conjugate gradient
/// i.e., matrix is not represented here.
/// only needs answers for A*x for any x.
int cg_rc ( int n, const double b[], double x[], double r[], double z[], 
  double p[], double q[], int job );