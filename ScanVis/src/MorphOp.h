#pragma once
#include "Array2D.h"

void Dilate(Array2D8u& scan, int dx, int dy);
void Erode(Array2D8u& scan, int dx, int dy);

/// Prepare depth image for dilation by removing bright spots
/// in the back ground so that they don't dilate into printed parts.
/// @param margin scan images have some pixels of margin around its borders.
void MaskBrightSpots(Array2D8u* scan, Vec2i margin, const Array2D8u& slice);