#pragma once
#include <string>
#include <opencv2\opencv.hpp>
void writeHMats(std::string file, const cv::Mat&HInput, const cv::Mat&HRef);
void readHMats(std::string file, cv::Mat&HInput, cv::Mat&HRef);
void writeCvMatFloat(const cv::Mat & m, std::ostream & o);
void readCvMatFloat(cv::Mat & m, std::istream & in);