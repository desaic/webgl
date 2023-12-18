#pragma once

#include <string>
#include <vector>

#include "Vec2.h"

int LoadOBJFile(std::string filename, std::vector<uint32_t>& t,
                std::vector<float>& v, std::vector<Vec2f>& uv);
