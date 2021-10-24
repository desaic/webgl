#pragma once

#include <string>
#include <vector>

class BitArray {
public:
  BitArray();
  
  bool GetBit(unsigned idx) const;
  void SetBit(unsigned idx, bool val);
  void Resize(size_t s);
  //count number of '1's from bits 0 to idx inclusive.
  unsigned CountOnes(unsigned idx)const;
  unsigned NumBits()const;
  std::vector<int64_t> v;

  std::string ToString();
};
