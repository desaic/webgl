
#include "UILib.h"
#include "lodepng.h"
#include "Array2D.h"
#include "ImageIO.h"
#include "ImageUtils.h"
#include "GLRender.h"
#include <iostream>
#include <fstream>
#include <filesystem>

// test png io by writing a channel to gray bmp
void TestPngLoading(const Array2D8u& image, int numChan) {
  Vec2u size = image.GetSize();
  Array2D8u redChannel(size[0] / numChan, size[1]);
  for (size_t row = 0; row < size[1]; row++) {
    for (size_t col = 0; col < size[0] / numChan; col++) {
      redChannel(col, row) = image(numChan * col, row);
    }
  }
  std::vector<uint8_t> bmpBuf;
  EncodeBmpGray(redChannel.DataPtr(), size[0] / numChan, size[1], bmpBuf);
  std::ofstream bmpout("debug.bmp", std::ostream::binary);
  bmpout.write((const char*)bmpBuf.data(), bmpBuf.size());
  bmpout.close();
}

void TestImageDisplay(UILib& ui) {
  const std::string testImageFile = "F:/github/imgui_example/resource/test.png";
  Array2D8u image;
  int ret = LoadPngColor(testImageFile, image);
  if (ret < 0) {
    return;
  }
  int imageId = ui.AddImage();

  Vec2u size = image.GetSize();
  ui.SetImageData(imageId, image, 3);
}

void Plot(const std::vector<Vec2f>& points, Array2D4b & image, Vec4b color) {
  for (size_t i = 1; i < points.size(); i++) {
    DrawLine(image, points[i], points[i - 1], color);
  }
}

void TestPlot(UILib& ui) { 
  std::vector<Vec2f> curve(200);
  Array2D4b image(500,500);
  int imageId = ui.AddImage();
  image.Fill(Vec4b(100,200,255,125));
  
  for (size_t i = 0; i < curve.size(); i++) {
    curve[i][0] = i*4;
    curve[i][1] = (1+std::sin(i*0.2))*200;
  }
  Plot(curve, image, Vec4b(200, 100, 80, 255));
  ui.SetImageData(imageId, image);
}

int main(int, char**) {
  GLRender gl;

  UILib ui;
  ui.SetFontsDir("./fonts");
  std::function<void()> btFunc = std::bind(&TestImageDisplay, std::ref(ui));
  TestPlot(ui);
  ui.AddLabel("lmao");
  ui.AddButton("Show image", btFunc);
  ui.Run();

  return 0;
}
