#include "OctAnalysis.h"
#include "ArrayUtil.hpp"
#include "FileUtil.hpp"
#include "Image.h"

#include <opencv2\opencv.hpp>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

void processPrintImageFile(std::string fileName, const std::string & outputDir) {
    cv::Mat inputImage = cv::imread(fileName);
    //shrink x to 1/3 size so that the xy resolution is now
    //0.42x0.4233um.
    cv::Size inputSize = inputImage.size();
    inputSize.width /= 3;
    //INTER_NEARST maintain exact pixel values for looking up material ids.
    cv::resize(inputImage, inputImage, inputSize, 0.0, 0.0, cv::INTER_NEAREST);
    //print data is saved upside down
    //flip it back.
    cv::flip(inputImage, inputImage, 0);
    cv::Mat outputImage = cv::Mat::zeros(inputSize, CV_8UC1);

    for (int i = 0; i < inputSize.height; i++) {
        for (int j = 0; j < inputSize.width; j++) {
            cv::Vec3b pixel = inputImage.at<cv::Vec3b>(i, j);
            int mat = 0;
            //pixels with material have blue > 125.
            if (pixel[0] > 125){
                //if red is also >=125, pixel is red, which is support material.
                if (pixel[2] >= 125) {
                    mat = 1;
                }
                else {
                    mat = 2;
                }
            }
            outputImage.at<uint8_t>(i, j) = mat * 100;
        }
    }

    fs::path fullpath(fileName);
    std::string file = fullpath.filename().string();
    std::string noSuffix = file.substr(0, file.size() - 4);
    std::string idxString = file.substr(4, noSuffix.size() - 4);
    std::string outName = outputDir + "/" + idxString + ".png";
    cv::imwrite(outName, outputImage);
}

void extractPrintData(const ConfigFile & conf)
{
    std::string dataDir = conf.getString("dataDir");
    std::string printDataDir = dataDir + "/printData/";
    std::string outputDir = dataDir + "/printDataOut/";
    fs::path printDataOut(outputDir);
    if (!fs::exists(printDataOut)) {
        fs::create_directory(printDataOut);
    }
    fs::path printDataPath(printDataDir);
    if (!fs::exists(printDataPath)) {
        std::cout << "print data path " << printDataDir << " does not exist.\n";
        return;
    }
    std::string suffix (".png");
    for (auto& p: fs::directory_iterator(printDataPath)){
        if (fs::is_directory(p.status())) {
            continue;
        }
        std::string filename = p.path().string();
        if (filename.substr(filename.size() - 4, 4) == suffix) {
            processPrintImageFile(filename, outputDir);
        }
    }
}

void loadBinImage(std::string filename, ImageF32& img) {
    size_t width, height;
    loadFloatImage(filename, img.data, width, height);
    img.size[0] = width;
    img.size[1] = height;
}

//resize and translate print data of the first layer to produce
//an overall mask for the print in the scanner coordinate space.
cv::Mat scanSpaceMask(const ConfigFile & conf, int scanWidth, int scanHeight) {
    std::string dataDir = conf.getString("dataDir");
    int footprintIdx = conf.getInt("footprintIdx");
    std::string printDir = dataDir + "/printDataOut/";
    std::string footprintFile = printDir + std::to_string(footprintIdx) + ".png";

    std::vector<float> scanRes = conf.getFloatVector("scanRes");
    std::vector<float> printRes = conf.getFloatVector("printRes");
    std::vector<float> scanOrigin = conf.getFloatVector("scanOrigin");
    
    //dilate the print data mask to cover material flowing outside of print data.
    int dilate_size = 5;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(2 * dilate_size + 1, 2 * dilate_size + 1),
        cv::Point(dilate_size, dilate_size));

    cv::Mat footprint, mask;
    footprint = cv::imread(footprintFile, cv::IMREAD_GRAYSCALE);
    cv::resize(footprint, footprint, cv::Size(0, 0), printRes[0] / scanRes[0], printRes[1] / scanRes[1], cv::INTER_NEAREST);
    cv::dilate(footprint, footprint, element);
    mask = cv::Mat::zeros(cv::Size(scanWidth, scanHeight), CV_8UC1);
    for (int row = 0; row < footprint.rows; row++) {
        int scanRow = clamp(0, scanHeight - 1, row + (int)scanOrigin[1]);
        if (scanRow != row + (int)scanOrigin[1]) {
            continue;
        }
        for (int col = 0; col < footprint.cols; col++) {
            int scanCol = clamp(0, scanWidth - 1, col + (int)scanOrigin[0]);
            if (scanCol != col + (int)scanOrigin[0]) {
                continue;
            }
            mask.at<uint8_t>(scanRow, scanCol) = footprint.at<uint8_t>(row, col);
        }
    }
    cv::imwrite("mask.png", mask);
    return mask;
}

