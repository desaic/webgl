#ifndef FONT_H
#define FONT_H
#include <stdint.h>
#include <vector>
struct glyph_dsc_t{
	uint32_t w_px;
	uint32_t glyph_index;
};

struct FontGlyph {
  std::vector<uint8_t> bitmap;
  /// width the glyph occupies.
  /// glyph is centered in bitmap
  uint32_t actualWidth;
  /// width of the bitmap
  uint32_t width;
  uint32_t height;
  uint8_t operator()(uint32_t x, uint32_t y)const;
  FontGlyph() :actualWidth(0),width(0), height(0) {}
};

struct font_t{
	 uint32_t unicode_first ;	/*First Unicode letter in this font*/
     uint32_t unicode_last ;	/*Last Unicode letter in this font*/
     uint32_t h_px;				/*Font height in pixels*/
     const uint8_t * glyph_bitmap ;	/*Bitmap of glyphs*/
     const struct glyph_dsc_t* glyph_dsc ;		/*Description of glyphs*/
     uint32_t glyph_cnt ;			/*Number of glyphs in the font*/
     FontGlyph GetBitmap(char c);
};
#endif