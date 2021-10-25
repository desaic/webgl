#include "Array2D.h"
#include <string>

int LoadPng(const std::string& filename, Array2D8u& slice);
int SavePng(const std::string& filename, Array2D8u& slice);