//mask scan images using printdata to remove scan noise
//outside of the printed model . 
//print data is dilated to catch ink spreading outside
//of the model to be printed.
void maskScans(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string scanDir = dataDir + "/tracking/";
    std::string printDir = dataDir + "/printDataOut/";
    std::string printOutDir = dataDir + "/printSplit/";
    std::string outputDir = dataDir + "/masked/";
    fs::path outPath(outputDir);
    if (!fs::exists(outPath)) {
        fs::create_directory(outPath);
    }
    fs::path printOutPath(printOutDir);
    if (!fs::exists(printOutDir)) {
        fs::create_directory(printOutDir);
    }

    //scanIdx0 is the depth image before printing the next layers.
    //after printing layers starting at printIdx0, the next two scans
    //contains depth for each material pass
    int scanIdx0 = conf.getInt("scanIdx0");
    int scanIdxEnd = conf.getInt("scanIdxEnd");
    int printIdx0 = conf.getInt("printDataIdx0");
        
    std::string scanFile = scanDir + std::to_string(0) + ".bin";
    std::string printDataFile = printDir + std::to_string(printIdx0) + ".png";

    //x y and coordinate in the scan image that is the top left corner of print data.
    std::vector<float> scanOrigin = conf.getFloatVector("scanOrigin");
    if (scanOrigin.size() != 2) {
        std::cout << "maskScans Warning: scanOrigin has wrong size.\n";
        scanOrigin.resize(2, 0);
    }
    std::vector<float> scanRes = conf.getFloatVector("scanRes");
    std::vector<float> printRes = conf.getFloatVector("printRes");

    ImageF32 baseScan, scan1, scan2;
    loadBinImage(scanFile, baseScan);
    cv::Mat printData, mask[2];

    //dilate the print data mask to cover material flowing outside of print data.
    int dilate_size = 3;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(2 * dilate_size + 1, 2 * dilate_size + 1),
        cv::Point(dilate_size, dilate_size));
    cv::Mat diff[2];

    float zStep = printRes[2] / scanRes[2];
    const int debugOffset = 200;
    for (int i = scanIdx0 + 1; i <= scanIdxEnd; i+=2) {
        scanFile = scanDir + std::to_string(i-scanIdx0) + ".bin";
        loadBinImage(scanFile, scan1);
        scanFile = scanDir + std::to_string(i-scanIdx0 +1) + ".bin";
        loadBinImage(scanFile, scan2);

        mask[0] = cv::Mat::zeros(cv::Size(scan1.size[0], scan1.size[1]), CV_8UC1);
        mask[1] = cv::Mat::zeros(cv::Size(scan1.size[0], scan1.size[1]), CV_8UC1);
       
        int printIdx = printIdx0 + (i - scanIdx0) / 2;
        printDataFile = printDir + std::to_string(printIdx) + ".png";
        printData = cv::imread(printDataFile, cv::IMREAD_GRAYSCALE);
        cv::resize(printData, printData, cv::Size(0, 0), printRes[0] / scanRes[0], printRes[1] / scanRes[1], cv::INTER_NEAREST);
       
        for (int row = 0; row < printData.rows; row++) {
            int scanRow = clamp(0, (int)scan1.size[1] - 1, row + (int)scanOrigin[1]);
            if (scanRow != row + (int)scanOrigin[1]) {
                continue;
            }
            for (int col = 0; col < printData.cols; col++) {
                int printVal = printData.at<uint8_t>(row, col);
                
                int scanCol = clamp(0, (int)scan1.size[0] - 1, col + (int)scanOrigin[0]);
                if (scanCol != col + (int)scanOrigin[0]) {
                    continue;
                }

                if (printVal == 100) {
                    mask[0].at<uint8_t>(scanRow, scanCol) = 255;
                }else if (printVal == 200) {
                    mask[1].at<uint8_t>(scanRow, scanCol) = 255;
                }
            }
        }

        std::string printOutFile = printOutDir + std::to_string(i - scanIdx0 - 1) + ".png";
        cv::imwrite(printOutFile, mask[0]);
        printOutFile = printOutDir + std::to_string(i - scanIdx0 ) + ".png";
        cv::imwrite(printOutFile, mask[1]);

        cv::dilate(mask[0], mask[0], element);
        cv::dilate(mask[1], mask[1], element);

        for (int row = 0; row < printData.rows; row++) {
            int scanRow = clamp(0, (int)scan1.size[1] - 1, row + (int)scanOrigin[1]);
            if (scanRow != row + (int)scanOrigin[1]) {
                continue;
            }
            for (int col = 0; col < printData.cols; col++) {
                int scanCol = clamp(0, (int)scan1.size[0] - 1, col + (int)scanOrigin[0]);
                if (scanCol != col + (int)scanOrigin[0]) {
                    continue;
                }
            }
        }
        float diffThresh1 = 1;
        float diffThresh2 = -4;
        diff[0] = cv::Mat::zeros(cv::Size(scan1.size[0], scan1.size[1]), CV_8UC3);
        diff[1] = cv::Mat::zeros(cv::Size(scan1.size[0], scan1.size[1]), CV_8UC3);
        for (int row = 0; row < scan1.size[1]; row++) {
            for (int col = 0; col < scan1.size[0]; col++) {
                uint8_t maskVal0 = mask[0].at<uint8_t>(row, col);
                uint8_t maskVal1 = mask[1].at<uint8_t>(row, col);
                float scanVal1 = scan1(col, row);
                float scanVal2 = scan2(col, row);
                float baseVal = baseScan(col, row);
                if (scanVal1 > 300 || scanVal2 > 300) {
                    continue;
                }
                float debugVal = scanVal1 - zStep - debugOffset;
                uint8_t redval = debugVal;
                if (maskVal0 > 0 && scanVal1-zStep-baseVal<-2) {
                 //   redval = 100;
                }
                diff[0].at<cv::Vec3b>(row, col) = cv::Vec3b(debugVal, debugVal, redval);
                debugVal = scanVal2 - zStep - debugOffset;
                redval = debugVal;
                if (maskVal1 > 0 && scanVal2 - scanVal1 <-2) {
                 //   redval = 200;
                }
                diff[1].at<cv::Vec3b>(row, col) = cv::Vec3b(debugVal, debugVal, redval);
            }
        }
        std::string debugFile = outputDir + std::to_string(i) + ".png";
        cv::imwrite(debugFile, diff[0]);
        debugFile = outputDir + std::to_string(i+1) + ".png";
        cv::imwrite(debugFile, diff[1]);
        baseScan = scan1;
    }
}

