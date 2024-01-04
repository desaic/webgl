#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace FileWriter {
void writeSTL(const std::string& filename, 
              const std::vector<float>& vertices, 
              const std::vector<uint32_t>& indices) {

    std::ofstream file(filename, std::ios::binary);

    if (!file) {
        std::cout << "Unable to open file: " << filename << std::endl;
        return;
    }

    // Write STL header
    char header[80] = {};
    file.write(header, 80);

    // Number of triangles
    uint32_t numTriangles = indices.size() / 3;
    file.write(reinterpret_cast<const char*>(&numTriangles), sizeof(uint32_t));

    // Write triangles
    for (size_t i = 0; i < indices.size(); i += 3) {
        // Normal vector (dummy values)
        float normal[3] = {0.0f, 0.0f, 0.0f};
        file.write(reinterpret_cast<const char*>(normal), 3 * sizeof(float));

        // Vertices of the triangle
        for (int j = 0; j < 3; ++j) {
            size_t vertexIndex = indices[i + j] * 3;
            file.write(reinterpret_cast<const char*>(&vertices[vertexIndex]), 3 * sizeof(float));
        }

        uint16_t attributeByteCount = 0;
        file.write(reinterpret_cast<const char*>(&attributeByteCount), sizeof(uint16_t));
    }

    file.close();
}

}