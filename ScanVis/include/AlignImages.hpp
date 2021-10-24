#pragma once
#include <vector>
#include "opencv2\opencv.hpp"
///\param inputImage CV16UC3
///\param hInput 3x3 float homography matrix for input
///\param hRef reference image homography matrix.
void alignImages(const cv::Mat & inputImage, const cv::Mat & refImage,
  cv::Mat & hInput, cv::Mat & hRef);