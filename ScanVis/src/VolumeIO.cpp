#include "VolumeIO.hpp"
#include "lz4.h"
#include "FileUtil.hpp"
#include <fstream>

//lz4 calls use integers as parameters, which means they can only compresse up to 2Gb data.
//This function divides data into chunks and calls lz4 separate to get around this limit.
size_t splitCompress(std::vector<char> & compressed,
  std::vector<int> &chunkSizes,
  const char * buf, size_t bufSize) {
  const int CHUNK_SIZE = 1 << 24;
  //[startIdx, endIdx)
  size_t startIdx = 0, endIdx = CHUNK_SIZE;
  int nChunk = bufSize / CHUNK_SIZE;
  if (nChunk < 1) {
    nChunk = 1;
  }
  chunkSizes.resize(nChunk * 2, 0);
  //the last chunk can contain more than CHUNK_SIZE data but that's fine.
  size_t totalBytes = 0;
  for (int c = 0; c < nChunk; c++) {
    startIdx = c * CHUNK_SIZE;
    endIdx = startIdx + CHUNK_SIZE;
    if (endIdx + CHUNK_SIZE > bufSize) {
      //this is the last chunk. allow it to be bigger.
      endIdx = bufSize;
    }
    int chunkSize = endIdx - startIdx;
    int dstSize = LZ4_compressBound(chunkSize);
    if (dstSize < 0) {
      //lz4 lib failed.
      return 0;
    }
    if (totalBytes + dstSize > compressed.size()) {
      compressed.resize(totalBytes + dstSize);
    }
    chunkSizes[2 * c] = chunkSize;
    int compressedSize = LZ4_compress_default((const char*)(buf + startIdx),
      (char*)(compressed.data() + totalBytes),
      chunkSize, compressed.size() - totalBytes);
    chunkSizes[2 * c + 1] = compressedSize;
    totalBytes += compressedSize;
  }
  return totalBytes;
}

int SavePrintVolume(std::string filename, std::vector<DualPixel> & vol, int * volSize) {
  //save volume as 3d array. three 4-byte integers followed by a list of uint8_t
  size_t outSize = volSize[0] * volSize[1] * (size_t)volSize[2];
  std::ofstream out(filename, std::ofstream::binary);
  if (!out.good()) {
    //open file failed.
    return -1;
  }
  out.write((const char *)(volSize), 3 * sizeof(int));
  std::vector<char> compressed;
  std::vector<int> chunkSizes;
  size_t compressedSize = splitCompress(compressed, chunkSizes, (const char*)vol.data(), vol.size());
  if (compressedSize == 0) {
    //compression failed.
    return -2;
  }
  int nChunk = int(chunkSizes.size());
  out.write((const char *)(&nChunk), sizeof(nChunk));
  out.write((const char *)chunkSizes.data(), sizeof(int) * nChunk);
  out.write((const char *)&compressedSize, sizeof(size_t));
  out.write((const char *)compressed.data(), compressedSize);
  out.close();
  return 0;
}

int SaveVolume(std::string filename, std::vector<uint8_t> & vol, int * volSize) {
  size_t outSize = volSize[0] * volSize[1] * (size_t)volSize[2];
  std::ofstream out(filename, std::ofstream::binary);
  if (!out.good()) {
    //open file failed.
    return -1;
  }
  out.write((const char *)(volSize), 3 * sizeof(int));
  std::vector<char> compressed;
  std::vector<int> chunkSizes;
  size_t compressedSize = splitCompress(compressed, chunkSizes, (const char*)vol.data(), vol.size());
  if (compressedSize == 0) {
    //compression failed.
    return -2;
  }
  int nChunk = int(chunkSizes.size());
  out.write((const char *)(&nChunk), sizeof(nChunk));
  out.write((const char *)chunkSizes.data(), sizeof(int) * nChunk);
  out.write((const char *)&compressedSize, sizeof(size_t));
  out.write((const char *)compressed.data(), compressedSize);
  out.close();
  return 0;
}

int LoadPrintVolume(std::string filename, std::vector<DualPixel> & vol, int * volSize)
{
  std::ifstream in(filename, std::ifstream::binary);
  if (!in.good()) {
    //open file failed.
    return -1;
  }
  in.read((char *)(volSize), 3 * sizeof(int));
  int nChunk = 0;
  in.read((char *)(&nChunk), sizeof(nChunk));
  std::vector<int> chunkSizes(nChunk);
  in.read((char *)chunkSizes.data(), sizeof(int) * nChunk);
  std::vector<char> compressed;
  size_t compressedSize;
  in.read((char *)&compressedSize, sizeof(size_t));
  compressed.resize(compressedSize);
  in.read((char*)compressed.data(), compressedSize);
  in.close();
  size_t offset = 0;
  size_t bufSize = 0;
  for (int c = 0; c < chunkSizes.size(); c += 2) {
    bufSize += chunkSizes[c];
  }
  vol.resize(bufSize);
  size_t compressedOffset = 0;
  for (int c = 0; c < chunkSizes.size(); c += 2) {

    const int decompressed_size = LZ4_decompress_safe((const char*)(&compressed[compressedOffset]),
      (char*)(&vol[offset]), chunkSizes[c + 1], chunkSizes[c]);
    compressedOffset += chunkSizes[c + 1];
    offset += chunkSizes[c];
  }
  return 0;
}


int LoadPrintVolume(std::string filename, std::vector<uint8_t> & vol, int * volSize)
{
  std::ifstream in(filename, std::ifstream::binary);
  if (!in.good()) {
    //open file failed.
    return -1;
  }
  in.read((char *)(volSize), 3 * sizeof(int));
  int nChunk = 0;
  in.read((char *)(&nChunk), sizeof(nChunk));
  std::vector<int> chunkSizes(nChunk);
  in.read((char *)chunkSizes.data(), sizeof(int) * nChunk);
  std::vector<char> compressed;
  size_t compressedSize;
  in.read((char *)&compressedSize, sizeof(size_t));
  compressed.resize(compressedSize);
  in.read((char*)compressed.data(), compressedSize);
  in.close();
  size_t offset = 0;
  size_t bufSize = 0;
  for (int c = 0; c < chunkSizes.size(); c += 2) {
    bufSize += chunkSizes[c];
  }
  vol.resize(bufSize);
  size_t compressedOffset = 0;
  for (int c = 0; c < chunkSizes.size(); c += 2) {
    const int decompressed_size = LZ4_decompress_safe((const char*)(&compressed[compressedOffset]),
      (char*)(&vol[offset]), chunkSizes[c + 1], chunkSizes[c]);
    compressedOffset += chunkSizes[c + 1];
    offset += chunkSizes[c];
  }
  return 0;
}

bool LoadVolume(std::string filename, std::vector<uint8_t> & vol, int * volSize) {
  FileUtilIn in;
  in.open(filename, std::ifstream::binary);
  if (!in.good()) {
    return false;
  }
  in.in.read((char*)(volSize), 3 * sizeof(int));
  int compressedSize;
  in.in.read((char*)&compressedSize, sizeof(compressedSize));
  std::vector<uint8_t> compressed(compressedSize);
  in.in.read((char*)compressed.data(), compressedSize);
  in.in.close();
  size_t nVox = volSize[0] * volSize[1] * (size_t)volSize[2];
  vol.resize(nVox);
  const int decompressed_size = LZ4_decompress_safe((const char*)compressed.data(), (char*)vol.data(), compressedSize, nVox);
  return true;
}