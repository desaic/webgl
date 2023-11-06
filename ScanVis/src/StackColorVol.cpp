#include "StackColorVol.hpp"
#include "Array3D.h"
#include "ArrayUtil.hpp"
#include "FileUtil.hpp"
#include "cvMatUtil.hpp"
#include "meshutil.h"
#include "VolumeIO.hpp"
#include "Recon.hpp"
#include <iostream>

/// Print layer is flipped in Y to match scan and color images.
void ReadPrintLayer(Array3D<uint8_t> & vol, std::string fileName, int layerIdx) {
  if (layerIdx < 0 || layerIdx >= vol.GetSize()[2]) {
    return;
  }
  cv::Mat slice = cv::imread(fileName);
  if (slice.rows == 0) {
    std::cout << "can't read " << fileName << "\n";
    return;
  }
  int cols = std::min(int(vol.GetSize()[0]), slice.cols/3);
  int rows = std::min(int(vol.GetSize()[1]), slice.rows);
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      cv::Vec3b color = slice.at < cv::Vec3b>(slice.rows - y - 1, 3 * x);
      vol(x, y, layerIdx) = color[2];
    }
  }
}

void SavePrintVol(Array3D<uint8_t>& vol, std::string outFile) {
  int outx = vol.GetSize()[0] / 2 + vol.GetSize()[2] % 2;
  std::vector<DualPixel> v(outx * vol.GetSize()[1] * vol.GetSize()[2], 0);
  for (int z = 0; z < vol.GetSize()[2]; z++) {
    for (int y = 0; y < vol.GetSize()[1]; y++) {
      for (int x = 0; x < vol.GetSize()[0]; x+=2) {
        size_t volIdx = z * size_t(outx) * vol.GetSize()[1]
          + y * outx + x / 2;
        int mats[2];
        for (int p = 0; p < 2; p++) {
          int matIdx = 0;
          if (x+p >= vol.GetSize()[0]) {
            break;
          }
          matIdx = vol(x+p, y, z);
          mats[p] = matIdx;
        }
        v[volIdx].pixel1 = mats[0];
        v[volIdx].pixel2 = mats[1];
      }
    }
  }
  int volSize[3];
  for (int d = 0; d < 3; d++) {
    volSize[d] = vol.GetSize()[d];
  }
  volSize[0] = outx;
  SavePrintVolume(outFile, v, volSize);
}

void LoadPrintVol(Array3D<uint8_t>& vol, std::string inFile) {
  int volSize[3];
  std::vector<DualPixel> v;
  LoadPrintVolume(inFile, v, volSize);

  vol.Allocate(volSize[0]*2, volSize[1], volSize[2]);
  for (size_t z = 0; z < vol.GetSize()[2]; z++) {
    for (size_t y = 0; y < vol.GetSize()[1]; y++) {
      for (size_t x = 0; x < size_t(volSize[0]); x ++) {
        size_t volIdx = z * size_t(volSize[0]) * vol.GetSize()[1] + y * volSize[0] + x;
        int mats[2];
        mats[0] = v[volIdx].pixel1;
        mats[1] = v[volIdx].pixel2;

        for (int p = 0; p < 2; p++) {
          int matIdx = 0;
          if (x*2 + p >= vol.GetSize()[0]) {
            break;
          }
          vol(x*2 + p, y, z) = mats[p];
        }
      }
    }
  } 
}

///\param hc color in hsv space
///\param hprev hsv color in previous image.
///\param pl previous label
///\return -1 if too dim to make any guess
/// -2 if change in color is too small
/// -3 if color is not moving towards any known color centers.
/// 0 if no material is printed
/// 1 for first material etc.
int inferMaterial(const std::vector<cv::Vec3f>& colorCenter,
  const cv::Vec3f& nc, const cv::Vec3f& hc,
  const cv::Vec3f& pc, const cv::Vec3f& hprev,
  uint8_t pl)
{
  float minDist = 1e6;
  int minLabel = -1;
  //for color values between 0-65535
  float minColorNorm = 256;
  //saturation needs to reduce by at least this amount
  //to register a transition from red to white material.
  const float MIN_SAT_CHANGE = 0.1f;
  const float MIN_DIFF_BASE = 128;
  float minDiff = MIN_DIFF_BASE;
  std::vector<float> distThresh(colorCenter.size(), MIN_DIFF_BASE);
  distThresh[0] = 2048;

  float colorNorm = cv::norm(nc, cv::NORM_L2);
  //color is too dim
  if (colorNorm < minColorNorm) {
    return -1;
  }
  cv::Vec3f diff = nc - pc;
  //color is similar to previous image
  //return previous label
  if (cv::norm(diff) < minDiff) {
    return pl;
  }
  if (hc(1) < 0.2) {
    return 1;
  }
  if (hprev(1) > 0.4f) {
    if (hprev(1) - hc(1)>0.1f) {
      return 1;
    }
    else if (hprev(1) - hc(1) > 0) {
      return pl;
    }
    else {
      return 2;
    }
  }
  else {
    if (hc(1) - hprev(1) > 0.3) {
      return 2;
    }
    else if (hprev(1) - hc(1)>0.1) {
      return 1;
    }
  }
  //not moving towards any target
  return -3;
}

