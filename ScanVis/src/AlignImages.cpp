#include "AlignImages.hpp"
#include <opencv2/opencv.hpp>

///\param img CV_8UC1
void drawCircles(cv::Mat & img, const std::vector<cv::Point2f>& o);

//\param print 3  channel color image where red channel is material id.
//convert print data image from id to depth and then blur it.
void formatAndBlurPrintData(const cv::Mat & print, cv::Mat & printDepth) {
  const int kernelSize = 15;
  double sigmaX = kernelSize / 2;
  printDepth = cv::Mat::zeros(print.size(), CV_8UC1);
  for (int row = 0; row < print.rows; row++) {
    for (int col = 0; col < print.cols; col++) {

    }
  }
  cv::GaussianBlur(printDepth, printDepth, cv::Size(kernelSize, kernelSize), sigmaX);
}

cv::Mat checkerCornerFilter(int xWindow, int yWindow) {
  //even sized kernel with 1 last row and column 0 padding.
  //actual kernel center is pixel center - (0.5, 0.5).
  int patchSize = xWindow * yWindow;
  cv::Mat kernel(2 * yWindow + 1, 2 * xWindow + 1, CV_32F, cv::Scalar(0));
  for (int i = 0; i < yWindow; i++) {
    for (int j = 0; j < xWindow; j++) {
      kernel.at<float>(i, j) = 0.25f / patchSize;
      kernel.at<float>(i + yWindow, j + xWindow) = 0.25f / patchSize;
      kernel.at<float>(i + yWindow, j) = -0.25f / patchSize;
      kernel.at<float>(i, j + xWindow) = -0.25f / patchSize;
    }
  }
  return kernel;
}

void Clahe8UC3(cv::Mat & u8Img, double clim, int cellSize) {
  cv::Mat lab;
  cv::cvtColor(u8Img, lab, cv::COLOR_RGB2Lab);
  std::vector<cv::Mat> lab_planes;
  cv::split(lab, lab_planes);
  cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clim, cv::Size(cellSize, cellSize));

  clahe->apply(lab_planes[0], lab_planes[0]);
  cv::merge(lab_planes, lab);
  cv::cvtColor(lab, u8Img, cv::COLOR_Lab2BGR);
}

void Clahe8UC1(cv::Mat & u8Img, double clim, int cellSize) {
  cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clim, cv::Size(cellSize, cellSize));
  clahe->apply(u8Img, u8Img);
}

///\return 32FC1 grayscale image
cv::Mat cornerFilterColor(const cv::Mat & inputImage, int filterSize,
  int thresh) {
  cv::Mat floatImg(inputImage.rows, inputImage.cols, CV_32FC3);
  inputImage.convertTo(floatImg, CV_32F, 1.0 / 255);

  cv::Mat cornerFilter = checkerCornerFilter(filterSize, filterSize);
  //filter response = sum abs of per channel response.
  cv::filter2D(floatImg, floatImg, CV_32F, cornerFilter);
  cv::Mat grayInput(floatImg.rows, floatImg.cols, CV_32FC1);
  floatImg = cv::abs(floatImg);
  cv::reduce(floatImg.reshape(1, floatImg.total()), grayInput, 2, cv::REDUCE_SUM);
  grayInput = grayInput.reshape(0, floatImg.cols);


  float maxResponse = 0;
  cv::MatIterator_<float> it, end;
  end = grayInput.end<float>();
  for (it = grayInput.begin<float>(); it != end; it++) {
    float val = *it;
    maxResponse = std::max(val, maxResponse);
  }
  grayInput = (255 / maxResponse) * grayInput;

  cv::threshold(grayInput, grayInput, thresh, 255, cv::THRESH_TOZERO);
  return grayInput;
}

///\return 32FC1 grayscale image
cv::Mat cornerFilterGray(const cv::Mat & inputImage, int filterSize,
  int thresh) {
  cv::Mat floatImg(inputImage.rows, inputImage.cols, CV_32FC1);
  inputImage.convertTo(floatImg, CV_32F);

  cv::Mat cornerFilter = checkerCornerFilter(filterSize, filterSize);
  //filter response = sum abs of per channel response.
  cv::filter2D(floatImg, floatImg, CV_32F, cornerFilter);
  floatImg = cv::abs(floatImg);

  float maxResponse = 0;
  cv::MatIterator_<float> it, end;
  end = floatImg.end<float>();
  for (it = floatImg.begin<float>(); it != end; it++) {
    float val = *it;
    maxResponse = std::max(val, maxResponse);
  }

  floatImg = (255 / maxResponse) * floatImg;
  cv::threshold(floatImg, floatImg, thresh, 255, cv::THRESH_TOZERO);
  return floatImg;
}


