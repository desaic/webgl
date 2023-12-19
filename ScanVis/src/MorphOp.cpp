#include "MorphOp.h"

#include <immintrin.h>

#include <deque>
#include <execution>
#include <functional>
#include <iostream>
#include <numeric>

/// set all non-print overly-bright pixels to 0 height.
void ApplyPrintMask(uint8_t* v, size_t vlen, const uint8_t* mask, size_t masklen) {
  uint8_t HEIGHT_UPPER_BOUND = 220;

  size_t len = vlen;
  if (masklen < vlen) {
    len = masklen;
  }
  for (size_t i = 0; i < len; i++) {
    if (mask[i] == 0 && v[i] > HEIGHT_UPPER_BOUND) {
      v[i] = 0;
    }
  }

  for (size_t i = len; i < vlen; i++) {
    if (v[i] > HEIGHT_UPPER_BOUND) {
      v[i] = 0;
    }
  }
}

//////////////////////
//  AVX2 implementation
//////////////////////
// number of bytes avx2 can handle at a time.
const size_t NUM_LANES = 32;

class VecUpdateMax {
 public:
  static uint8_t Op(uint8_t a, uint8_t b) { return std::max(a, b); }
  static __m256i VecOp(__m256i a, __m256i b) { return _mm256_max_epu8(a, b); }
};

class VecUpdateMin {
 public:
  static uint8_t Op(uint8_t a, uint8_t b) { return std::min(a, b); }
  static __m256i VecOp(__m256i a, __m256i b) { return _mm256_min_epu8(a, b); }
};

// normal dilation
template <class VecUpdate>
void MorphInPlaceRad1(uint8_t* v, size_t len) {
  if (len < 2) {
    return;
  }
  uint8_t prevVal = v[0];
  v[0] = VecUpdate::Op(v[0], v[1]);
  v[len - 1] = VecUpdate::Op(v[len - 1], v[len - 2]);

  for (size_t i = 1; i < len - 1; i++) {
    uint8_t val = VecUpdate::Op(v[i], prevVal);
    val = VecUpdate::Op(val, v[i + 1]);
    prevVal = v[i];
    v[i] = val;
  }
}

template <class VecUpdate>
void MorphVecAVX2Rad1(uint8_t* v, size_t len) {
  if (len < 2) {
    return;
  }
  if (len < NUM_LANES + 2) {
    // normal dilation
    MorphInPlaceRad1<VecUpdate>(v, len);
    return;
  }
  // 256 bits. -2 for padding.
  size_t nChunks = (len - 2) / NUM_LANES;
  __m256i left, mid, right;
  // last chunk handled outside of loop for the remainder.
  __m256i lastm;

  left = _mm256_loadu_si256((const __m256i*)(v));
  // handle first pixel.
  v[0] = VecUpdate::Op(v[0], v[1]);

  size_t lastIdx = len - 1 - NUM_LANES;
  {
    __m256i lastl, lastr;
    lastl = _mm256_loadu_si256((const __m256i*)(v + lastIdx - 1));
    lastm = _mm256_loadu_si256((const __m256i*)(v + lastIdx));
    lastr = _mm256_loadu_si256((const __m256i*)(v + lastIdx + 1));
    lastm = VecUpdate::VecOp(lastm, lastl);
    lastm = VecUpdate::VecOp(lastm, lastr);
  }

  for (size_t c = 0; c < nChunks; c++) {
    //+1 for padding on the left
    size_t i = 1 + c * NUM_LANES;
    mid = _mm256_loadu_si256((const __m256i*)(v + i));
    right = _mm256_loadu_si256((const __m256i*)(v + i + 1));
    mid = VecUpdate::VecOp(mid, left);
    mid = VecUpdate::VecOp(mid, right);
    if (c + 1 < nChunks) {
      // load left of next chunk before writing mid.
      left = _mm256_loadu_si256((const __m256i*)(v + i + NUM_LANES - 1));
    }
    _mm256_storeu_si256((__m256i*)(v + i), mid);
  }
  // handle last pixel.
  v[len - 1] = VecUpdate::Op(v[len - 1], v[len - 2]);

  // now write last chunk
  _mm256_storeu_si256((__m256i*)(v + lastIdx), lastm);
}

template <class VecUpdate>
void MorphVecAVX2(uint8_t* v, size_t len, int rad) {
  if (len == 0 || rad <= 0) {
    return;
  }
  for (int r = 0; r < rad; r++) {
    MorphVecAVX2Rad1<VecUpdate>(v, len);
  }
}

