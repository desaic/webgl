#include "SDFVoxCb.h"

#include <iostream>

void TrigListVoxCb::operator()(unsigned x, unsigned y, unsigned z,
                               size_t trigIdx) {
  Vec3u gridIdx(x, y, z);
  unsigned cellIdx = sdf->AddDenseCell(gridIdx);
  std::vector<std::vector<size_t> >& trigList = sdf->trigList;

  if (cellIdx == trigList.size()) {
    trigList.push_back(std::vector<size_t>());
  } else if (cellIdx > trigList.size()) {
    std::cout << "bug TrigListVoxCb::operator()\n";
  }
  trigList[cellIdx].push_back(trigIdx);
}
