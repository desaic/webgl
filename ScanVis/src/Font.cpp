#include "Font.h"

#include <stdint.h>
#include <vector>
#include <assert.h>

uint8_t FontGlyph ::operator()(uint32_t x, uint32_t y)const {
  assert(x < width);
  assert(y < height);
  uint32_t byteIdx = x / 8;
  uint8_t bitIdx = x % 8;
  uint8_t b = bitmap[y * width / 8 + byteIdx];
  return (b >> (7 - bitIdx)) & 1;
}

FontGlyph font_t::GetBitmap(char c)
{
  FontGlyph glyph;
  if (c<unicode_first || c>unicode_last) {
    return glyph;
  }
  struct glyph_dsc_t desc = glyph_dsc[c - unicode_first];
  glyph.actualWidth = desc.w_px;
  glyph.height = h_px;
  glyph.width = desc.w_px;
  if (glyph.width % 8 > 0) {
    glyph.width += 8 - (glyph.width % 8);
  }
  uint32_t numPix = glyph.height * glyph.width;
  uint32_t numBytes = numPix / 8;
  glyph.bitmap.resize(numBytes);
  for (size_t i = 0; i < numBytes; i++) {
    glyph.bitmap[i] = glyph_bitmap[desc.glyph_index + i];
  }
  return glyph;
}