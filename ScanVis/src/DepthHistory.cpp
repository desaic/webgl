#include "DepthHistory.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
DepthHistory::DepthHistory() :tileIndex(0),
maxSize(10)
{
}

DepthHistory::~DepthHistory()
{
}

void DepthHistory::addHistory(const std::vector<float> & depth, int w, int h)
{
    if (tileIndex<0 || tileIndex>MAX_TILE_INDEX) {
        return;
    }
    if (tileIndex >= (int)depthHistory.size()) {
        depthHistory.resize(size_t(tileIndex + 1));
        depthCount.resize(size_t(tileIndex + 1), 0);
    
    }
    if (depthHistory[tileIndex].size() >= maxSize) {
        depthHistory[tileIndex].pop_back();
    }
    ImageF32 img;
    img.data = depth;
    img.size[0] = w;
    img.size[1] = h;
    depthHistory[tileIndex].push_front(img);
    depthCount[tileIndex] ++;
    
}

std::vector<float> DepthHistory::getHistory(int pixelIdx)
{
    std::vector<float> h;
    if (tileIndex<0 || tileIndex>=depthHistory.size()) {
        return h;
    }
    h.resize(depthHistory[tileIndex].size(), 0);
    for (size_t i = 0; i < depthHistory[tileIndex].size(); i++) {
        int imgSize = (int)depthHistory[tileIndex][i].data.size();
        if (pixelIdx >= imgSize) {
            std::cout << "DepthHistory::getHistory warning pixelIdx>image size " << pixelIdx << " " << imgSize << "\n";
            continue;
        }
        h[i] = depthHistory[tileIndex][i][pixelIdx];
    }
    return h;
}

void DepthHistory::clear() {
    depthHistory.clear();
    tileIndex = 0;
}

int DepthHistory::getMaxSize() {
    return maxSize;
}

int DepthHistory::size() {
    if (tileIndex <0) {
        return -1;
    }
    if (depthHistory.size() <= tileIndex) {
        return 0;
    }
    return depthHistory[tileIndex].size();
}

void DepthHistory::setMaxSize(int s) {
    maxSize = s;
}

void DepthHistory::setTileIndex(int t) {
    tileIndex = t;
}

void DepthHistory::saveHistory(const std::string & dir)
{
  for (size_t i = 0; i < depthHistory.size(); i++) {
    std::string filename = dir + "/" + std::to_string(i) + "_" + std::to_string(depthCount[i]) + ".jpg";
    auto & img = depthHistory[i].front();
    cv::Mat cvimg(cv::Size(img.size[0], img.size[1]), CV_32FC1, (void *)img.data.data());
    cv::imwrite(filename, cvimg);
  }
    
}