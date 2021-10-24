#pragma once

#include "ConfigFile.hpp"

void computeSurface(const ConfigFile & conf);
unsigned short findDepthConv(float* aline, uint32_t size);
unsigned short findDepth(unsigned char* aline, uint32_t size);
void findDepth();