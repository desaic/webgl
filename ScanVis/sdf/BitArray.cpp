#include "BitArray.h"
#include <intrin.h>
#include <sstream>

//number of bits in int64_t duh
#define INT64_BITS 64
#define INT64_MAX_SHIFT 63

inline uint32_t CountOn(uint64_t v);
inline uint32_t CountOn(uint32_t v);

BitArray::BitArray()
{

}

bool BitArray::GetBit(unsigned idx) const
{
  unsigned vIdx = idx / INT64_BITS;
  unsigned bitIdx = idx - vIdx * INT64_BITS;
  return (v[vIdx] & (1LL << bitIdx));
}

void BitArray::SetBit(unsigned idx, bool val)
{
  unsigned vIdx = idx / INT64_BITS;
  unsigned bitIdx = idx - vIdx * INT64_BITS;
  if (val) {
    v[vIdx] |= (1LL << bitIdx);
  } else {
    v[vIdx] &= (~(1LL << bitIdx));
  }
}

void BitArray::Resize(size_t s)
{
  size_t vecSize = s / INT64_BITS + (s % INT64_BITS > 0);
  v.resize(vecSize, 0);
}

unsigned BitArray::NumBits() const {
  return v.size() * INT64_BITS;
}

unsigned BitArray::CountOnes (unsigned idx)const
{
  unsigned intIdx = unsigned( idx / INT64_BITS);
  unsigned count = 0;
  for (unsigned i = 0; i < intIdx; i++) {
    count += CountOn(uint64_t(v[i]));
  }
  unsigned bitIdx = idx - unsigned(intIdx) * INT64_BITS;
  count += CountOn(uint64_t(v[intIdx] & ( (~0ULL) >> (INT64_MAX_SHIFT - bitIdx) ) ) );
  return count;
}

std::string BitArray::ToString()
{
  std::ostringstream out;
  out << v.size() << " ";
  for (unsigned i = 0; i < v.size(); i++) {
    out << v[i];
    if (i != v.size() - 1) {
      out << " ";
    }
  }
  return out.str();
}

inline uint32_t
CountOn(uint64_t v)
{
#if defined(_MSC_VER) && defined(_M_X64)
  v = __popcnt64(v);
#elif (defined(__GNUC__) || defined(__clang__))
  v = __builtin_popcountll(v);
#else
  // Software Implementation
  v = v - ((v >> 1)& UINT64_C(0x5555555555555555));
  v = (v & UINT64_C(0x3333333333333333)) + ((v >> 2)& UINT64_C(0x3333333333333333));
  v = (((v + (v >> 4))& UINT64_C(0xF0F0F0F0F0F0F0F))* UINT64_C(0x101010101010101)) >> 56;
#endif
  return static_cast<uint32_t>(v);
}

inline uint32_t
CountOn(uint32_t v)
{
  v = v - ((v >> 1) & 0x55555555U);
  v = (v & 0x33333333U) + ((v >> 2) & 0x33333333U);
  return (((v + (v >> 4)) & 0xF0F0F0FU) * 0x1010101U) >> 24;
}
