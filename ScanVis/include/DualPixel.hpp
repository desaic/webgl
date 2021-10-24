#pragma once

#ifdef WIN32
#pragma pack(push, 1)
#endif
struct
#if !defined(WIN32)
  __attribute__((__packed__))
#endif
  DualPixel {
  DualPixel() {}

  DualPixel(int p1, int p2) :
    pixel2(p2),
    pixel1(p1)
  {
  }
  DualPixel(int p) :
    pixel2(p),
    pixel1(p)
  {
  }
  // Watch for reverse endian storage; this is why pixel2 comes before pixel1
  uint8_t pixel2 : 4;
  uint8_t pixel1 : 4;
};
#ifdef WIN32
#pragma pack(pop)
#endif