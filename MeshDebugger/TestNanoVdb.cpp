#include "TestNanoVdb.h"

#include <filesystem>
#include <iostream>

#include "ImageIO.h"
#include "Vec3.h"

#ifdef USE_NANOVDB

#include "nanovdb/NanoVDB.h"
#include <nanovdb/util/CreateNanoGrid.h>
#include <nanovdb/util/IO.h>

void TestNanoVdb() {
  std::string printDir = "F:/v8/1213overlay/print0.25mm/";
  unsigned startIdx = 2678;
  unsigned endIdx = 6899;
  unsigned zInterval = 10;
  Vec3f res(0.256, 0.254, 0.25);
  nanovdb::build::Grid<float> grid(0, "print");
  auto acc = grid.getAccessor();
  unsigned cols = 372;
  unsigned rows = 291;
  for (unsigned i = startIdx; i <= endIdx;i+=zInterval) {
    Array2D8u slice;
    std::string imageFile = printDir + std::to_string(i) + ".png";
    std::filesystem::path p(imageFile);
    if (!std::filesystem::exists(p)) {
      continue;
    }
    std::cout<<i<<"\n";
    LoadPngGrey(imageFile, slice);
    unsigned z = (i - startIdx)/zInterval;
    Vec2u imageSize = slice.GetSize();
    for (unsigned row = 0; row < imageSize[1]; row++) {
      if (row >= rows) {
        break;
      }
      for (unsigned col = 0; col < imageSize[0]; col++) {
        if (col >= cols) {
          break;          
        }
        
        const auto ijk = nanovdb::Coord(col, row, z);
        uint8_t mat = slice(col, row);
        acc.setValue(ijk, float(mat/200.0f));
        float val = acc.getValue(ijk);
      }
    }
  }
  nanovdb::io::writeGrid<nanovdb::HostBuffer>("testfloat.nvdb",
                                              nanovdb::createNanoGrid(grid));
}

#else

void TestNanoVdb() { std::cout << "not using nano vdb\n"; }

#endif