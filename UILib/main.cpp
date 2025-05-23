
#include "UILib.h"
#include "lodepng.h"
#include "Array2D.h"
#include "ImageIO.h"
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
  const std::string testImageFile = "./resource/test.png";
  Array2D8u image;
  int ret = LoadPngColor(testImageFile, image);
  if (ret < 0) {
    return;
  }
  int imageId = ui.AddImage();

  Vec2u size = image.GetSize();
  Array2D8u imageA(size[0] / 3 * 4, size[1]);
  for (size_t row = 0; row < size[1]; row++) {
    for (size_t col = 0; col < size[0] / 3; col++) {
      imageA(4 * col, row) = image(3 * col, row);
      imageA(4* col + 1, row) = image(3 * col + 1, row);
      imageA(4 * col + 2, row) = image(3 * col + 2, row);
      imageA(4 * col + 3, row) = 255;
    }
  }
  ui.SetImageData(imageId, image, 3);
}

void ShowGLInfo(UILib& ui, int label_id) { 
  std::string info = ui.GetGLInfo();
  ui.SetLabelText(label_id, info);
}

int main(int, char**)
{
  //SaveObjFromWebgl();
  UILib ui;
  ui.SetFontsDir("./fonts");
  std::function<void()> btFunc = std::bind(&TestImageDisplay, std::ref(ui));  
  ui.SetWindowSize(1280, 800);
  int buttonId = ui.AddButton("GLInfo", {});
  int gl_info_id = ui.AddLabel(" ");
  std::function<void()> showGLInfoFunc =
      std::bind(&ShowGLInfo, std::ref(ui), gl_info_id);
  ui.SetButtonCallback(buttonId, showGLInfoFunc);
  ui.AddButton("Show image", btFunc);

  auto mesh=std::make_shared<TrigMesh>();
  mesh->LoadObj("F:/uvTest/cyl_out.obj");
  int meshId = ui.AddMesh(mesh);  
  ui.SetMeshColor(meshId, Vec3b(250,180,128));
  Array2D8u texImage;
  LoadPngColor("F:/dump/checker.png", texImage);
  ui.SetMeshTex(meshId, texImage, 3);
  ui.SetWindowSize(1280, 800);
  ui.Run();
  const unsigned PollFreqMs = 100;
  while (ui.IsRunning()) {
    // main thread not doing much
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  return 0;
}