std::vector<cv::Point2i> bfs(const cv::Mat & img, std::vector<int> & labels, float thresh, int seedi, int seedj, int l) {
  const int N_NBR = 8;
  const int nbr[N_NBR][2] = { { -1,-1 },{ -1, 0 },{ -1, 1 },{ 0, -1 },
  { 0, 1 },{ 1, -1 },{ 1, 0 },{ 1, 1 } };

  std::queue <cv::Point2i> q;
  q.push(cv::Point2i(seedj, seedi));
  labels[uint32_t(seedi*img.cols + seedj)] = l;
  std::vector<cv::Point2i> comp;
  while (!q.empty()) {
    cv::Point2i p = q.front();
    q.pop();
    comp.push_back(cv::Point2f(p.x, p.y));
    for (int k = 0; k < N_NBR; k++) {
      int ni = p.y + nbr[k][0];
      int nj = p.x + nbr[k][1];
      if (ni < 0 || ni >= img.rows || nj < 0 || nj >= img.cols) {
        continue;
      }
      float val = img.at<float>(ni, nj);
      if (val < thresh || labels[uint32_t(ni * img.cols + nj)] >= 0) {
        continue;
      }
      labels[uint32_t(ni * img.cols + nj)] = l;
      q.push(cv::Point2i(nj, ni));
    }
  }

  return comp;
}
///black-white connected-component
std::vector < std::vector<cv::Point2i> > bwconncomp(const cv::Mat & img,
  float thresh, std::vector<int> & labels) {
  std::vector < std::vector<cv::Point2i> >cc;
  labels.resize(uint32_t(img.rows*img.cols), -1);
  for (int i = 1; i < img.rows - 1; i++) {
    for (int j = 1; j < img.cols - 1; j++) {
      float val = img.at<float>(i, j);
      if (val <= thresh || labels[uint32_t(i*img.cols + j)] >= 0) {
        continue;
      }
      cc.push_back(bfs(img, labels, thresh, i, j, int(cc.size())));
    }
  }
  return cc;
}

///find centroids of each blob of signal response.
///small blobs are ignored
///\param filtered image of filter response in CV_32FC1
std::vector<cv::Point2f> findCentroids(const std::vector<std::vector<cv::Point2i> > & comps,
  const cv::Mat & filtered, float thresh)
{
  std::vector<cv::Point2f> dots;
  //dots smaller than this number of pixels are removed.
  const int MIN_AREA = 100;
  //find local maximal response of each component.
  for (size_t i = 0; i < comps.size(); i++) {
    float localMax = 0.0f;
    cv::Point2f maxPos(0, 0);
    if (comps[i].size()<MIN_AREA) {
      continue;
    }
    cv::Point2f weightedSum(0, 0);
    float weight = 0;
    for (size_t j = 0; j < comps[i].size(); j++) {
      float val = filtered.at<float>(comps[i][j].y, comps[i][j].x);
      weightedSum += val * cv::Point2f(comps[i][j].x, comps[i][j].y);
      weight += val;
      if (val>localMax) {
        localMax = val;
        maxPos = cv::Point2f(comps[i][j].x, comps[i][j].y);
      }
    }

    if (localMax >= thresh) {
      //dots.push_back(maxPos);
      dots.push_back(weightedSum / weight);
    }
  }
  return dots;
}