/// <param name="up">mid is saved in up before its values are updated.
/// so that the next row can dilate correctly using old values.</param>
/// <param name="len"></param>
template <class VecUpdate>
void MorphYAVX2Rad1(uint8_t* up, uint8_t* mid, const uint8_t* down, size_t len) {
  // 256 bits
  size_t nChunks = len / NUM_LANES;
  size_t remainder = len - NUM_LANES * nChunks;
  __m256i m1, m2, m3;

  if (len < 32) {
    // normal y dilation
    for (size_t i = 0; i < len; i++) {
      uint8_t val = VecUpdate::Op(up[i], mid[i]);
      // save original pixel value for the next row.
      up[i] = mid[i];
      mid[i] = VecUpdate::Op(val, down[i]);
    }
  }

  // temp variable for remainder chunk
  __m256i lastm, lastu;
  size_t lastIdx = len - NUM_LANES;
  if (remainder > 0) {
    lastu = _mm256_loadu_si256((const __m256i*)(up + lastIdx));
    lastm = _mm256_loadu_si256((const __m256i*)(mid + lastIdx));
  }

  for (size_t i = 0; i <= len - NUM_LANES; i += NUM_LANES) {
    m1 = _mm256_loadu_si256((const __m256i*)(up + i));
    m2 = _mm256_loadu_si256((const __m256i*)(mid + i));
    m3 = _mm256_loadu_si256((const __m256i*)(down + i));
    // save mid value in up
    _mm256_storeu_si256((__m256i*)(up + i), m2);

    m2 = VecUpdate::VecOp(m2, m1);
    m2 = VecUpdate::VecOp(m2, m3);
    _mm256_storeu_si256((__m256i*)(mid + i), m2);
  }
  if (remainder == 0) {
    return;
  }
  __m256i lastd = _mm256_loadu_si256((const __m256i*)(down + lastIdx));
  // save mid value in up
  _mm256_storeu_si256((__m256i*)(up + lastIdx), lastm);

  lastm = VecUpdate::VecOp(lastm, lastu);
  lastm = VecUpdate::VecOp(lastm, lastd);
  _mm256_storeu_si256((__m256i*)(mid + lastIdx), lastm);
}

template <class VecUpdate>
void MorphOp(Array2D8u& scan, int dx, int dy) {
  Vec2u size = scan.GetSize();
  for (unsigned row = 0; row < size[1]; row++) {
    MorphVecAVX2<VecUpdate>(scan.DataPtr() + row * size[0], size[0], dx);
  }

  std::vector<uint8_t> tempRow(size[0]);
  std::memcpy(tempRow.data(), scan.DataPtr(), size[0]);
  for (int rady = 0; rady < dy; rady++) {
    for (unsigned row = 0; row < size[1]; row++) {
      uint8_t *rowMid, *rowBot;
      rowMid = scan.DataPtr() + row * size[0];
      if (row + 1 < size[1]) {
        rowBot = rowMid + size[0];
      } else {
        rowBot = rowMid;
      }
      MorphYAVX2Rad1<VecUpdate>(tempRow.data(), rowMid, rowBot, size[0]);
    }
  }
}

void Dilate(Array2D8u& scan, int dx, int dy) { MorphOp<VecUpdateMax>(scan, dx, dy); }

void Erode(Array2D8u& scan, int dx, int dy) { MorphOp<VecUpdateMin>(scan, dx, dy); }

////////////////////////////////////////////
/// Special printing-related image operations
////////////////////////////////////////////
void MaskBrightSpots(Array2D8u* scan, Vec2i margin, const Array2D8u& slice) {
  Vec2u scanSize = scan->GetSize();
  Vec2u sliceSize = slice.GetSize();
  // very tall height in scan.
  const uint8_t HIGH_VAL = 220;
  // some good amount below print plane height
  // to not affect print area.
  const uint8_t REPLACE_VAL = 90;
  unsigned row0 = unsigned(margin[1]);
  unsigned col0 = unsigned(margin[0]);

  for (unsigned row = row0; row < scanSize[1]; row++) {
    unsigned sliceRow = row - row0;
    uint8_t* rowPtr = scan->DataPtr() + row * scanSize[0];
    if (sliceRow >= int(sliceSize[1])) {
      for (unsigned col = 0; col < scanSize[0]; col++) {
        if (rowPtr[col] > HIGH_VAL) {
          rowPtr[col] = REPLACE_VAL;
        }
      }
      continue;
    }
    const uint8_t* slicePtr = slice.DataPtr() + sliceSize[0] * size_t(sliceRow);

    for (unsigned col = col0; col < scanSize[0]; col++) {
      unsigned sliceCol = col - col0;
      if (sliceCol < sliceSize[0]) {
        uint8_t mat = slicePtr[sliceCol];
        if (mat > 0) {
          continue;
        }
      }
      if (rowPtr[col] > HIGH_VAL) {
        rowPtr[col] = REPLACE_VAL;
      }
    }
  }
}