///\return CV_8UC1
cv::Mat extractSlice(const Array3D<uint8_t>& vol, int sliceIdx)
{
  const Vec3u volSize = vol.GetSize();
  size_t nPix = volSize[0] * volSize[2];
  cv::Mat img(volSize[2], volSize[0], CV_8UC1);
  for (int row = 0; row < volSize[2]; row++) {
    for (int col = 0; col < volSize[0]; col++) {
      img.at<uint8_t>(row, col) = vol(col, sliceIdx, row);
    }
  }
  return img;
}

void saveSlice(const Array3D<uint8_t> & vol, int sliceIdx,
  const std::string & imgFileName) {
  //save xz slice;
  const Vec3u volSize = vol.GetSize();
  size_t nPix = volSize[0] * volSize[2];
  std::vector<uint8_t > slice(nPix, 0);
  for (int row = 0; row < volSize[2]; row++) {
    for (int col = 0; col < volSize[0]; col++) {
      slice[row * volSize[0] + col] = vol(col, sliceIdx, row);
    }
  }
  cv::Mat img(volSize[2], volSize[0], CV_8UC1, slice.data());
  cv::imwrite(imgFileName, img);
}

///get the invert affine transformation 2x3 matrix from a perspective transformation
///throwing away the third row 
cv::Mat invertAffine(const cv::Mat & pers ) {
  cv::Mat affine(2, 3, pers.type());
  int depth = pers.depth();
  bool isFloat = false;
  if (depth == CV_32F) {
    isFloat = true;
  }
  for (int row = 0; row < affine.rows; row++) {
    for (int col = 0; col < affine.cols; col++) {
      if (isFloat) {
        affine.at<float>(row, col) = pers.at<float>(row, col);
      }
      else {
        affine.at<double>(row, col) = pers.at<double>(row, col);
      }
    }
  }

  cv::Mat i;
  cv::invertAffineTransform(affine, i);
  return i;
}

//filter the volume along z direction 
void filterVolZ(const Array3D<uint8_t> & vol, Array3D<uint8_t>& out) {
  //float coef[] = { -0.98661, -0.96403, -0.90515, -0.76159, -0.46212,
  //                 0, 0.46212,   0.76159,   0.90515, 0.96403,   0.98661 };
  float coef[] = { -0.986614, -0.978026, -0.964028, -0.941376, -0.905148, -0.848284, -0.761594, -0.635149, -0.462117,
    -0.244919, 0.000000, 0.244919, 0.462117, 0.635149, 0.761594, 0.848284, 0.905148, 0.941376, 0.964028
    , 0.978026, 0.986614 };
  int kerSize = sizeof(coef) / sizeof(coef[0]);
  cv::Mat kernelY(kerSize, 1, CV_32FC1, coef);
  cv::Mat kernelX(1, kerSize, CV_32FC1, coef);

  out.Allocate(vol.GetSize()[0], vol.GetSize()[1], vol.GetSize()[2]);
  for (size_t x = 0; x < vol.GetSize()[0]; x++) {
    size_t idx = x * vol.GetSize()[1] * vol.GetSize()[2];
    //cv mat is row major by default.
    cv::Mat slice(vol.GetSize()[1], vol.GetSize()[2], CV_8UC1, (void*)(&(vol.GetData()[idx])));
    cv::Mat floatSlice;
    slice.convertTo(floatSlice, CV_32FC1);
    
    cv::filter2D(floatSlice, floatSlice, -1, kernelX, cv::Point(-1, -1), 0, CV_HAL_BORDER_REPLICATE);
    for (size_t y = 0; y < vol.GetSize()[1]; y++) {
      for (size_t z = 0; z < vol.GetSize()[2]; z++) {
        float fv = floatSlice.at<float>(y, z) / 5.0f;
        fv = clamp(fv, 0.0f, 255.0f);
        out(x, y, z) = uint8_t( fv );
      }
    }
  }
}

