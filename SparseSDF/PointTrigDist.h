// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2021
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 4.0.2019.08.13

#pragma once

#include "Vec2.h"
#include "Vec3.h"

struct TrigDistInfo
{
	///squared distance
	float sqrDist;
	Vec3f bary;
	Vec3f closest;
};

//computes squared point triangle distance.
TrigDistInfo PointTrigDist(const Vec3f& pt, float* trig);

// v0 v1 is x axis. v0 is origin.
// v1y = 0.
// normal is z
void TriangleFrame(const float* trig, const Vec3f& n, Vec3f& x, Vec3f& y,
                   Vec3f& z);

TrigDistInfo PointTrigDist2D(float px, float py, float t1x, float t2x,float t2y);
