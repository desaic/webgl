#include "SDFVoxCb.h"
#include <iostream>

void TrigListVoxCb::operator()(unsigned x, unsigned y, unsigned z, unsigned long long trigIdx) {
  Vec3u gridIdx(x, y, z);
  unsigned cellIdx = df->AddDenseCell(gridIdx);
  std::vector<std::vector<size_t> >& trigList = df->trigList;
  if (cellIdx == trigList.size()) {
    trigList.push_back(std::vector<size_t>());
  } else if (cellIdx > trigList.size()) {
    std::cout << "bug\n";
    return;
  }
  trigList[cellIdx].push_back(trigIdx);
}