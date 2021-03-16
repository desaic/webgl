// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2021
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 4.0.2019.08.13

#pragma once

#include "Vec2.h"
#include "Vec3.h"

void GetMinEdge02(float & a11, float & b1, Vec2f& p);

void GetMinEdge12(float a01, float a11, float b1,
	float f10, float f01, Vec2f & p);

void GetMinInterior(const Vec2f& p0, float h0,
	const Vec2f& p1, float h1, Vec2f& p);

//computes signed point triangle distance.
//sign is determined by the trig normal.
float PointTrigDist(Vec3f& pt, float* trig);
