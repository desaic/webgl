#include "ImageIO.h"
#include "lodepng.h"

#include <filesystem>
#include <iostream>
#include <fstream>

int LoadPng(const std::string& filename, Array2D8u& slice)
{
  std::filesystem::path p(filename);
  if (!(std::filesystem::exists(p) && std::filesystem::is_regular_file(p))) {
    return -1;
  }
  std::vector<unsigned char> buf;
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  unsigned error = lodepng::load_file(buf, filename.c_str());
  if (error) {
    std::cout << "lodepng load_file error " << error << ". file name " << filename << "\n";
    return -2;
  }
  unsigned w, h;
  slice.GetData().clear();
  error = lodepng::decode(slice.GetData(), w, h, state, buf);
  if (error) {
    std::cout << "decode error " << error << "\n";
    return -3;
  }
  slice.SetSize(w, h);
  return 0;
}

int SavePng(const std::string& filename, Array2D8u& slice)
{
  std::ofstream out(filename, std::ofstream::binary);
  if (!out.good()) {
    return -1;
  }
  std::vector<unsigned char> buf;
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  state.encoder.auto_convert = 0;
  std::vector<uint8_t> outbuf;
  Vec2u size = slice.GetSize();
  int error = lodepng::encode(outbuf, slice.GetData(), size[0], size[1],
    state);
  if (error) {
    std::cout << "encode png error " << error << "\n";
    return -3;
  }
  out.write((const char *)outbuf.data(), outbuf.size());
  return 0;
}
