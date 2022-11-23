#include "ShallowWater.h"
#include <iostream>
void ShallowWater::Init(unsigned sizex, unsigned sizey) {
  _H.Allocate(sizex, sizey, 0);
  _h.Allocate(sizex, sizey, 0);
  _u.Allocate(sizex, sizey, 0);
  _v.Allocate(sizex, sizey, 0);
}

void ShallowWater::Step1D(float dt) {
  //v = 0 for 1D case.
  //dh/dt + d(H+h)u/dx + d(H+h)v/dy = 0
  //du/dt + u du/dx = -g dh/d - ku + nu d2u/dx2
  Vec2u size = _h.GetSize();
  float invDx = 1.0f / _dx;
  Array2Df newh(size[0], size[1]);
  Array2Df newu(size[0], size[1]);
  float drag = 0.1;
  //update h
  for (unsigned i = 0; i < size[0] - 1; i++) {
    float um = _u(i, 0);
    float up = _u(i + 1, 0);
    float hi = _h(i, 0);
    float Hi = _H(i, 0);
    /// TODO H is not used properly.
    // advect h with linear interp
    float advecth = hi;
    float uavg = (um + up) / 2;
    // source x position in grid cell unit
    float srcx = i - invDx * uavg * dt;
    if (srcx < 0) {
      srcx = 0;
    }
    int leftIdx = int(srcx);

    if (leftIdx < 0) {
      advecth = _h(i, 0);
    } else if (leftIdx >= size[0] - 2) {
      // the last h value is not valid.
      advecth = _h(size[0] - 2, 0);
    } else {
      float lefth = _h(leftIdx, 0);
      float righth = _h(leftIdx + 1, 0);
      float wr = srcx - leftIdx;
      float wl = 1.0f - wr;
      advecth = lefth * wl + righth * wr;
    }
    newh(i, 0) = advecth;
  }
  _h = newh;
  for (unsigned i = 0; i < size[0]-1; i++) {
    float um = _u(i, 0);
    float up = _u(i+1, 0);
    float hi = _h(i, 0);
    float Hi = _H(i, 0);

    //divergence term
    float hdu = (hi + Hi) * (up - um) * invDx;
    newh(i, 0) = hi - hdu * dt;
  }

  _h = newh;
  //update u
  for (unsigned i = 0; i < size[0]; i++) {
    float ui = _u(i, 0);
    float hp = _h(i, 0);
    // left values
    float um = 0;
    float hm = hp;
    if (i > 0) {
      um = _u(i - 1, 0);
      hm = _h(i - 1, 0);
    }

    // right values
    float up = 0;    
    if (i < size[0] - 1) {
      up = _u(i + 1, 0);
    } else {
      hp = _h(i - 1, 0);
    }

    // check correctness of upwind
    
    float udu = 0;
    float dh = (hp - hm) * invDx;
    if (ui > 0) {
      udu = ui * invDx * (ui - um);
    } else {
      udu = ui * invDx * (up - ui);
    }
    newu(i, 0) = ui - ( g * dh + drag * ui) * dt;
  }

  _u = newu;
}

void ShallowWater::SethRow(const std::vector<float>& vals, unsigned row) {
  Vec2u size = _h.GetSize();
  unsigned cols = vals.size();
  if (cols > size[0]) {
    cols = size[0];
  }
  if (row >= size[1]) {
    row = size[1] - 1;
  }
  for (unsigned col = 0; col < cols; col++) {
    _h(col, row) = vals[col];
  }
}
