#pragma once

#include "ConfigFile.hpp"
#include "Image.h"

void extractPrintData(const ConfigFile & conf);

bool loadBinSeq(int idx, std::string seqDir, const ConfigFile & conf, std::vector<ImageU8> & images);

///\brief post process, offline analysis of captured data. 
void makeTrainingData(ConfigFile & conf);

void computeBrightness(const std::vector<uint8_t> & volume,
  const int * volumeSize, const std::vector<float> & depth,
  std::vector<uint8_t> & brightness);

void computeNormal(const uint32_t * surfSize, const std::vector<float> & depth,
  std::vector<uint8_t> & normal);

void computeReflectance(const std::vector<uint8_t> & volume,
  const int * volumeSize, const std::vector<float> & depth,
  const ConfigFile & conf, std::vector<float> & angles, std::vector<float> & r);

float leastSquares1D(const std::vector<float> & history);

bool loadImgSeq(std::string seqDir, const ConfigFile & conf,
  std::vector<ImageU8> & images);

void computeTilt(const std::vector<float> & img, int width, int height,
  const ConfigFile & conf, std::vector<float> & out);

void saveObjMesh(const ImageF32 & floatImg,
  const ConfigFile & conf, int scanIdx);