#pragma once
#ifndef SF_BMP_WRITER_H
#define SF_BMP_WRITER_H

#include <cassert>
#include <cinttypes>
#include <fstream>
#include <iostream>
#include <vector>

#include "BMPIO.h"
#include "Image4bit.h"

class SfBMPWriter : public SfBMPIO {
 public:
  SfBMPWriter() {}

  bool Write(const std::string &fileName, const Image4bit &image);

 private:
  void _WriteFileHeader();
  void _WriteDIBHeader();
  void _WriteData(const Image4bit &image);
};

#endif  // SF_BMP_WRITER_H