//sort out coordinates of the detected dots
//organize them to a rectangular grid, accounting for
//missing dots and outliers.
//\return index for each dot in an nx x ny grid.
std::vector<cv::Point2i> mapDots(std::vector<cv::Point2f> & dots,
  int nx, int ny)
{
  float eps_x = 10;
  std::vector<cv::Point2i> idx;
  //sort dots by x coordinates.
  std::sort(dots.begin(), dots.end(), [](const cv::Point2f & a, const cv::Point2f & b)
  {
    return a.x<b.x;
  });

  //bin dots into columns
  float prevX = dots[0].x;
  std::vector<cv::Point2f> col;
  std::vector<std::vector<cv::Point2f> > cols;
  col.push_back(dots[0]);
  for (size_t i = 1; i<dots.size(); i++) {
    float x = dots[i].x;
    if (x - prevX>eps_x) {
      if (int(col.size()) > ny / 2) {
        //sort points in column by y coordinates
        std::sort(col.begin(), col.end(), [](const cv::Point2f & a, const cv::Point2f & b)
        {
          return a.y<b.y;
        });
        cols.push_back(col);
      }
      //finish up previous columns.
      col.clear();
    }
    col.push_back(dots[i]);
    prevX = x;
  }
  //add the last column
  if (int(col.size()) > ny / 2) {
    //sort points in column by y coordinates
    std::sort(col.begin(), col.end(), [](const cv::Point2f & a, const cv::Point2f & b)
    {
      return a.y<b.y;
    });
    cols.push_back(col);
  }
  dots.clear();
  for (size_t x = 0; x<cols.size(); x++) {
    for (size_t y = 0; y<cols[x].size(); y++) {
      dots.push_back(cols[x][y]);
      idx.push_back(cv::Point2i(int(x), int(y)));
    }
  }

  return idx;
}

void alignImages(const cv::Mat & inputImage, const cv::Mat & refImage,
  cv::Mat & hInput, cv::Mat & hRef) {
  int filterSize = 50;
  int refFilterSize = 40;
  double clim = 30;
  const int inputThresh = 200;
  const int refThresh = 150;
  const int gridx = 4;
  const int gridy = 4;
  int cellSize = 2 * filterSize;
  //process color input image
  //converted to 1-channel image
  cv::Mat inputResponse = cornerFilterColor(inputImage, filterSize, inputThresh);
  cv::Mat contrastImg = refImage;
  Clahe8UC1(contrastImg, clim, cellSize);

  std::vector<std::vector<cv::Point2i> > inputComps;
  std::vector<int> inputLabels;
  inputComps = bwconncomp(inputResponse, inputThresh, inputLabels);
  std::vector<cv::Point2f> dots = findCentroids(inputComps, inputResponse, inputThresh);
  //identify checker corner grid in input image
  cv::Mat imgu8 = cv::Mat::zeros(inputResponse.rows, inputResponse.cols, CV_8UC1);
  std::vector<cv::Point2i> dotIdx = mapDots(dots,
    gridx, gridy);
  drawCircles(imgu8, dots);
  std::cout << "write inputRes.png\n";
  cv::imwrite("inputRes.png", imgu8);
  
  cv::Mat refResponse = cornerFilterGray(refImage, refFilterSize, refThresh);
  //identify check corners in reference image.

  std::vector<std::vector<cv::Point2i> > refComps;
  std::vector<int> refLabels;
  refComps = bwconncomp(refResponse, refThresh, refLabels);
  std::vector<cv::Point2f> refDots = findCentroids(refComps, refResponse, refThresh);
  //identify checker corner grid in input image
  cv::Mat refu8 = cv::Mat::zeros(refResponse.rows, refResponse.cols, CV_8UC1);
  std::vector<cv::Point2i> refDotIdx = mapDots(refDots,
    gridx, gridy);
  drawCircles(refu8, refDots);
  std::cout << "write refRes.png\n";
  cv::imwrite("refRes.png", refu8);
  
  float targetGridLen[2] = { 100,100 };
  std::vector<cv::Point2f> targetPoints(gridx*gridy);
  for (int x = 0; x < gridx; x++) {
    for (int y = 0; y < gridy; y++) {
      targetPoints[x * gridy + y] = cv::Point2f(targetGridLen[0] * (1 + x), targetGridLen[1] * (y + 1));
    }
  }
  
  //perspective transformation
  cv::Mat persMat;
  persMat = findHomography(dots, targetPoints,cv::RANSAC);

  cv::Mat scanPers;
  scanPers = cv::findHomography(refDots, targetPoints, cv::RANSAC);

  hInput = persMat;
  hRef = scanPers;
}

void drawCircles(cv::Mat & img, const std::vector<cv::Point2f>& o)
{
  int radius = 10;
  cv::Scalar color(255);
  int thickness = 2;
  for (size_t i = 0; i < o.size(); i++) {
    cv::circle(img, o[i], radius, color, thickness);
    if (i > 0) {
      cv::line(img, o[i - 1], o[i], color, thickness);
    }
  }
}