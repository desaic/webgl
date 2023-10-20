#pragma once
#include <string>
#include "Array2D.h"
int LoadPngColor(const std::string& filename, Array2D8u& slice);
int LoadPngGrey(const std::string& filename, Array2D8u& arr);
void EncodeBmpGray(const unsigned char* img, unsigned int width,
                   unsigned int height, std::vector<unsigned char>& out);