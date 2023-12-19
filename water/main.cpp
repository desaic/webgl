#include <iostream>
#include "UILib.h"
#include "ImageIO.h"
#include "ImageUtils.h"
#include "TrigMesh.h"
#include "Water.h"
#include "UILib.h"
#include <cassert>

void approxEqual(float a, float b) {
	assert(abs(a - b) < 1e-10);
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

void testAdvectU_3x3_X_interior() {
  Water water;
  water.Allocate(3,3,3);
  Array3D<Vec3f>& u = water.U();
  const Vec3u& sz = u.GetSize();
  for (int i = 1; i < sz[0] - 1; ++i) {
    for (int j = 1; j < sz[1] - 1; ++j) {
      for (int k = 1; k < sz[2] - 1; ++k) {
        u(i, j, k) = Vec3f(1.0f, 0.f, 0.f);
      }
    }
  }

  water.AdvectU();
  
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        if (i == 0 || j == 0 || k == 0 || i == sz[0] -1 || j == sz[1] - 1 || k == sz[2] - 1) {
          approxEqual(u(i,j,k)[0], 0);
        } else {
          approxEqual(u(i,j,k)[1], 0);
          approxEqual(u(i,j,k)[2], 0);
        }
      }
    }
  }

  approxEqual(u(1,2,1)[0], u(1,2,2)[0]);

  std::cout << "testAdvectU_3x3_X_interior passed!" << std::endl;
}

void testAdvectU_3x3_X_constant() {
  Water water;
  water.Allocate(3,3,3);
  Array3D<Vec3f>& u = water.U();
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        u(i, j, k) = Vec3f(1.0f, 0.f, 0.f);
      }
    }
  }

  water.AdvectU();
  
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        approxEqual(u(i,j,k)[0], 1);
        approxEqual(u(i,j,k)[1], 0);
        approxEqual(u(i,j,k)[2], 0);
      }
    }
  }

  std::cout << "testAdvectU_3x3_X_constant passed!" << std::endl;
}

void testAdvectU_3x3_Y_constant() {
  Water water;
  water.Allocate(3,3,3);
  Array3D<Vec3f>& u = water.U();
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        u(i, j, k) = Vec3f(0.f, 1.f, 0.f);
      }
    }
  }

  water.AdvectU();
  
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        approxEqual(u(i,j,k)[0], 0);
        approxEqual(u(i,j,k)[1], 1);
        approxEqual(u(i,j,k)[2], 0);
      }
    }
  }

  std::cout << "testAdvectU_3x3_Y_constant passed!" << std::endl;
}

void testAdvectU_3x3_Z_constant() {
  Water water;
  water.Allocate(3,3,3);
  Array3D<Vec3f>& u = water.U();
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        u(i, j, k) = Vec3f(0.f, 0.f, 1.f);
      }
    }
  }

  water.AdvectU();
  
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        approxEqual(u(i,j,k)[0], 0);
        approxEqual(u(i,j,k)[1], 0);
        approxEqual(u(i,j,k)[2], 1);
      }
    }
  }

  std::cout << "testAdvectU_3x3_Z_constant passed!" << std::endl;
}

void testAdvectU_3x3_constant() {
  Water water;
  water.Allocate(3,3,3);
  Array3D<Vec3f>& u = water.U();
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        u(i, j, k) = Vec3f(1.f, 1.f, 1.f);
      }
    }
  }

  water.AdvectU();
  
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        approxEqual(u(i,j,k)[0], 1);
        approxEqual(u(i,j,k)[1], 1);
        approxEqual(u(i,j,k)[2], 1);
      }
    }
  }

  std::cout << "testAdvectU_3x3_constant passed!" << std::endl;
}