///filter the volume along z direction 
cv::Mat calcDepthMap(const Array3D<uint8_t>& vol, Array3D<uint8_t>& out) {
  filterVolZ(vol, out);
  cv::Mat depthMap(vol.GetSize()[1], vol.GetSize()[0], CV_8UC1);
  uint8_t thresh = 50;
  
  for (size_t x = 0; x < vol.GetSize()[0]; x++) {
    for (size_t y = 0; y < vol.GetSize()[1]; y++) {
      uint8_t maxv = 0;
      int maxIdx = -1;
      for (size_t z = 0; z < vol.GetSize()[2]; z++) {
        uint8_t v = out(x, y, z);
        if (v > maxv) {
          maxv = v;
          maxIdx = z;
        }
        if (v > thresh) {
          break;
        }
      }
      if (maxIdx < 0 || maxv<thresh) {
        maxIdx = uint8_t(vol.GetSize()[2] - 1);
      }
      else {
        maxIdx += 8;
        maxIdx = std::min(maxIdx, int(vol.GetSize()[2] - 1));
      }
      depthMap.at<uint8_t>(y, x) = maxIdx;      
    }
  }
  cv::medianBlur(depthMap, depthMap, 3);
  return depthMap;
}

///\param h CV_16UC1 height map in scan space, should be warped
///\param zOffset only applied to non-zero depth values.
void SaveObjWithTexture(const cv::Mat & h, const std::string & objFile) {
  const unsigned short MIN_COLOR_NORM = 256;
  //save each vertex as a vertex,
  //mm
  float voxelRes[3] = { 0.05, 0.05, 0.005 };
  std::ofstream out(objFile);

  //int crop[2][2] = { {60,80}, {523, 540} };
  int crop[2][2] = { {0,0}, {h.cols, h.rows} };
  int col0 = std::max(crop[0][0], 0);
  int row0 = std::max(crop[0][1], 0);
  //end column index
  int cols = std::min(crop[1][0], h.cols);
  int rows = std::min(crop[1][1], h.rows);
  //write vertices
  for (int row = row0; row < rows; row++) {
    for (int col = col0; col < cols; col++) {
      float x, y, z;
      //image row is from top to bottom, needs to flip
      int imgRow = h.rows - row - 1;
      x = col * voxelRes[0];
      y = row * voxelRes[1];
      uint16_t d = int(h.at<uint16_t>(imgRow, col));
      z = d * voxelRes[2];
      
      out << "v " << x << " " << y << " " << z << "\n";
    }
  }
  
  //write texture coordinates
  //texture coordinate uses pixel as unit
  //texture coordinate convention is flipped from 
  //image row ordering
  for (int row = row0; row < rows; row++) {
    for (int col = col0; col < cols; col++) {
      float v = (row + 0.5f) / h.rows;
      float u = (col + 0.5f) / h.cols;
      out << "vt " << u<< " " << v << "\n";
    }
  }

  //save triangles. two triangles per pixel
  ///TODO subtract indices to account for points outside of ROI
  for (int row = row0; row < rows - 1; row++) {
    for (int col = col0; col < cols - 1; col++) {
      int vidx[3];
      vidx[0] = (row-row0) * (cols-col0) + (col-col0);
      vidx[1] = vidx[0] + 1;
      vidx[2] = vidx[0] + cols - col0;
      out << "f";
      for (int v = 0; v < 3; v++) {
        if (vidx[v] >= (cols - col0) * (rows - row0)) {
          std::cout << "debug \n";
        }
        out << " " << (vidx[v]+1) << "/" << (1+vidx[v]);
      }
      out << "\n";
      
      vidx[0] = (row-row0) * (cols -col0) + (col - col0)+ 1;
      vidx[1] = vidx[0] + (cols-col0);
      vidx[2] = vidx[1] - 1;
      out << "f";
      for (int v = 0; v < 3; v++) {
        if (vidx[v] >= (cols - col0) * (rows - row0)) {
          std::cout << "debug \n";
        }
        out << " " << (vidx[v] + 1) << "/" << (1 + vidx[v]);
      }
      out << "\n";
    }
  }
  
  out.close();
}

///\param calcFromVol if false, load from depth images directly
void calcDepthMaps(const std::string dataDir,
  int startIdx, int endIdx, bool calcFromVol,
  std::vector<cv::Mat> & depthMaps) 
{
  int scanVolSize[3];
  Array3D<uint8_t> scanVol, filteredScan;
  std::string volDir = dataDir + "/vol";
  std::string depthDir = dataDir + "/depth";
  
  for (int i = startIdx; i <= endIdx; i++) {
    std::string volFile = volDir + "/" + std::to_string(i) + ".vol";
    std::string depthFile = depthDir + "/" + std::to_string(i) + ".png";
    cv::Mat depthMap;

    if (calcFromVol) {
      LoadVolume(volFile, scanVol.GetData(), scanVolSize);
      scanVol.Allocate(scanVolSize[0], scanVolSize[1], scanVolSize[2]);
      depthMap = calcDepthMap(scanVol, filteredScan);
      cv::imwrite(depthFile, depthMap);
    }
    else {
      depthMap = cv::imread(depthFile, cv::IMREAD_UNCHANGED);
    }

    depthMaps.push_back(depthMap);
  }
}

