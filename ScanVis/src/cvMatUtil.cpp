#include <fstream>
#include "cvMatUtil.hpp"

void writeHMats(std::string file, const cv::Mat&HInput, const cv::Mat&HRef) {
  std::ofstream out(file);
  out << "#color H mat\n";
  writeCvMatFloat(HInput, out);
  out << "#scan H mat\n";
  writeCvMatFloat(HRef, out);
  out.close();
}

void readHMats(std::string file, cv::Mat&HInput, cv::Mat&HRef) {
  std::ifstream in(file);
  std::string line;
  std::getline(in, line);
  readCvMatFloat(HInput, in);
  std::getline(in, line);
  std::getline(in, line);
  readCvMatFloat(HRef, in);
  in.close();
}


void writeCvMatFloat(const cv::Mat & m, std::ostream & o)
{
  int type = m.type();
  uchar depth = type & CV_MAT_DEPTH_MASK;
  bool isDouble = (depth == CV_64F);
  o << m.rows << " " << m.cols << "\n";
  for (int row = 0; row < m.rows; row++) {
    for (int col = 0; col < m.cols; col++) {
      if (isDouble) {
        o << m.at<double>(row, col) << " ";
      }
      else {
        o << m.at<float>(row, col) << " ";
      }
    }
    o << "\n";
  }
}

void readCvMatFloat(cv::Mat & m, std::istream & in)
{
  int rows = 0, cols = 0;
  in >> rows >> cols;

  if (rows > 0 && cols > 0) {
    m = cv::Mat(rows, cols, CV_32F);
  }
  else {
    std::cout << "matrix size incorrect " << rows << " " << cols << "\n";
    return;
  }
  for (int row = 0; row < m.rows; row++) {
    for (int col = 0; col < m.cols; col++) {
      in >> m.at<float>(row, col);
    }
  }
}
