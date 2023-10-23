#include <iostream>
#include "UILib.h"
#include "ImageIO.h"
#include "TrigMesh.h"
#include "Water.h"
//#include "UILib/UILib.h"

void debugInterpU() {
	
}

void GrayToRGBA(const Array2D8u& gray, Array2D8u& color) {
  Vec2u size = gray.GetSize();
  color.Allocate(size[0] * 4, size[1]);
  for (size_t row = 0; row < size[1]; row++) {
    const uint8_t* srcPtr = gray.DataPtr() + row * size[0];
    uint32_t* dstPtr = (uint32_t*)(color.DataPtr() + row * size[0] * 4);
    for (size_t col = 0; col < size[1]; col++) {
      uint32_t gray = srcPtr[col];
      uint32_t color = 0xff000000 | (gray << 16) | (gray << 8) | (gray);
      dstPtr[col] = color;
    }
  }
}

void UpdateImage(UILib& ui, int imageId, const Array2D8u& gray) {
  Array2D8u color;
  GrayToRGBA(gray, color);
  unsigned numChannels = 4;
  ui.SetImageData(imageId, color, numChannels);
}

int main(int argc, char* argv[]){
  UILib ui;
  TrigMesh mesh;
	std::cout<<"XD\n";
  Water water;

  ui.SetShowGL(false);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 1000);
  ui.SetInitImagePos(100, 0);
  int dummyId = ui.AddSlideri("lol", 20, -1, 30);
  int mainImageId = ui.AddImage();

  Array2D8u mainImage(1024, 800);
  mainImage.Fill(0);
  std::function<void()> btFunc =
      std::bind(&UpdateImage, std::ref(ui), mainImageId, mainImage);
  ui.AddButton("Update image", btFunc);
  std::function<void()> saveImageFunc =
      std::bind(&SavePngGrey, "mainImage.png", std::ref(mainImage));
  ui.AddButton("Save main image", saveImageFunc);
  ui.Run();

	while (true) {
		water.Step();
	}
	return 0;
}