#include "ImageUtils.h"

bool Inbound(const Vec2u& size, int x, int y) {
  return x >= 0 && x < size[0] && y >= 0 && y < size[1];
}

void DrawLine(Array2D4b& img, Vec2f p0, Vec2f p1, Vec4b color)
{
  int x0 = int(p0[0]);
  int x1 = int(p1[0]);
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int y0 = int(p0[1]);
  int y1 = int(p1[1]);
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  Vec2u size = img.GetSize();
  while (1) {
    if (Inbound(size, x0, y0)) {
      img(x0, y0) = color;
    }
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int e2 = 2 * error;
    if (e2 >= dy) {
      if (x0 == x1) {
        break;
      }
      error += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      if (y0 == y1) {
        break;
      }
      error += dx;
      y0 += sy;
    }
  }
}