float leastSquares1D(const std::vector<float> & history) {
    double sumX = 0, sumY = 0;
    double meanY = 0;
    for (size_t i = 0; i < history.size(); i++) {
        meanY += history[i];
    }
    meanY /= (double)history.size();
    for (size_t i = 0; i < history.size(); i++) {
        float x = (i - history.size() / 2.0f + 0.5f);
        sumX += x * x;
        sumY += x * (history[i] - meanY);
    }
    double beta = sumY / sumX;
    double val = beta * (-(double)history.size() / 2.0f + 0.5f) + meanY;

    return (float)val;
}

void trackDepth(ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string scanDir = dataDir + "/scan/";
    std::string printDir = dataDir + "/printDataOut/";
    std::string outputDir = dataDir + "/tracking/";
    fs::path outPath(outputDir);
    if (!fs::exists(outPath)) {
        fs::create_directory(outPath);
    }

    //scanIdx0 is the depth image before printing the next layers.
    //after printing layers starting at printIdx0, the next two scans
    //contains depth for each material pass
    int scanIdx0 = conf.getInt("scanIdx0");
    int scanIdxEnd = conf.getInt("scanIdxEnd");
    int foregroundIdx = conf.getInt("foregroundIdx");

    std::string scanFile = scanDir + std::to_string(scanIdx0) + ".bin";
    
    //x y and coordinate in the scan image that is the top left corner of print data.
    std::vector<float> scanOrigin = conf.getFloatVector("scanOrigin");
    if (scanOrigin.size() != 2) {
        std::cout << "maskScans Warning: scanOrigin has wrong size.\n";
        scanOrigin.resize(2, 0);
    }
    
    ImageF32 scan, foreground;
    cv::Mat printData, mask, foregroundMask;
    //compute foreground from a clean depth scan by threshholding.
    float foregroundThresh = 0;
    conf.getFloat("foregroundThresh", foregroundThresh);
    std::string foregroundFile = scanDir + std::to_string(foregroundIdx) + ".bin";
    loadBinImage(foregroundFile, foreground);
    foregroundMask = cv::Mat::zeros(foreground.size[1], foreground.size[0], CV_8UC1);
    for (int row = 0; row < foreground.size[1]; row++) {
        for (int col = 0; col < foreground.size[0]; col++) {
            if (foreground(col, row) < foregroundThresh) {
                foregroundMask.at<uint8_t>(row, col) = 255;
            }
        }
    }
    cv::imwrite("foreground.png", foregroundMask);
    const int minHistorySize = 3;
    for (int i = scanIdx0; i <= scanIdxEnd; i ++) {
        scanFile = scanDir + std::to_string(i) + ".bin";
        loadBinImage(scanFile, scan);
        if (mask.size[0] == 0) {
            mask = scanSpaceMask(conf, scan.size[0], scan.size[1]);
        }

        ImageF32 track = scan;

        for (int row = 0; row < track.size[1]; row++) {
            for (int col = 0; col < track.size[0]; col++) {
                if (mask.at<uint8_t>(row, col) < 10 || foregroundMask.at<uint8_t>(row, col)<10) {
                    track(col, row) = 400;
                }
            }
        }

        std::string filename = outputDir + "/" + std::to_string(i-scanIdx0) + ".bin";
        saveFloatImage(filename, track.data, track.size[0], track.size[1]);

        filename = outputDir + "/" + std::to_string(i-scanIdx0) + ".png";
        cv::Mat debugImg(cv::Size(track.size[0], track.size[1]), CV_8UC1, track.data.data());

        float matThresh = -1;
        if ((i - scanIdx0) % 2 == 0) {
            matThresh = -4;
        }

        for (int row = 0; row < track.size[1]; row++) {
            for (int col = 0; col < track.size[0]; col++) {
                float val = track(col, row);
                unsigned char b = (unsigned char)(val - 200);
                debugImg.at<uint8_t>(row, col) = b;
            }
        }
        cv::imwrite(filename, debugImg);
    }
}

