#include "Recon.hpp"
#include "FileUtil.hpp"
#include "ArrayUtil.hpp"
#include <opencv2\opencv.hpp>

unsigned short findDepth(unsigned char * aline, uint32_t size);

void computeSurface(const ConfigFile & conf) {
    int depthIdx = 38;
    std::string dataDir = conf.getString("dataDir") + "/";
    std::string outDir = conf.getString("outDir");
    std::string volFile = dataDir +"/volume/" + std::to_string(depthIdx) + ".vol";
    std::vector<uint8_t> vol;
    int volSize[3];
    loadVolumeRaw(volFile, vol, volSize);

    cv::Mat depthImage(volSize[1], volSize[0], CV_8UC1);

    createDir(outDir);
    std::string outFileName = outDir + "/" + std::to_string(depthIdx) + ".png";
    for (int row = 0; row < depthImage.rows; row++) {
        for (int col = 0; col < depthImage.cols; col++) {
            int volIdx = col * volSize[1] * volSize[2] + row * volSize[2];
            unsigned short depth = findDepth((&vol[volIdx]), volSize[2]);
            depthImage.at<uint8_t>(row, col) = depth;
        }
    }
    cv::imwrite(outFileName, depthImage);

    std::string outFloatImage = outDir + "/" + std::to_string(depthIdx) + ".bin";
    std::vector<float> floatImg(volSize[1] * volSize[0]);
    for (int row = 0; row < depthImage.rows; row++) {
        for (int col = 0; col < depthImage.cols; col++) {
            unsigned short depth = depthImage.at<uint8_t>(row, col);
            floatImg[col + row * depthImage.cols] = depth;
        }
    }
    saveFloatImage(outFloatImage, floatImg, volSize[0], volSize[1]);
}

int maxIndex(unsigned char * a, unsigned int size) {
    unsigned char maxVal = a[0];
    int maxIdx = 0;
    for (int i = 1; i < size; i++) {
        if (a[i] > maxVal) {
            maxVal = a[i];
            maxIdx = i;
        }
    }
    return maxIdx;
}

void smooth1D(unsigned char * arr, size_t len, size_t windowSize) {
    std::vector<unsigned char> padded(len + 2 * windowSize);
    //pad boundary value
    for (size_t i = 0; i < windowSize; i++) {
        padded[i] = arr[0];
    }
    for (size_t i = 0; i < len; i++) {
        padded[i + windowSize] = arr[i];
    }
    for (size_t i = len + windowSize; i < padded.size(); i++) {
        padded[i] = arr[len - 1];
    }
    std::vector<int> sum(padded.size());
    int s = 0;
    for (size_t i = 0; i < sum.size(); i++) {
        s += padded[i];
        sum[i] = s;
    }
    for (size_t i = 0; i < len; i++) {
        arr[i] = (sum[i + 2 * windowSize - 1] - sum[i]) / (2 * windowSize - 1);
    }
}

void conv1d(float* a, float* k, float * out, uint32_t sizea, uint32_t sizek) {
  int halfk = sizek / 2;
  for (int i = 0; i < sizea; i++) {
    float sum = 0;
    for (int j = 0; j < sizek; j++) {
      int l = i + j - halfk;
      l = clamp(l, 0, int(sizea));
      sum += a[l] * k[j];
    }
    out[i] = sum;
  }
}

//find depth by running a convolution
unsigned short findDepthConv(float * aline, uint32_t size)
{
  const int kerSize = 11;
  float coef[kerSize] = { -0.98661, -0.96403, -0.90515, -0.76159, -0.46212, 
                       0, 0.46212,   0.76159,   0.90515, 0.96403,   0.98661 };
  cv::Mat line(size, 1, CV_32FC1, aline);
  cv::Mat filtered;
  cv::Mat kernelX = cv::Mat::ones(1, 1, CV_32FC1);
  cv::Mat kernelY(kerSize , 1, CV_32FC1, coef);
  //cv::filter2D(line, filtered, -1, kernelY, cv::Point(-1,-1), 0, CV_HAL_BORDER_REPLICATE);
  //std::vector<float> filtered(size);
  cv::filter2D(line, filtered, -1, kernelY, cv::Point(-1, -1), 0, CV_HAL_BORDER_REPLICATE);
  //conv1d(aline, coef, filtered.data(), size, kerSize);
  float maxv = 0.0f;
  int idx = 0;
  float thresh = 180;
  for (int row = 0; row < filtered.rows; row++) {
    float fv = filtered.at<float>(row,0);
    if (fv > maxv) {
      maxv = fv;
      idx = row;
    }
    if (fv > thresh) {
      break;
    }
  }
  return idx;
}

///\aline post-fft signal. magnitude of each frequency or depth.
///\size length of signal
unsigned short findDepth(unsigned char * aline, uint32_t size ) {
    
    unsigned char saturatedVal = 240;
    //radius for finding weighted average around maximum
    int peakRadius = 5;
    //radius for smoothing signal.
    int smoothRadius = 15;
    //threshold of gradient to confirm signal transition from
    //background to measured surface.
    short gradThreshold = 15;

    //if some signal value is saturated return the center of the peak by taking a weighted average 
    //around the peak
    //int maxIdx = maxIndex(aline, size);
    //int maxVal = aline[maxIdx];
    //if (maxVal >= saturatedVal) {
    //    int weightSum = 0;
    //    int indexSum = 0;
    //    int start = std::max(0, maxIdx - peakRadius);
    //    int end = std::min( (int)size - 1, maxIdx + peakRadius);
    //    for (int i = start; i <= end; i++) {
    //        unsigned char val = aline[i];
    //        weightSum += val;
    //        indexSum += i * val;
    //    }
    //    //peak at maximum brightness is offset
    //    //from peak in gradient.
    //    return indexSum / weightSum - 5;
    //}
    //find smoothed gradient that exceeds the threshold.
    int maxIdx = 0;
    smooth1D(aline, size, smoothRadius);
    std::vector<short> gradient(size, 0);
    int maxGrad = 0;
    for (int i = 0; i < size; i++) {
        int i0 = (i > smoothRadius)?(i-smoothRadius) : 0;
        int i1 = std::min((int)size - 1, i + smoothRadius);
        gradient[i] = std::max(0, (aline[i1] - aline[i0]));
        //if gradient is above threshold
        //look no more than pixels
        if (gradient[i] > gradThreshold) {
            i1 = std::min(i1, i+5);
            maxIdx = i;
            break;
        }
        if (maxGrad < gradient[i]) {
            maxGrad = gradient[i];
            maxIdx = i;
        }
    }
    //if no jump in gradient, 
    //signal is too weak, return a large depth.
    if (maxGrad < gradThreshold) {
        maxIdx = size - 1;
    }
    //return maximum depth if signal is low.
    return maxIdx;
}
