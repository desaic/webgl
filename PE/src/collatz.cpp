#include "Array2D.h"
#include "lodepng.h"
#include <iostream>

unsigned nextNum(unsigned num) {
  if (num % 2 == 0) {
    num /= 2;
  }
  else {
    num = (3 * num + 1) / 2;
  }
  return num;
}

void saveArray8u(Array2D8u & img) {
  std::vector <unsigned char> out;
  lodepng::State s;
  s.info_raw.bitdepth = 8;
  s.info_raw.colortype = LCT_GREY;
  unsigned error = lodepng::encode(out, img.GetData(), img.GetSize()[0], img.GetSize()[1], s);
  if (!error) {
    lodepng::save_file(out, "out.png");
  }
}

void collatz()
{
  int w = 800;
  int h = 600;
  Array2D8u img(800, 600);
  img.Fill(138);
  unsigned num0 = 100;

  for (int x = 0; x < w; x++) {
    unsigned num = num0 + x;
    while (num > 1) {
      num = nextNum(num);
      if (num < img.GetSize()[1]) {
        img(x, num) = 255;
      }
    }
  }

  saveArray8u(img);
}