#pragma once

#include "Array2D.h"
#include "Vec4.h"
#include <cmath>

bool Inbound(const Vec2u& size, int x, int y);

void DrawLine(Array2D4b& img, Vec2f p0, Vec2f p1, Vec4b color);