void testInterpU() {
  Water water;
  water.Allocate(1, 1, 1);
  Array3D<Vec3f>& u = water.U();
  
  u(0,0,0) = Vec3f(0.0, 0.0, 0.0);
  u(0,1,0) = Vec3f(10.0, 10.0, 10.0);
  u(0,0,1) = Vec3f(20.0, 20.0, 20.0);
  u(0,1,1) = Vec3f(30.0, 30.0, 30.0);
  u(1,0,0) = Vec3f(40.0, 40.0, 40.0);
  u(1,1,0) = Vec3f(50.0, 50.0, 50.0);
  u(1,0,1) = Vec3f(60.0, 60.0, 60.0);
  u(1,1,1) = Vec3f(70.0, 70.0, 70.0);

  const float h = 0.02;

  for (int i = 0; i < 4; ++ i) {
    for (int j = 0; j < 4; ++ j) {
      for (int k = 0; k < 4; ++k) {
        float x = 0.25 *i * h;
        float y = 0.25* j * h;
        float z = 0.25* k * h;
        Vec3f p(x,y,z);
        Vec3f interpolated = water.InterpU(p, Axis::X);

        std::cout << interpolated[1] << " ";
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

void testSolvePSimple() {
  Water water;
  water.Allocate(2, 2, 1);
  Array3D<Vec3f>& u = water.U();
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
      }
    }
  }

  water.Step();
}

void testStep() {
  Water water;
  water.Allocate(50, 50, 50);
  water.Step();
}

void DrawVecs(const Water& water, Array2D4b& image, float vScale) {
  Vec3u gridSize = water.U().GetSize(); 
  if (gridSize[0] == 0 || gridSize[1] == 0 || gridSize[2] == 0) {
    return;
  }
  unsigned z = gridSize[2] / 2;
  int spacing = 10;
  Vec2u imageSize = image.GetSize();
  unsigned maxX = std::min(gridSize[0] - 1, imageSize[0] / spacing);
  unsigned maxY = std::min(gridSize[1] - 1, imageSize[1] / spacing);
  image.Fill(Vec4b(10, 10, 10, 255));
  for (unsigned y = 0;y<maxY;y++) {
    for (unsigned x = 0; x < maxX; x++) {
      unsigned ox = x * spacing + (spacing / 2);
      unsigned oy = y * spacing + (spacing / 2);

      float vx = (water.U()(x, y, z)[0] + water.U()(x + 1, y, z)[0]) / 2;
      float vy = (water.U()(x, y, z)[1] + water.U()(x, y+1, z)[1]) / 2;
      unsigned px = ox + vx * vScale;
      unsigned py = oy + vy * vScale;      
      DrawLine(image, Vec2f(ox, oy), Vec2f(px, py), Vec4b(255, 255, 255, 255));      
    }
  }
}

class WaterApp {
 public:
  WaterApp(UILib* ui) : _ui(ui) { Init(); }
  void Init() {
    _water.Allocate(50, 50, 1);
    _image.Allocate(800, 800);
    _image.Fill(Vec4b(10, 10, 10, 255));
    if (!_ui) {
      return;
    }
    _imageId = _ui->AddImage();
    _vScaleSliderId = _ui->AddSlideri("v scale", 10, 1, 500);
    _ui->AddButton("save image",
                   [&] { SavePngColor("water_app.png", _image); });
  }

  int& ImageId() { return _imageId; }
  int ImageId() const { return _imageId; }

  void Refresh() {
    if (!_ui || _imageId < 0) {
      return;
    }
    _water.Step();
    float vScale = 1.0f;
    if (_vScaleSliderId >= 0) {
      std::shared_ptr<Slideri> slider =
          std::dynamic_pointer_cast<Slideri> (_ui->GetWidget(_vScaleSliderId));
      vScale = slider->GetVal() / 10.0f;
    }
    DrawVecs(_water, _image, vScale);
    _ui->SetImageData(_imageId, _image);
  }
 private:
  Water _water;
  UILib* _ui = nullptr;
  int _imageId = -1;
  Array2D4b _image;
  int _vScaleSliderId = -1;
};

int main(int argc, char* argv[]){
  UILib ui;
  WaterApp app(&ui);

  //testSolvePSimple();
  //testStep();
  //testInterpU();
  //testAdvectU_3x3_X_interior();
  //testAdvectU_3x3_X_constant();
  //testAdvectU_3x3_Y_constant();
  //testAdvectU_3x3_Z_constant();
  //testAdvectU_3x3_constant();

  ui.SetFontsDir("./fonts");
  ui.SetInitImagePos(100, 0);
  ui.Run();

  const unsigned PollFreqMs = 20;
  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
	return 0;
}