void makeTrainingData(ConfigFile & conf) {
  std::string dataDir = conf.getString("dataDir");
  std::string outDir = dataDir + "/depth/";
  int nSlices = conf.getInt("trainingLayers");
  //extractPrintData(conf);
  //trackDepth(conf);
  maskScans(conf);
}

bool loadBinSeq(int idx, std::string seqDir, const ConfigFile & conf,std::vector<ImageU8> & images)
{
  std::string imagePrefix = conf.getString("depthImagePrefix");
  std::string suffix = ".bin";
  if (conf.hasOpt("depthImageSuffix")) {
    suffix = conf.getString("depthImageSuffix");
  }
  int padding = 0;
  conf.getInt("filenamePadding", padding);

  std::string filename = sequenceFilename(idx, seqDir.c_str(), imagePrefix.c_str(), padding, suffix.c_str());
  size_t width = 0, height = 0;
  std::vector<float> floatImage;
  loadFloatImage(filename, floatImage, width, height);

  if (floatImage.size() > 0) {
    int nImages = height;
    images.resize(nImages);
  }

  if (floatImage.size() == 0) {
    //try loading png version
    filename = sequenceFilename(idx, seqDir.c_str(), imagePrefix.c_str(), padding, ".png");
    cv::Mat img = cv::imread(filename, cv::IMREAD_GRAYSCALE);

    floatImage.resize(img.rows*img.cols);
    for (int i = 0; i < img.rows; i++) {
      for (int j = 0; j < img.cols; j++) {
        floatImage[i*img.cols + j] = (float)img.at<uint8_t>(i, j);
      }
    }
  }
  if (floatImage.size() == 0) {
    return false;
  }
  cv::Mat cvFloat(cv::Size(width, height), CV_32FC1, floatImage.data());
  int morph_size = 1;
  cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT,
    cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
    cv::Point(morph_size, morph_size));
  cv::dilate(cvFloat, cvFloat, element);

  //make the depth thicker.
  int thickness = 10;
  int zoffset = 0;
  for (size_t i = 0; i < images.size(); i++) {
    ImageU8 & img = images.at(i);
    img.size[0] = 1024;
    img.size[1] = width;
    img.allocate();
    std::fill(img.data.begin(), img.data.end(), 0);
    for(int iy = 0; iy< img.size[1]; iy++){
      int x0 = floatImage[iy + i * width];
      for (int ix = x0; ix < x0 + thickness; ix++) {
        if (ix >= img.size[1]) {
          break;
        }
        img[iy*img.size[1] + ix] = 255 - (ix - x0);
      }
    }
  }

  return true;
}