void processPrintData(const ConfigFile& conf, Array3D<uint8_t> & pvol) {
  //read configs
  bool savePrintVol = false;
  conf.getBool("savePrintVol", savePrintVol);
  std::string dataDir = conf.getString("dataDir");
  std::string printDir = dataDir + "/slices";
  if (conf.hasOpt("printDir")) {
    printDir = conf.getString("printDir");
  }

  std::string printVolFile = dataDir + "print.pvol";
  if (conf.hasOpt("printVol")) {
    printVolFile = conf.getString("printVol");
  }

  //print data has 3x x resolution
  //print slices are flipped in y before being saved to .pvol file
  //to agree with scan and color images.
  std::cout << "load print vol\n";
  if (savePrintVol) {
    int status = 0;
    std::vector<std::string> files = listFiles(printDir, status);
    int numLayers = files.size();
    std::string fileName = printDir + "/bits0.png";
    cv::Mat slice = cv::imread(fileName, cv::IMREAD_UNCHANGED);
    if (slice.rows == 0) {
      std::cout << "can't read " << fileName << "\n";
      return;
    }
    pvol.Allocate(slice.cols / 3, slice.rows, numLayers);
    for (int i = 0; i < files.size(); i++) {
      fileName = printDir + "/bits" + std::to_string(i) + ".png";
      if (i % 10 == 0) {
        std::cout << i << "\n";
      }
      ReadPrintLayer(pvol, fileName, i);
    }
    SavePrintVol(pvol, printVolFile);
  }
  else {
    LoadPrintVol(pvol, printVolFile);
  }
  std::cout << "load print vol end\n";
}

void loadEncoder(const std::string& file, std::vector < std::vector<int> >& enc) {
  std::ifstream in(file);
  if (!in.good()) {
    std::cout << "cant open " << file << std::endl;
    return;
  }
  const int NUM_TOKENS = 3;
  std::string line;
  while (std::getline(in, line)) {
    //skip empty lines;
    if (line.size() < 3) {
      continue;
    }
    std::stringstream ss(line);
    std::vector<int>tokens(NUM_TOKENS, 0);
    for (size_t i = 0; i < tokens.size(); i++) {
      ss >> tokens[i];
    }
    enc.push_back(tokens);
  }
  in.close();
}

double avgVal(const cv::Mat& depth) {
  double sum = 0;
  //check image pixel width
  int d = depth.depth();
  int byteSize =1;
  if (d == CV_8U) {
    byteSize = 1;
  }
  else if (d == CV_16U) {
    byteSize = 2;
  }
    
  if (byteSize == 1) {
    for (int row = 0; row < depth.rows; row++) {
      for (int col = 0; col < depth.cols; col++) {
        sum += depth.at<uint8_t>(row, col);
      }
    }
  }
  else {
    for (int row = 0; row < depth.rows; row++) {
      for (int col = 0; col < depth.cols; col++) {
        sum += depth.at<uint16_t>(row, col);
      }
    }
  }
  sum /= depth.rows * depth.cols;
  return sum;
}

