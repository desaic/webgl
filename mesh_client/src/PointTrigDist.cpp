#include "PointTrigDist.h"

TrigDistInfo PointTrigDist(const Vec3f& pt, float * trig)
{
	Vec3f diff(pt[0] - trig[0], pt[1] - trig[1], pt[2] - trig[2]);
	Vec3f edge0(trig[3] - trig[0], trig[4] - trig[1], trig[5] - trig[2]);
	Vec3f edge1(trig[6] - trig[0], trig[7] - trig[1], trig[8] - trig[2]);
	float a00 = Dot(edge0, edge0);
	float a01 = Dot(edge0, edge1);
	float a11 = Dot(edge1, edge1);
	float b0 = -Dot(diff, edge0);
	float b1 = -Dot(diff, edge1);

	float f00 = b0;
	float f10 = b0 + a00;
	float f01 = b0 + a01;

	Vec2f p0, p1, p;
	float dt1, h0, h1;

	// Compute the endpoints p0 and p1 of the segment.  The segment is
	// parameterized by L(z) = (1-z)*p0 + z*p1 for z in [0,1] and the
	// directional derivative of half the quadratic on the segment is
	// H(z) = Dot(p1-p0,gradient[Q](L(z))/2), where gradient[Q]/2 =
	// (F,G).  By design, F(L(z)) = 0 for cases (2), (4), (5), and
	// (6).  Cases (1) and (3) can correspond to no-intersection or
	// intersection of F = 0 with the triangle.
	if (f00 >= (float)0)
	{
		if (f01 >= (float)0)
		{
			// (1) p0 = (0,0), p1 = (0,1), H(z) = G(L(z))
			GetMinEdge02(a11, b1, p);
		}
		else
		{
			// (2) p0 = (0,t10), p1 = (t01,1-t01),
			// H(z) = (t11 - t10)*G(L(z))
			p0[0] = (float)0;
			p0[1] = f00 / (f00 - f01);
			p1[0] = f01 / (f01 - f10);
			p1[1] = (float)1 - p1[0];
			dt1 = p1[1] - p0[1];
			h0 = dt1 * (a11 * p0[1] + b1);
			if (h0 >= (float)0)
			{
				GetMinEdge02(a11, b1, p);
			}
			else
			{
				h1 = dt1 * (a01 * p1[0] + a11 * p1[1] + b1);
				if (h1 <= (float)0)
				{
					GetMinEdge12(a01, a11, b1, f10, f01, p);
				}
				else
				{
					GetMinInterior(p0, h0, p1, h1, p);
				}
			}
		}
	}
	else if (f01 <= (float)0)
	{
		if (f10 <= (float)0)
		{
			// (3) p0 = (1,0), p1 = (0,1), H(z) = G(L(z)) - F(L(z))
			GetMinEdge12(a01, a11, b1, f10, f01, p);
		}
		else
		{
			// (4) p0 = (t00,0), p1 = (t01,1-t01), H(z) = t11*G(L(z))
			p0[0] = f00 / (f00 - f10);
			p0[1] = (float)0;
			p1[0] = f01 / (f01 - f10);
			p1[1] = (float)1 - p1[0];
			h0 = p1[1] * (a01 * p0[0] + b1);
			if (h0 >= (float)0)
			{
				p = p0;  // GetMinEdge01
			}
			else
			{
				h1 = p1[1] * (a01 * p1[0] + a11 * p1[1] + b1);
				if (h1 <= (float)0)
				{
					GetMinEdge12(a01, a11, b1, f10, f01, p);
				}
				else
				{
					GetMinInterior(p0, h0, p1, h1, p);
				}
			}
		}
	}
	else if (f10 <= (float)0)
	{
		// (5) p0 = (0,t10), p1 = (t01,1-t01),
		// H(z) = (t11 - t10)*G(L(z))
		p0[0] = (float)0;
		p0[1] = f00 / (f00 - f01);
		p1[0] = f01 / (f01 - f10);
		p1[1] = (float)1 - p1[0];
		dt1 = p1[1] - p0[1];
		h0 = dt1 * (a11 * p0[1] + b1);
		if (h0 >= (float)0)
		{
			GetMinEdge02(a11, b1, p);
		}
		else
		{
			h1 = dt1 * (a01 * p1[0] + a11 * p1[1] + b1);
			if (h1 <= (float)0)
			{
				GetMinEdge12(a01, a11, b1, f10, f01, p);
			}
			else
			{
				GetMinInterior(p0, h0, p1, h1, p);
			}
		}
	}
	else
	{
		// (6) p0 = (t00,0), p1 = (0,t11), H(z) = t11*G(L(z))
		p0[0] = f00 / (f00 - f10);
		p0[1] = (float)0;
		p1[0] = (float)0;
		p1[1] = f00 / (f00 - f01);
		h0 = p1[1] * (a01 * p0[0] + b1);
		if (h0 >= (float)0)
		{
			p = p0;  // GetMinEdge01
		}
		else
		{
			h1 = p1[1] * (a11 * p1[1] + b1);
			if (h1 <= (float)0)
			{
				GetMinEdge02(a11, b1, p);
			}
			else
			{
				GetMinInterior(p0, h0, p1, h1, p);
			}
		}
	}
	TrigDistInfo info;
	info.closest = Vec3f(trig[0], trig[1], trig[2]) + p[0] * edge0 + p[1] * edge1;
	info.bary[0] = 1 - p[0] - p[1];
	info.bary[1] = p[0];
	info.bary[2] = p[1];
	diff = pt - info.closest;
	info.sqrDist = Dot(diff, diff);
	return info;
}

void GetMinEdge02(float& a11, float& b1, Vec2f& p)
{
	p[0] = (float)0;
	if (b1 >= (float)0)
	{
		p[1] = (float)0;
	}
	else if (a11 + b1 <= (float)0)
	{
		p[1] = (float)1;
	}
	else
	{
		p[1] = -b1 / a11;
	}
}

inline void GetMinEdge12(float a01, float a11, float b1,
	float f10, float f01, Vec2f& p)
{
	float h0 = a01 + b1 - f10;
	if (h0 >= (float)0)
	{
		p[1] = (float)0;
	}
	else
	{
		float h1 = a11 + b1 - f01;
		if (h1 <= (float)0)
		{
			p[1] = (float)1;
		}
		else
		{
			p[1] = h0 / (h0 - h1);
		}
	}
	p[0] = (float)1 - p[1];
}

inline void GetMinInterior(const Vec2f& p0, float h0,
	const Vec2f& p1, float h1, Vec2f& p)
{
	float z = h0 / (h0 - h1);
	p = ((float)1 - z) * p0 + z * p1;
}