bool loadImgSeq(std::string seqDir, const ConfigFile & conf,
  std::vector<ImageU8> & images)
{
  fs::path filepath;
  fs::path dirPath(seqDir);
  if (!fs::exists(dirPath)) {
      std::cout << "loadImgSeq: " + seqDir + " does not exist.";
    return false;
  }
  if (fs::is_empty(dirPath)) {
      std::cout << "loadImgSeq: " + seqDir + " is empty.";
    return false;
  }
  for (auto p : fs::directory_iterator(seqDir)) {
    filepath = p.path();
    break;
  }
  std::string imagePrefix = conf.getString("imagePrefix");
  std::string imageSuffix = conf.getString("imageSuffix");
  int padding = 0;
  conf.getInt("filenamePadding", padding);

  int imageBeginIndex = 1, imageEndIndex = 1024;
  conf.getInt("imageBeginIndex", imageBeginIndex);
  conf.getInt("imageEndIndex", imageEndIndex);
  int numImages = imageEndIndex - imageBeginIndex + 1;

  int imageFileIndex = 1;
  int imageCount = 0;
  
  imageEndIndex = std::max(imageEndIndex, 1);
  imageBeginIndex = std::max(imageBeginIndex, 1);
  
  images.resize(imageEndIndex - imageBeginIndex + 1);

  std::vector<int> xoffset(images.size(), 0);
  if (conf.hasOpt("xlist")) {
    bool succeed = loadArr1d<int>(seqDir + "/" + conf.getString("xlist"), xoffset);
  }
  int height = 1024;
  for (int imageFileIndex = imageBeginIndex; imageFileIndex <= imageEndIndex; imageFileIndex++) {
    std::string filename = sequenceFilename(imageFileIndex, seqDir.c_str(), imagePrefix.c_str(), padding, imageSuffix.c_str());
    fs::path filepath(filename);
    cv::Mat imgbuf = cv::imread(filename, cv::IMREAD_GRAYSCALE);
    if (imgbuf.data == NULL) {
      std::cerr << "Cannot load image " << filename << "\n";
      break;
    }
    ImageU8 & img = images[imageCount];
    img.size[1] = imgbuf.rows;
    img.size[0] = height;
    img.allocate();
    std::fill(img.data.begin(), img.data.end(), 0);

    for (int iy = 0; iy < imgbuf.rows; iy++) {
      for (int ix = 0; ix < imgbuf.cols; ix++) {
        int x = clamp(ix + xoffset[imageCount], 0, (int)img.size[0] - 1);
        int dstIdx = iy * img.size[0] + x;
        img[dstIdx] = imgbuf.at<uint8_t>(iy, ix);
      }
    }
    imageCount++;
  }

  std::cout << "loaded " << imageCount << " images.\n";
  images.resize(imageCount);
  return true;
}

