#include "Image.h"
#include <opencv2/opencv.hpp>

void fitZPlane(const ImageF32 & image, double & a, double &b, double & zcenter, int margin) {
  zcenter = 0;
  int cnt = 0;
  for (int i = margin; i < (int)image.size[0] - margin; i++) {
    for (int j = margin; j < (int)image.size[1] - margin; j++) {
      zcenter += image(i, j);
      cnt++;
    }
  }
  zcenter /= cnt;
  double xx = 0, xy = 0, yy = 0, xz = 0, yz = 0;
  int centerx = image.size[0] / 2;
  int centery = image.size[1] / 2;
  for (int i = margin; i < (int)image.size[0] - margin; i++) {
    for (int j = margin; j < (int)image.size[1] - margin; j++) {
      int ix = i - centerx;
      int iy = j - centery;
      xx += ix * ix;
      yy += iy * iy;
      xy += ix * iy;
      xz += ix * (image(i, j) - zcenter);
      yz += iy * (image(i, j) - zcenter);
    }
  }
  cv::Mat A = (cv::Mat_<double>(2, 2) << xx, xy, xy, yy);
  cv::Mat B = (cv::Mat_<double>(2, 1) << xz, yz);
  cv::Mat x;
  cv::solve(A, B, x);
  a = x.at<double>(0, 0);
  b = x.at<double>(1, 0);
}

void rotateImage(const ImageF32 & input, ImageF32 & output, double angle) {
  output.size = input.size;
  output.allocate();
  int centerx = output.size[0] / 2;
  int centery = output.size[1] / 2;

  cv::Size imgSize((int)output.size[0], (int)output.size[1]);
  cv::Mat M = cv::getRotationMatrix2D(cv::Point2f(input.size[0] / 2.0f, input.size[1] / 2.0f), 
    angle, 1);
  output.size = input.size;
  output.allocate();
  cv::Mat src(imgSize, CV_32FC1, (char*) input.data.data());
  cv::Mat dst(imgSize, CV_32FC1, (char*)output.data.data());
  cv::warpAffine(src, dst, M, imgSize, 1, cv::BORDER_REPLICATE);
}

void transformImage(const ImageF32 & input, ImageF32 & output, double a, double b) {
  output.size = input.size;
  output.allocate();
  int centerx = output.size[0] / 2;
  int centery = output.size[1] / 2;
  for (int i = 0; i < (int)output.size[0]; i++) {
    for (int j = 0; j < (int)output.size[1]; j++) {
      output(i, j) = (float)(input(i, j) + a * (i - centerx) + b * (j - centery));
    }
  }
}