///\param heightMaps filtered height maps.
///\param hm heightmap of print data.
void processDepthData(const ConfigFile & conf, const cv::Mat & hm , std::vector<cv::Mat> &heightMaps) {
  std::cout << "load depth\n";
  //depth of platform in the first scan, used as baseline
  //calculated by averaging the first scan image of the platform.
  float depthOffset = 156;
  conf.getFloat("depthOffset", depthOffset);
  //maximum value in depth map. value beyond it are treated as invalid.
  const int DEPTH_MAX = 235;
  std::string dataDir = conf.getString("dataDir");
  std::string objDir = dataDir + "/obj/";
  std::string debugDir = dataDir + "/debug/";
  bool depthFromVol = true;
  conf.getBool("depthFromVol", depthFromVol);
  bool saveWarped = true;
  conf.getBool("saveWarped", saveWarped);
  int startIdx = conf.getInt("startIdx");
  int endIdx = conf.getInt("endIdx");
  std::vector<cv::Mat> depthMaps;
  calcDepthMaps(dataDir, startIdx, endIdx, depthFromVol, depthMaps);
  std::vector<std::vector<int>> enc;
  std::string encfile = conf.getString("encoder");
  loadEncoder(encfile, enc);
  float scanRes[3] = { 0.05, 0.05, 0.005 };
  float printRes[3] = { 0.042, 0.04233,0.025 };
  std::string hFile = conf.getString("homography");
  //homography perspective transformation matrices for depth scan and color images
  cv::Mat HScan, HColor;
  readHMats(hFile, HColor, HScan);
  //how many pixels around print data to leave blank.
  std::vector<int> margin(2, 50);
  conf.getIntVector("margin", margin);
  HScan.at<float>(0, 2) += margin[0];
  HScan.at<float>(1, 2) += margin[1];

  std::vector<int> targetSize(2, 600);
  conf.getIntVector("targetSize", targetSize);
  float platformStep = 2 * 0.02;//mm
  conf.getFloat("platformStep", platformStep);
  cv::Mat prevH = cv::Mat::zeros(targetSize[1], targetSize[0], CV_16UC1);
  
  cv::Mat debugHeight = cv::Mat::zeros(targetSize[1], targetSize[0], CV_8UC1);
  float baseHeight = 0;
  const int RIGHT_CROP = 588;

  //calculate platform average height.
  double avg = avgVal(depthMaps[0]);
  std::cout << "average baseline height " << avg;
  baseHeight = avg;

  for (int i = startIdx; i <= endIdx; i++) {
    //fill in missing region in color image with print data 
    //platform z coordinate in mm
    float platCoord = (i - startIdx) * platformStep;
    cv::Mat depthMap = depthMaps[size_t(i - startIdx)];
    cv::Mat depthWarped = cv::Mat::zeros(targetSize[1], targetSize[0], CV_16UC1);
    cv::warpPerspective(depthMap, depthWarped, HScan, depthWarped.size(), cv::INTER_LINEAR);
    cv::Mat heightMap = cv::Mat::zeros(targetSize[1], targetSize[0], CV_16UC1);

    //convert depth map to height map
    //new depth is bounded from below by prevDepth + platformStep.
    for (int row = 0; row < heightMap.rows; row++) {
      for (int col = 0; col < heightMap.cols; col++) {
        uint16_t prevVal = prevH.at<uint16_t>(row, col);
        int depth = int(depthWarped.at<uint8_t>(row, col));
        //if invalild depth value, keep previous height value.
        if (depth < 90 || depth >= DEPTH_MAX) {
          heightMap.at < uint16_t >(row, col) = prevVal;
          continue;
        }
        if (i>1 && col >= RIGHT_CROP) {
          heightMap.at < uint16_t >(row, col) = 0;
          continue;
        }
        float fheight = depthOffset - depth + platCoord / scanRes[2];
        //print can't grow that fast
        float maxVal = prevVal + (4 * platformStep / scanRes[2]);
        if (i>startIdx+20 && fheight > maxVal) {
          float absHeight = fheight - baseHeight;
          int prow = row - margin[1];
          int pcol = col - margin[0];
          uint16_t printHeight = 0;
          if (prow > 0 && prow < hm.rows && pcol>0 && pcol < hm.cols) {
            printHeight = hm.at<uint16_t>(prow, pcol);
          }
          float fPrintHeight = printHeight * printRes[2] / scanRes[2];
          if (absHeight > fPrintHeight + 3) {
            fheight = prevVal;
          }
        }
        fheight = std::max(0.0f, fheight);

        uint16_t newHeight = uint16_t(fheight);
        newHeight = std::max(newHeight, prevVal);
        
        heightMap.at<uint16_t>(row, col) = newHeight;
        debugHeight.at<uint8_t>(row, col) = newHeight/4;
      }
    }
    
    prevH = heightMap;
    heightMaps.push_back(heightMap);
    //std::string objFile = sequenceFilename(i - startIdx, objDir.c_str(), 0, 3, ".obj");    
    //std::string debugFile = sequenceFilename(i - startIdx, debugDir.c_str(), 0, 3, ".png");
    //cv::imwrite(debugFile, debugHeight);
  }
  
  std::cout << "load depth end\n";
}

cv::Mat printHeightMap(const Array3D<uint8_t> & pvol) {
  cv::Mat hm = cv::Mat::zeros(pvol.GetSize()[1], pvol.GetSize()[0], CV_16UC1);
  for (size_t z = 0; z < pvol.GetSize()[2]; z++) {
    for (size_t y = 0; y < pvol.GetSize()[1]; y++) {
      for (size_t x = 0; x < pvol.GetSize()[0]; x++) {
        int mat = pvol(x, y, z);
        if (mat > 0) {
          hm.at<uint16_t>(y, x) = z;
        }
      }
    }
  }
  return hm;
}

