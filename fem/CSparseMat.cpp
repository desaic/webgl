#include "CSparseMat.h"
void CSparseMat::Allocate(unsigned rows, unsigned cols, unsigned nz) {
  csMat.nzmax = nz;
  csMat.m=rows;
  csMat.n=cols;
  colStart.resize(cols + 1, 0);
  vals.resize(nz, 0);
  rowIdx.resize(nz, 0);
  csMat.p = colStart.data();          /* column pointers (size n+1) or col indices (size nzmax) */
  csMat.i = rowIdx.data();          /* row indices, size nzmax */
  csMat.x = vals.data();          /* numerical values, size nzmax */
  csMat.nz=-1;    /* # of entries in triplet matrix, -1 for compressed-col */
}