#pragma once
#include <string>
#include <vector>
#include <DualPixel.hpp>
int SavePrintVolume(std::string filename, std::vector<DualPixel> & vol, int * volSize);
//identical to SavePrintVolume except argument type.
int SaveVolume(std::string filename, std::vector<uint8_t> & vol, int * volSize);
int LoadPrintVolume(std::string filename, std::vector<DualPixel> & vol, int * volSize);
///identical to LoadPrintVolume except argument type.
///the entire input volume is compressed with a single call to lz4.
bool LoadVolume(std::string filename, std::vector<uint8_t> & vol, int * volSize);