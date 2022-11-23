
#include "UILib.h"
#include "lodepng.h"
#include "Array2D.h"
#include "ImageIO.h"
#include "ImageUtils.h"
#include "GLRender.h"
#include "ShallowWater.h"

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

void Plot(const std::vector<float>& y, float dx, Array2D4b& image, Vec4b color) {
  for (size_t i = 1; i <y.size(); i++) {
    DrawLine(image, Vec2f(dx*i,y[i]), Vec2f(dx*(i-1),y[i - 1]), color);
  }
}

void TestPlot(UILib& ui, float phase, int imageId) { 
  std::vector<Vec2f> curve(200);
  Array2D4b image(500,500);
  image.Fill(Vec4b(100,200,255,125));
  
  for (size_t i = 0; i < curve.size(); i++) {
    curve[i][0] = i*4;
    curve[i][1] = (1+std::sin(phase+i*0.2))*200;
  }
  Plot(curve, image, Vec4b(200, 100, 80, 255));
  ui.SetImageData(imageId, image);
}

std::vector<float> GetRow(const Array2Df& arr, unsigned row) {
  Vec2u size = arr.GetSize();
  std::vector<float> r(size[0]);
  for (size_t i = 0; i < size[0]; i++) {
    r[i] = arr(i, row);  
  }
  return r;
}

void Add(float val, std::vector<float>& v) {
  for (size_t i = 0; i < v.size(); i++) {
    v[i] += val;
  }
}

void Mul(float s, std::vector<float>& v) {
  for (size_t i = 0; i < v.size(); i++) {
    v[i] *= s;
  }
}

void PlotWater1D(const ShallowWater& water, Array2D4b& image) {
  Vec4b bg = Vec4b(100, 200, 255, 125);
  image.Fill(bg);
  Vec4b Hcolor(77, 56, 0, 255);
  Vec4b hcolor(0, 156, 189, 255);
  Vec4b ucolor(180, 18, 0, 255);
  float offset = 300;
  float dx = 2;
  std::vector<float> H = GetRow(water.GetH(), 0);
  std::vector<float> h = GetRow(water.Geth(), 0);
  std::vector<float> u = GetRow(water.Getu(), 0);

  Mul(400, H);
  Add(offset, H);
  Mul(-400, h);
  Add(offset-10, h);
  Mul(-100, u);
  Add(offset-100, u);

  Plot(H, dx, image, Hcolor);
  Plot(h, dx, image, hcolor);
  Plot(u, dx, image, ucolor);
}


bool animRunning = false;
bool step = false;

void AnimThread(UILib* ui) {
  int imageId = ui->AddImage();
  int frameCnt = 0;
  
  Array2D4b image(500, 500);

  ShallowWater water;
  unsigned int gridWidth = 200;
  std::vector<float> inith(gridWidth, 0);
  unsigned i0 = gridWidth / 5 * 2;
  unsigned i1 = gridWidth / 5 * 3;
  for (unsigned int i = i0; i < i1; i++) {
    inith[i] = 0.05f;
  }
  inith[1] = 0.05f;
  water.Init(gridWidth, 1);
  water.SethRow(inith, 0);
  float dt = 0.01f;
  step = true;
  while (animRunning) {
    if (step) {
      water.Step1D(dt);
      PlotWater1D(water, image);
      ui->SetImageData(imageId, image);
    //  step = false;
    }
    frameCnt++;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void TestAnimPic(UILib& ui) {
  animRunning = true;
  std::thread t(AnimThread, &ui);
  t.detach();
}

int main(int, char**) {
  GLRender gl;

  UILib ui;
  ui.SetFontsDir("./fonts");
  std::function<void()> btFunc = std::bind(&TestImageDisplay, std::ref(ui));
  TestAnimPic(ui);
  ui.AddLabel("lmao");
  ui.AddButton("Show image", btFunc);
  ui.Run();
  while (1) {
    std::string line;
    std::cin >> line;
    if (line == "q") {
      break;
    } else if (line == "s") {
      step = true;
    }
  }
  animRunning = false;
  ui.Shutdown();

  return 0;
}
