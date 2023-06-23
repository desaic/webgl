#pragma once

#include "BitArray.h"
#include <string>
#include <sstream>
#include <vector>

///asymptotically unsuitable for large sets.
template <typename ValueT>
class SparseSet
{
public:
  
  void Allocate(size_t s) {
    mask.Resize(s);
  }

  ///assumes that the spot exists. otherwise use AddValue.
  void SetValue(unsigned idx, ValueT & v)
  {
    unsigned position = mask.CountOnes(idx);
    val[position - 1] = v;
  }

  ///assumes that the spot does not exist. otherwise use SetValue.
  void AddValue(unsigned idx, ValueT& v)
  {
    unsigned position = mask.CountOnes(idx);
    if (mask.GetBit(idx)) {
      val[position - 1] = v;
    }
    else {
      mask.SetBit(idx, 1);
      val.insert(val.begin() + position, v);
    }
  }

  bool HasValue(unsigned idx)
  {
    return mask.GetBit(idx);
  }

  ValueT GetValue(unsigned idx)
  {
    bool hasVal = mask.GetBit(idx);
    if (!hasVal) {
      return ValueT(0);
    }
    unsigned pos = mask.CountOnes(idx) - 1;
    return val[pos];
  }

  unsigned GetNumValues() const { return unsigned(val.size()); }

  std::string ToString() {
    std::ostringstream ss;
    ss << val.size() << "\n";
    for (unsigned i = 0; i < val.size(); i++) {
      ss << val[i] << " ";
    }
    ss << "\n";
    ss << mask.ToString();
    return ss.str();
  }

  std::vector<ValueT> val;
  BitArray mask;

};

template <>
inline std::string SparseSet<unsigned char>::ToString() {
  std::ostringstream ss;
  ss << val.size() << "\n";
  for (unsigned i = 0; i < val.size(); i++) {
    ss << int(val[i]) << " ";
  }
  ss << "\n";
  ss << mask.ToString();
  return ss.str();
}
