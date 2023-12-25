#include "Quadrature.h"
#include <cmath>
const float Gauss2Pt[2]=
{
 -1.0f/std::sqrt(3.0f),
  1.0f/std::sqrt(3.0f)
};

const float Gauss4Wt[4] =
{
  8*(18.0f - std::sqrt(30.0f)) / 72.0f,
  8*(18.0f + std::sqrt(30.0f)) / 72.0f,
  8*(18.0f + std::sqrt(30.0f)) / 72.0f,
  8*(18.0f - std::sqrt(30.0f)) / 72.0f
};

Quadrature makeGauss2_2D();
Quadrature makeGauss2();

const Quadrature Quadrature::Gauss2_2D = makeGauss2_2D();

const Quadrature Quadrature::Gauss2=makeGauss2();

Quadrature makeGauss2_2D()
{
	Quadrature q;
	q.x.resize(4);
  q.w.resize(4, 1.0f);
  q.x[0] = Vec3f(Gauss2Pt[0], Gauss2Pt[0], 0);
  q.x[1] = Vec3f(Gauss2Pt[1], Gauss2Pt[0], 0);
  q.x[2] = Vec3f(Gauss2Pt[0], Gauss2Pt[1], 0);
  q.x[3] = Vec3f(Gauss2Pt[1], Gauss2Pt[1], 0);
	return q;
}

Quadrature makeGauss2()
{
  Quadrature q;
  q.x.resize(8);
  q.w.resize(8, 1.0f);
  q.x[0] = Vec3f(Gauss2Pt[0], Gauss2Pt[0], Gauss2Pt[0]);
  q.x[1] = Vec3f(Gauss2Pt[0], Gauss2Pt[0], Gauss2Pt[1]);
  q.x[2] = Vec3f(Gauss2Pt[0], Gauss2Pt[1], Gauss2Pt[0]);
  q.x[3] = Vec3f(Gauss2Pt[0], Gauss2Pt[1], Gauss2Pt[1]);
  q.x[4] = Vec3f(Gauss2Pt[1], Gauss2Pt[0], Gauss2Pt[0]);
  q.x[5] = Vec3f(Gauss2Pt[1], Gauss2Pt[0], Gauss2Pt[1]);
  q.x[6] = Vec3f(Gauss2Pt[1], Gauss2Pt[1], Gauss2Pt[0]);
  q.x[7] = Vec3f(Gauss2Pt[1], Gauss2Pt[1], Gauss2Pt[1]);
  return q;
}

Quadrature::Quadrature()
{}
