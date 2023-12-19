#include "BMPWriter.h"

#include <iostream>

bool SfBMPWriter::Write(const std::string &fileName, const Image4bit &image) {
  std::ofstream os(fileName, std::ostream::binary);
  if (os.bad()) {
    return false;
  }

  int width = image.GetSize()[0];
  int height = image.GetSize()[1];

  Header header;
  DIBHeader dibHeader(width, height);

  uint8_t colors[16][4];

  // @todo(ari) ReviewME: This appears to leave the fourth component "alpha"
  // undefined.
  // making every color 255 white. make 0 appear black.
  for (int c = 0; c < 3; ++c) {
    colors[0][c] = 0;
  }
  for (int i = 1; i < 16; ++i) {
    for (int c = 0; c < 3; ++c) {
      colors[i][c] = 255;
    }
    colors[i][3] = 0;
  }

  const Image4bit::ImageData &imageData = *image.GetData();

  header.fileSize =
      sizeof(Header) + sizeof(DIBHeader) + sizeof(colors) + imageData.GetSize();
  header.imageOffset = sizeof(Header) + sizeof(DIBHeader) + sizeof(colors);

  os.write(reinterpret_cast<const char *>(&header), sizeof(header));
  os.write(reinterpret_cast<const char *>(&dibHeader), sizeof(dibHeader));
  os.write(reinterpret_cast<const char *>(colors), sizeof(colors));
  os.write(reinterpret_cast<const char *>(imageData.data.data()),
           imageData.GetSize());
  if (os.fail() || os.bad()) {
    return false;
  }
  os.close();

  return true;
}