void computeBrightness(const std::vector<uint8_t> & volume,
  const int * volumeSize, const std::vector<float> & depth,
  std::vector<uint8_t> & brightness)
{
  int windowSize = 5;
  brightness.resize(volumeSize[0] * volumeSize[1], 0);
  for (int row = 0; row < volumeSize[1]; row++) {
    for (int col = 0; col < volumeSize[0]; col++) {
      
      int imgIdx = row * volumeSize[0] + col;
      int z = (int)depth[imgIdx];
      int b = 0;
      for (int w = z - windowSize; w <= z + windowSize; w++) {
        int d = clamp(w, 0, volumeSize[2] - 1);
        int volIdx = gridToLinearIdx(col, row, d, volumeSize);
        b+= volume[volIdx];
      }
      brightness[imgIdx] = b / (2 * windowSize + 1);

    }
  }
}

void computeNormal(const uint32_t * surfSize, const std::vector<float> & depth,
  std::vector<uint8_t> & normal)
{
  const int WINDOW_ROW = 3;
  const int WINDOW_COL = 3;
  cv::Mat filterX = (cv::Mat_<float>(WINDOW_ROW, WINDOW_COL) << -3,0,3,-10,0,10,-3,0,3 );
  cv::Mat filterY = (cv::Mat_<float>(WINDOW_ROW, WINDOW_COL) << -3,-10,-3,0,0,0,3,10,3 );
  float gradientScale = 1;
  filterX *= gradientScale;
  filterY *= gradientScale;

  normal.resize(surfSize[0] * surfSize[1] * 3, 0);

  cv::Size imgSize(surfSize[0], surfSize[1]);
  cv::Mat cvDepth(imgSize, CV_32FC1, (char*)depth.data());
  cv::Mat filteredX, filteredY;
  cv::filter2D(cvDepth, filteredX, -1, filterX);
  cv::filter2D(cvDepth, filteredY, -1, filterY);
  normal.resize(3 * surfSize[0] * surfSize[1]);
  for (int row = 0; row < surfSize[1]; row++) {
    for (int col = 0; col < surfSize[0]; col++) {
      int imgIdx = 3 * (row*surfSize[0] + col);
      normal[imgIdx] = (uint8_t)filteredX.at<float>(row, col) + 125;
      normal[imgIdx + 1] = (uint8_t)filteredY.at<float>(row, col) + 125;
      normal[imgIdx + 2] = 125;
    }
  }
}

