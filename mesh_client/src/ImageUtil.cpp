#include "lodepng.h"
#include "Array2D.h"
#include "ImageUtil.h"
#include <fstream>

int SavePng(const std::string& filename, Array2D8u& slice)
{
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  state.encoder.auto_convert = 0;
  std::vector<unsigned char> out;
  Vec2u size = slice.GetSize();
  lodepng::encode(out, slice.GetData(), size[0], size[1], state);
  std::ofstream outFile(filename, std::ofstream::out | std::ofstream::binary);
  outFile.write((const char*)out.data(), out.size());
  outFile.close();
  return 0;
}
