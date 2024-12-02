#include <filesystem>
#include <fstream>
#include <iostream>

#include "ArrayUtil.h"
#include "BBox.h"
#include "TrigMesh.h"
#include "Vec2.h"

struct Segment {
  virtual Vec2f eval(float t) const = 0;
  virtual float len() const = 0;
};

/// always picks the center with shorter arc.
struct Arc : public Segment{
  Vec2f x0;
  Vec2f x1;
  float radius = 1;
  Vec2f center;
  float theta0 = 0;
  float theta1 = 0;
  Vec2f eval(float t) const {
    float theta = theta0 + t / radius;
    Vec2f local(std::cos(theta),std::sin(theta));
    return center + radius * local;
  }
  float len() const { return radius * (theta1 - theta0); }
  Arc(Vec2f a, Vec2f b, float r) : x0(a), x1(b), radius(r) {
    Vec2f mid = 0.5f * (a + b);
    Vec2f ab = b - a;
    Vec2f midDir(-ab[1],ab[0]);
    float eps = 1e-20;
    float ab_len = ab.norm();
    if (ab_len < eps) {
      ab_len = eps;
    }
    midDir = (1.0f / ab_len) * midDir;
    float diff = r * r - 0.25f * ab_len * ab_len;
    float h = 0;
    if (diff >= 0) {
      h = std::sqrt(diff);
    }
    center = mid + h * midDir;
    Vec2f ao = a - center;
    theta0 = std::atan2(ao[1], ao[0]);
    Vec2f bo = b - center;
    theta1 = std::atan2(bo[1], bo[0]);
    if (theta1 < theta0) {
      theta1 += 2 * 3.14159265f;
    }
  }
};

struct Line : public Segment {
  Vec2f x0;
  Vec2f x1;
  Line(Vec2f a, Vec2f b) : x0(a), x1(b) {}
  Vec2f eval(float t) const {
    Vec2f dir = (x1 - x0);
    float eps = 1e-20;
    float n = dir.norm();
    if (n < eps) {
      n = eps;
    }
    dir = (1.0f / n) * dir;
    return x0 + t * dir;
  }
  float len() const override { return (x1 - x0).norm(); }
};

struct Curve {
  std::vector<std::unique_ptr<Segment>> segs;
  void Add(std::unique_ptr<Segment> s) { segs.push_back(std::move(s)); }
  void AddLine(Vec2f a, Vec2f b) {
    std::unique_ptr<Segment> s = std::make_unique<Line>(a, b);
    Add(std::move(s));
  }
  void AddArc(Vec2f a, Vec2f b, float r) {
    std::unique_ptr<Segment> s = std::make_unique<Arc>(a, b, r);
    Add(std::move(s));
  }
  std::vector<Vec2f> SamplePoints(float ds) {
    std::vector<Vec2f> points;
    for (size_t i = 0;i<segs.size();i++) {
      float l = segs[i]->len();
      float numSamples = l / ds + 1;
      for (size_t j = 0; j < numSamples; j++) {
        float t = (float(j + 0.5f) / numSamples) * l;
        points.push_back(segs[i]->eval(t));
      }
    }
    return points;
  }
};

struct Placement {
  float rot=0;
  Vec2f trans;
};

Vec2f ApplyRot(Vec2f v, float r) {
  float c = std::cos(r);
  float s = std::sin(r);
  Vec2f out;
  out[0] = c * v[0] - s * v[1];
  out[1] = s * v[0] + c * v[1];
  return out;
}

std::vector<Placement> PlaceTracks(Vec2f hole0, Vec2f hole1,
                                   const std::vector<Vec2f>& curve) {
  std::vector<Placement> p;
  unsigned i = 0;
  if (curve.size() < 2) {
    return p;
  }
  Vec2f p0 = curve[0];
  float eps = 0.05f;
  float trackWidth = (hole1 - hole0).norm();
  for (unsigned i = 1; i < curve.size(); i++) {
    Vec2f c1 = curve[i];
    float dist = (c1 - p0).norm();
    Vec2f p1;
    if (dist > trackWidth) {
      // approximate interpolation
      float prevDist = (curve[i - 1] - p0).norm();
      // weight for curve[i-1].
      float alpha = 1;
      if (prevDist < trackWidth) {
        alpha = (dist - trackWidth) / (dist - prevDist);
      }
      p1 = alpha * curve[i - 1] + (1 - alpha) * curve[i];      
      Vec2f p01 = p1 - p0;
      float r = std::atan2(p01[1], p01[0]);      
      Vec2f hole0Rot = ApplyRot(hole0, r);
      Vec2f tran = p0 - hole0Rot;
      p.push_back({r, tran});
      p0 = p1;

    }
  }
  return p;
}

void MakeCurve() { 
  //// tank versions 1-6
  //Curve c;

  //c.AddArc({-0.024, 19.119}, {-18.88, 4.494}, 19);
  //c.AddArc({-18.88, 4.494}, {-0.16, -19.4}, 19);
  //c.AddLine({-0.16, -19.4} ,{45, -32.66});
  //c.AddLine({45, -32.66}, {133, -31.16});
  //c.AddArc({133, -31.16}, {146.68, -26.222}, 19.4);
  //c.AddLine({146.68, -26.222}, {178.22, 1.7});
  //c.AddArc({178.22, 1.7}, {183.3, 7.7781}, 18.5);
  //c.AddArc({183.3, 7.7781}, {166, 33.3}, 18.5);
  //c.AddLine({166, 33.3} , {68.042, 33.8});
  //c.AddLine({68.042, 33.8}, {-0.024, 19.519});
  //auto points = c.SamplePoints(0.1f);
  // tank version 7
  Curve c;
  c.AddLine({196,17}, {0,17});
  c.AddArc({0,17}, {-18.55,4.14}, 18.5);
  c.AddArc({-18.55, 4.14}, {0, -19}, 18.5);
  c.AddLine({0, -19}, {205, -19});
  auto points = c.SamplePoints(0.1f);
  std::ofstream out("F:/meshes/tank/place_tracks.txt");
  std::ofstream ptOut("F:/dump/trackCurve.txt");
  for (size_t i = 0; i < points.size(); i++) {
    ptOut << points[i][0] << " " << points[i][1] << "\n";
  }
  ptOut.close();
  //Vec2f hole0(-3.4157, 0.53905);
  //Vec2f hole1(5.0823, 0.53937);
  Vec2f hole1(2.674, -0.38);
  Vec2f hole0(-5.815, -0.38);
  std::reverse(points.begin(), points.end());
  std::vector<Placement> placement = PlaceTracks(hole0, hole1, points);
  for (size_t i = 0; i < placement.size(); i++) {
    const auto& p = placement[i];
    out << p.trans[0] << " " << p.trans[1] << " " << p.rot << "\n";
  }
}