///warp color images to align with print data
///with margin.
void warpColorImages(const ConfigFile& conf, const Array3D<uint8_t> &pvol,
  const std::vector<cv::Mat> & heightMaps,
  std::vector<cv::Mat> & colorImages)
{
  //read config
  std::string dataDir = conf.getString("dataDir");
  std::string hFile = conf.getString("homography");
  std::string tifDir = dataDir + "/tif/";
  std::string colorDir = dataDir + "/color/";
  std::string debugDir = dataDir + "/debug/";
  int startIdx = conf.getInt("startIdx");
  int endIdx = conf.getInt("endIdx");
  std::vector<int> targetSize(2, 600);
  conf.getIntVector("targetSize", targetSize);
  bool saveWarped = true;
  conf.getBool("saveWarped", saveWarped);
  float platformStep = 2 * 0.02;//mm
  conf.getFloat("platformStep", platformStep);
  cv::Mat HScan, HColor;
  readHMats(hFile, HColor, HScan);
  std::vector<int> margin(2, 50);
  conf.getIntVector("margin", margin);
  HColor.at<float>(0, 2) += margin[0];
  HColor.at<float>(1, 2) += margin[1];
  float depthOffset = 155;
  conf.getFloat("depthOffset", depthOffset);

  //hard coded values. should be config.
  float scanRes[3] = { 0.05,0.05,0.005 };
  float voxelRes[3] = { 0.042, 0.04233, 0.025 };
  std::vector<cv::Vec3w> matColor(2);
  matColor[0] = cv::Vec3w(57604, 57425, 57280);
  matColor[1] = cv::Vec3w(25877, 9685, 8907);
  const unsigned short MIN_COLOR_NORM = 256;
  ///
  const float heightRange = 0.1;
  cv::Mat heightImg=cv::Mat::zeros(heightMaps[0].rows, heightMaps[0].cols, CV_16UC1);
  for (int i = startIdx; i <= endIdx; i++) {
    std::string colorFile = tifDir + "/jcam" + std::to_string(i) + ".tif";
    cv::Mat colorImg = cv::imread(colorFile, cv::IMREAD_UNCHANGED);
    cv::Mat colorWarped = cv::Mat::zeros(targetSize[1], targetSize[0], CV_16UC3);
    cv::warpPerspective(colorImg, colorWarped, HColor, colorWarped.size(), cv::INTER_NEAREST);
    float platCoord = (i - startIdx - 1) * platformStep;
    int printz = int(platCoord / voxelRes[2] );
    printz = std::max(0, printz);
    printz = std::min(printz, int(pvol.GetSize()[2] - 1));
    colorImages.push_back(colorWarped);
    
    int heightIdx = i - startIdx;
    if (heightIdx >= 0 && heightIdx < heightMaps.size()) {
      heightImg = heightMaps[heightIdx];
    }
    //fill in color for cropped part.
    for (int row = 0; row < colorWarped.rows; row++) {
      for (int col = 0; col < colorWarped.cols; col++) {
        cv::Vec3w fillc (matColor[0](2), matColor[0](1), matColor[0](0));
        int prow = row - margin[1];
        int pcol = col - margin[0];
        bool inside = inbound(pcol, prow, 0, pvol.GetSize()[0], pvol.GetSize()[1], 1);
        if (!inside) {
          colorWarped.at<cv::Vec3w>(row, col) = fillc;
          continue;
        }

        cv::Vec3w color = colorWarped.at<cv::Vec3w>(row, col);
        double n = cv::norm(color);
        if (n > MIN_COLOR_NORM) {
          continue;
        }
        //in this pixel inside height image
        bool inHeightImg = inbound(row, col, 0, heightImg.rows, heightImg.cols, 1);
        float height = 0;
        if (inHeightImg) {
          height = float(heightImg.at<uint16_t>(row, col) * scanRes[2]);
        }
        int zIdx = int(height / voxelRes[2]);
        if (zIdx >= 0 && zIdx < pvol.GetSize()[2]) {
          int matIdx = pvol(pcol, prow, zIdx);
          if (matIdx > 2) {
            matIdx = 2;
          }
          matIdx--;
          if (matIdx>0) {
            fillc = cv::Vec3w(matColor[matIdx](2), matColor[matIdx](1), matColor[matIdx](0));
          }
        }
        colorWarped.at<cv::Vec3w>(row, col) = fillc;
      }
    }
    colorImages.push_back(colorWarped);
    //save warped color immage
    if (saveWarped) {
      std::string warpedFile = colorDir + std::to_string(i) + ".png";
      cv::imwrite(warpedFile, colorWarped);
      std::cout << "saved " << warpedFile << "\n";

      //save print slice image with margin.
      cv::Mat printSlice = cv::Mat::zeros(pvol.GetSize()[1] + margin[1], pvol.GetSize()[0] + margin[0], CV_8UC1);
      for (int row = 0; row < printSlice.rows; row++) {
        for (int col = 0; col < printSlice.cols; col++) {
          int prow = row - margin[1];
          int pcol = col - margin[0];
          bool inside = inbound(pcol, prow, 0, pvol.GetSize()[0], pvol.GetSize()[1], 1);
          if (!inside) {
            continue;
          }
          printSlice.at<uint8_t>(row, col) = pvol(pcol, prow, printz) * 100;
        }
      }
      std::string sliceFile = colorDir + "/slice" + std::to_string(printz) + ".png";
      cv::imwrite(sliceFile, printSlice);
    }
  }
}

