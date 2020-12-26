#include <string>
#include <fstream>
#include <vector>
#include "FileUtil.h"
void writeIntVec(const std::string filename, const std::vector<int>&vec) {
    std::ofstream out(filename, std::ofstream::binary);
    size_t s = vec.size();
    out.write((char*)&s, sizeof(size_t));
    out.write((char*)vec.data(), s * sizeof(int));
    out.close();
}

void readIntVec(const std::string filename, std::vector<int>& vec)
{
    std::ifstream in(filename, std::ofstream::binary);
    size_t s = 0;
    in.read((char*)&s, sizeof(size_t));
    vec.resize(s);
    in.read((char*)vec.data(), s * sizeof(int));
    in.close();
}