void computeReflectance(const std::vector<uint8_t> & volume,
  const int * volumeSize, const std::vector<float> & depth,
  const ConfigFile & conf, std::vector<float> & angles, std::vector<float> & r)
{
  uint32_t surfSize[2];
  surfSize[0] = volumeSize[0];
  surfSize[1] = volumeSize[1];
  cv::Size imgSize(volumeSize[0], volumeSize[1]);
  std::vector<uint8_t> brightness;
  computeBrightness(volume, volumeSize, depth, brightness);
  cv::Mat cvBright(imgSize, CV_8UC1, brightness.data());
  cv::imwrite(conf.getString("outputDir") + "/brightness.png", cvBright);

  std::vector<uint8_t> normal;
  computeNormal(surfSize,  depth, normal);
  cv::Mat cvNormal(imgSize, CV_8UC3, normal.data());
  cv::imwrite(conf.getString("outputDir") + "/normal.png", cvNormal);

  cv::Mat mask;
  std::string maskFile = conf.getString("maskFile");
  mask = cv::imread(maskFile, cv::IMREAD_GRAYSCALE);
  
  float zRes = 64;//um
  float xRes = 50;
  float yRes = 50;
  int maxDist = 200;
  int binSize = 5;
  int nBins = maxDist / binSize;
  angles.resize(nBins);
  r.clear();
  r.resize(nBins,0);
  std::vector<int> count(nBins, 0);
  for (int i = 0; i < nBins; i++) {
    angles[i] = i * binSize;
  }
  for (int row = 0; row < volumeSize[1]; row++) {
    for (int col = 0; col < volumeSize[0]; col++) {
      if (mask.at<uint8_t>(row, col) < 125) {
        continue;
      }
      int imgIdx = row * volumeSize[1] + col;
      uint8_t b = brightness[imgIdx];
      float dx = normal[3 * imgIdx] - 125.0;
      float dy = normal[3 * imgIdx + 1] - 125.0;
      float dx2 = dx * dx;
      float dy2 = dy * dy;
      float slope = std::sqrt(dy2 + dx2);
      int binIdx = (int)(slope / binSize);
      binIdx = clamp(binIdx, 0, nBins - 1);
      r[binIdx] += b;
      count[binIdx] ++;
    }
  }
  for (size_t i = 0; i < nBins; i++) {
    if (count[i] > 0) {
      r[i] /= count[i];
    }
  }
}

void computeTilt(const std::vector<float> & img, int width, int height,
  const ConfigFile & conf, std::vector<float> & out) {
  double a, b, zcenter;
  ImageF32 floatImg;
  floatImg.size[0] = width;
  floatImg.size[1] = height;
  floatImg.data = img;
  fitZPlane(floatImg, a, b, zcenter, 20);
  std::cout << "fit z plane " << a << " " << b << " " << zcenter << "\n";
  ImageF32 outImg;
  std::vector<float> transform = conf.getFloatVector("transform");
  float transformA = -a, transformB=-b, rotateAngle=0;
  if (transform.size() == 3) {
    transformA = transform[0];
    transformB = transform[1];
    rotateAngle = transform[2];
  }
  transformImage(floatImg, outImg, transformA, transformB);
  rotateImage(outImg, floatImg, rotateAngle);
  out = floatImg.data;
}

void saveObjMesh(const ImageF32 & floatImg,
  const ConfigFile & conf, int scanIdx) {
  std::vector<int> roi;
  conf.getIntVector("depthROI", roi);
  if (roi.size() != 4) {
    return;
  }
  int cropSize[2];
  cropSize[0] = roi[2] - roi[0];
  cropSize[1] = roi[3] - roi[1];
  std::vector<float> cropped(cropSize[0] * cropSize[1]);
  for (int i = 0; i < cropSize[1]; i++) {
    for (int j = 0; j < cropSize[0]; j++) {
      cropped[i*cropSize[0] + j] = floatImg(j + roi[0], i + roi[1]);
    }
  }
  float dx[3] = { 0.05f, 0.05f, 0.05f };
  
  std::string meshFileName = conf.getString("outputDir") + "/"+ std::to_string(scanIdx) + ".obj";
  saveHeightObj(meshFileName, cropped, cropSize[0], cropSize[1], dx);
}

void saveDepthMask() {

}