void loadColorImages(const ConfigFile& conf, std::vector<cv::Mat>& colorImages)
{
  std::string dataDir = conf.getString("dataDir");
  std::string colorDir = dataDir + "/color/";
  int startIdx = conf.getInt("startIdx");
  int endIdx = conf.getInt("endIdx");
  for (int i = startIdx; i <= endIdx; i++) {
    std::string warpedFile = colorDir + std::to_string(i) + ".png";
    cv::Mat img = cv::imread(warpedFile, cv::IMREAD_UNCHANGED);
    std::cout << "loaded " << warpedFile << "\n";
    colorImages.push_back(img);
  }
}

void StackColorVol(const ConfigFile & conf)
{
  //read configs
  std::string dataDir = conf.getString("dataDir");
  std::string colorDir = dataDir + "/color/";
  std::string objDir = dataDir + "/obj/";

  std::string objDir0 = objDir + "/0/";
  std::string objDir1 = objDir + "/1/";
  createDir(objDir0);
  createDir(objDir1);

  //load print data
  Array3D<uint8_t> printVol;
  processPrintData(conf, printVol);
  flipY(printVol);
  float voxelRes[3] = { 0.042, 0.04233, 0.025 };
  std::string outFile0 = objDir0 + std::to_string(0) + "p.obj";
  SaveVolAsObjMesh(outFile0, printVol, voxelRes, 1);
  std::string outFile1 = objDir1 + std::to_string(0) + "p.obj";
  SaveVolAsObjMesh(outFile1, printVol, voxelRes, 3);
  flipY(printVol);
  //load depth data
  //warp depth data so that it's aligned to print data
  
  //uses encoder position for alignment
  //cleanup depth data with input print model
  
  //uint16
  std::vector<cv::Mat> heightMaps;
  std::string heightMapFile = conf.getString("printHM");
  cv::Mat hm = cv::imread(heightMapFile, cv::IMREAD_UNCHANGED);
  if (hm.rows == 0) {
    hm=printHeightMap(printVol);
    cv::imwrite(heightMapFile, hm);
  }
  processDepthData(conf, hm, heightMaps);
  std::vector<cv::Mat>colorImages;
  //warpColorImages(conf, printVol, heightMaps, colorImages);
  //save warped color images
  bool saveWarped = true;
  //output grid size
  std::vector<int> targetSize(2, 600);
  conf.getIntVector("targetSize", targetSize);
  //how many pixels around print data to leave blank.
  std::vector<int> margin(2, 50);
  Array3D<uint8_t> outvol;
  //add 5 layers of slack.
  int numOutLayers = printVol.GetSize()[2]+5;
  conf.getInt("numLayers", numOutLayers);
  outvol.Allocate(targetSize[0], targetSize[1], numOutLayers);

  float scanRes[3] = { 0.05,0.05,0.005 };
  float platformStep = 2 * 0.02;//mm
  conf.getFloat("platformStep", platformStep);  

  int startIdx = conf.getInt("startIdx");
  int endIdx = conf.getInt("endIdx");
  float volOffset = 0.0f;
  
  std::vector<cv::Vec3f> matColor(2);
  matColor[0] = cv::Vec3f(57604.27219, 57425.84681, 57280.34856);
  matColor[1] = cv::Vec3f(25877.88700, 9685.43295, 8907.64702);

  cv::Mat prevColorImage = cv::Mat::zeros(targetSize[1], targetSize[0], CV_16UC3);
  cv::Mat prevLabel = cv::Mat::zeros(targetSize[1], targetSize[0], CV_8UC1);
  cv::Mat prevHeight = cv::Mat::zeros(targetSize[1], targetSize[0], CV_16UC1);;
  std::string debugDir = dataDir + "/debug";
  createDir(debugDir);

  const unsigned short MIN_COLOR_NORM = 256;
  
  for (int i = startIdx; i <= endIdx; i++) {
    std::string debugFile = debugDir + "/label" + std::to_string(i) + ".png";
    std::string colorFile = colorDir + std::to_string(i) + ".png";
    cv::Mat debugImg = cv::Mat::zeros (targetSize[1], targetSize[0], CV_8UC3);

    cv::Mat colorWarped = cv::imread(colorFile, cv::IMREAD_UNCHANGED);
    cv::Mat hsvImg, hsvPrev;
    cv::Mat fImg, fprev;
    cv::Mat labelImg = cv::Mat::zeros(targetSize[1], targetSize[0], CV_8UC1);
    colorWarped.convertTo(fImg, CV_32FC3);
    prevColorImage.convertTo(fprev, CV_32FC3);
    cv::cvtColor(fImg, hsvImg, cv::COLOR_RGB2HSV);
    cv::cvtColor(fprev, hsvPrev, cv::COLOR_RGB2HSV);
    //platform z coordinate in mm
    float platCoord = (i - startIdx) * platformStep;
    int printz0 = int(platCoord / voxelRes[2]);
    cv::Mat heightMap = heightMaps[i-startIdx];
    //if (i == 60) {
      std::string objFile = sequenceFilename(i - startIdx, objDir.c_str(), 0, 3, ".obj");
      SaveObjWithTexture(heightMap, objFile);
    //}
    for (int y = 0; y < colorWarped.rows; y++) {
      for (int x = 0; x < colorWarped.cols; x++) {
        cv::Vec3f color = fImg.at<cv::Vec3f>(y, x);
        cv::Vec3f hc = hsvImg.at<cv::Vec3f>(y, x);
        cv::Vec3f pColor(0, 0, 0);
        cv::Vec3f hprev = hsvPrev.at<cv::Vec3f>(y, x);
        pColor = fprev.at<cv::Vec3f>(y, x);
        int guess = 0;
        int printMat = 0;
        int printx = x - margin[0];
        int printy = y - margin[1];
        uint16_t heightVal = heightMap.at<uint16_t>(y, x);
        uint16_t prevHeightVal = prevHeight.at<uint16_t>(y, x);
        float fheight = heightVal * scanRes[2];
        int printz = int(fheight / voxelRes[2]);        
        if (inbound(printx, printy, printz,
          int(printVol.GetSize()[0]), int(printVol.GetSize()[1]), int(printVol.GetSize()[2]))) {
          printMat = printVol(printx, printy, printz);
          if (printMat > 1) {
            //material 2 and 3 are mapped to 1 and 2
            printMat--;
          }
        }

        uint8_t pl = prevLabel.at<uint8_t>(y, x);
        guess = inferMaterial(matColor, color, hc, pColor, hprev, pl);

        cv::Vec3b c(0,0,0);
        if (guess < 0) {
          guess = printMat;
          if (guess == 3) {
            guess = 2;
          }
        }
        labelImg.at<uint8_t>(y, x) = guess;

        //height did not increase, not material was added.
        if (prevHeightVal + 3 >= heightVal) {
          guess = 0;
        }
        if (guess == 1) {
          debugImg.at<cv::Vec3b>(y, x) = cv::Vec3b(255, 255, 255);
        }
        else if (guess == 2) {
          debugImg.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 255);
        }

        int z0 = int(fheight / voxelRes[2]);
        z0 = std::min( int(outvol.GetSize()[2]) - 1, z0);
        for (int z = z0; z >= 0; z--) {
          uint8_t oldVal = outvol(x, y, z);
          if (oldVal > 0) {
            //hit data from previous scan.
            break;
          }
          outvol(x, y, z) = printMat;          
        }
      }
    }
    prevColorImage = colorWarped;
    prevLabel = labelImg;
    prevHeight = heightMap;
    std::cout << "i " << i << "\n";
    cv::imwrite(debugFile, debugImg);
    std::cout << "wrote " << debugFile << "\n";
    if (i > endIdx-3) {
      flipY(outvol);
      std::string outFile0 = objDir0 + std::to_string(i) + ".obj";
      SaveVolAsObjMesh(outFile0, outvol, voxelRes, 1);
      std::string outFile1 = objDir1 + std::to_string(i) + ".obj";
      SaveVolAsObjMesh(outFile1, outvol, voxelRes, 2);
      flipY(outvol);
    }
  }
  SavePrintVol(outvol, dataDir + "/output.pvol");
}
