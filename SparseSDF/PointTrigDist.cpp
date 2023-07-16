#include "PointTrigDist.h"

void GetMinEdge02(float a11, float b1, Vec2f& p);

void GetMinEdge12(float a01, float a11, float b1, float f10, float f01,
                  Vec2f& p);

void GetMinInterior(const Vec2f& p0, float h0, const Vec2f& p1, float h1,
                    Vec2f& p);

TrigDistInfo PointTrigDist(const Vec3f& pt, float* trig) {
  Vec3f diff(pt[0] - trig[0], pt[1] - trig[1], pt[2] - trig[2]);
  Vec3f edge0(trig[3] - trig[0], trig[4] - trig[1], trig[5] - trig[2]);
  Vec3f edge1(trig[6] - trig[0], trig[7] - trig[1], trig[8] - trig[2]);
  float a00 = edge0.dot(edge0);
  float a01 = edge0.dot(edge1);
  float a11 = edge1.dot(edge1);
  float b0 = -(diff.dot(edge0));
  float b1 = -(diff.dot(edge1));

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
  if (f00 >= (float)0) {
    if (f01 >= (float)0) {
      // (1) p0 = (0,0), p1 = (0,1), H(z) = G(L(z))
      GetMinEdge02(a11, b1, p);
    } else {
      // (2) p0 = (0,t10), p1 = (t01,1-t01),
      // H(z) = (t11 - t10)*G(L(z))
      p0[0] = (float)0;
      p0[1] = f00 / (f00 - f01);
      p1[0] = f01 / (f01 - f10);
      p1[1] = (float)1 - p1[0];
      dt1 = p1[1] - p0[1];
      h0 = dt1 * (a11 * p0[1] + b1);
      if (h0 >= (float)0) {
        GetMinEdge02(a11, b1, p);
      } else {
        h1 = dt1 * (a01 * p1[0] + a11 * p1[1] + b1);
        if (h1 <= (float)0) {
          GetMinEdge12(a01, a11, b1, f10, f01, p);
        } else {
          GetMinInterior(p0, h0, p1, h1, p);
        }
      }
    }
  } else if (f01 <= (float)0) {
    if (f10 <= (float)0) {
      // (3) p0 = (1,0), p1 = (0,1), H(z) = G(L(z)) - F(L(z))
      GetMinEdge12(a01, a11, b1, f10, f01, p);
    } else {
      // (4) p0 = (t00,0), p1 = (t01,1-t01), H(z) = t11*G(L(z))
      p0[0] = f00 / (f00 - f10);
      p0[1] = (float)0;
      p1[0] = f01 / (f01 - f10);
      p1[1] = (float)1 - p1[0];
      h0 = p1[1] * (a01 * p0[0] + b1);
      if (h0 >= (float)0) {
        p = p0;  // GetMinEdge01
      } else {
        h1 = p1[1] * (a01 * p1[0] + a11 * p1[1] + b1);
        if (h1 <= (float)0) {
          GetMinEdge12(a01, a11, b1, f10, f01, p);
        } else {
          GetMinInterior(p0, h0, p1, h1, p);
        }
      }
    }
  } else if (f10 <= (float)0) {
    // (5) p0 = (0,t10), p1 = (t01,1-t01),
    // H(z) = (t11 - t10)*G(L(z))
    p0[0] = (float)0;
    p0[1] = f00 / (f00 - f01);
    p1[0] = f01 / (f01 - f10);
    p1[1] = (float)1 - p1[0];
    dt1 = p1[1] - p0[1];
    h0 = dt1 * (a11 * p0[1] + b1);
    if (h0 >= (float)0) {
      GetMinEdge02(a11, b1, p);
    } else {
      h1 = dt1 * (a01 * p1[0] + a11 * p1[1] + b1);
      if (h1 <= (float)0) {
        GetMinEdge12(a01, a11, b1, f10, f01, p);
      } else {
        GetMinInterior(p0, h0, p1, h1, p);
      }
    }
  } else {
    // (6) p0 = (t00,0), p1 = (0,t11), H(z) = t11*G(L(z))
    p0[0] = f00 / (f00 - f10);
    p0[1] = (float)0;
    p1[0] = (float)0;
    p1[1] = f00 / (f00 - f01);
    h0 = p1[1] * (a01 * p0[0] + b1);
    if (h0 >= (float)0) {
      p = p0;  // GetMinEdge01
    } else {
      h1 = p1[1] * (a11 * p1[1] + b1);
      if (h1 <= (float)0) {
        GetMinEdge02(a11, b1, p);
      } else {
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
  info.sqrDist = diff.dot(diff);
  return info;
}

void GetMinEdge02(float a11, float b1, Vec2f& p) {
  p[0] = (float)0;
  if (b1 >= (float)0) {
    p[1] = (float)0;
  } else if (a11 + b1 <= (float)0) {
    p[1] = (float)1;
  } else {
    p[1] = -b1 / a11;
  }
}

inline void GetMinEdge12(float a01, float a11, float b1, float f10, float f01,
                         Vec2f& p) {
  float h0 = a01 + b1 - f10;
  if (h0 >= (float)0) {
    p[1] = (float)0;
  } else {
    float h1 = a11 + b1 - f01;
    if (h1 <= (float)0) {
      p[1] = (float)1;
    } else {
      p[1] = h0 / (h0 - h1);
    }
  }
  p[0] = (float)1 - p[1];
}

inline void GetMinInterior(const Vec2f& p0, float h0, const Vec2f& p1, float h1,
                           Vec2f& p) {
  float z = h0 / (h0 - h1);
  p = ((float)1 - z) * p0 + z * p1;
}

void ComputeTrigFrame(const float* trig, const Vec3f& n, TrigFrame& frame)
{
  frame.x = Vec3f(trig[3] - trig[0], trig[4] - trig[1], trig[5] - trig[2]);
  float xlen = frame.x.norm();
  if (xlen > 0) {
    frame.x = (1.0f / xlen)*frame.x;
  } 
  frame.z = n;
  frame.y = frame.z.cross(frame.x);

  frame.v1x = xlen;
  Vec3f v2 = Vec3f(trig[6] - trig[0], trig[7] - trig[1], trig[8] - trig[2]);
  frame.v2x = v2.dot(frame.x);
  frame.v2y = v2.dot(frame.y);
}

TrigDistInfo PointTrigDist2D(float px, float py, float t1x, float t2x,
                             float t2y)
{
  TrigDistInfo info;  
  if (py <= 0) {
    //V0, V1, or E01
    if (px<=0) {
      info.type = TrigPointType::V0;
      info.sqrDist = px * px + py * py;
      return info;
    }
    if (px > t1x) {
      info.type = TrigPointType::V1;
      info.sqrDist = (px - t1x) * (px - t1x) + py * py;
      return info;
    }
    info.type = TrigPointType::E01;
    info.sqrDist = py * py;
    return info;
  }

  //test E12
  Vec2f n12(t2y, -t2x+t1x);
  Vec2f pv1(px - t1x, py);
  Vec2f pv2(px - t2x, py-t2y);
  Vec2f e12(t2x - t1x, t2y);
  float edgeDist = pv1.dot(n12);
  float eps = 1e-12;
  if (edgeDist >= 0) {
    if (pv1.dot(e12)<=0) {
      info.type = TrigPointType::V1;
      info.sqrDist = (px - t1x) * (px - t1x) + py * py;
      return info;
    }
    if (pv2.dot(e12) >= 0) {
      info.type = TrigPointType::V2;
      info.sqrDist = pv2.norm2();
      return info;
    }
    info.type = TrigPointType::E12;   
    info.sqrDist = edgeDist * edgeDist / (eps+n12.norm2());
    return info;
  }
  //testE02
  Vec2f n20(-t2y, t2x);
  edgeDist = pv2.dot(n20);
  if (edgeDist >= 0) {
    if (pv2.dot(Vec2f(t2x, t2y))>=0) {
      info.type = TrigPointType::V2;
      info.sqrDist = pv2.norm2();
      return info;
    }
    if (px*t2x+ py*t2y <= 0) {
      info.type = TrigPointType::V0;
      info.sqrDist = px*px+py*py;
      return info;
    }
    info.type = TrigPointType::E02;
    info.sqrDist = edgeDist * edgeDist / (eps + n20.norm2());
    return info;
  }
  //interior
  info.type = TrigPointType::FACE;
  info.sqrDist = 0;
  return info;
}