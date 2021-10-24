#pragma once
#include <vector>
#include <deque>
#include <string>
#include <Image.h>

const int MAX_TILE_INDEX = 100;

///\brief previous scan depths. Used
///for depth reconstructions based on previous scanned
///layers.
class DepthHistory {
public:
    
    DepthHistory();

    ~DepthHistory();
    ///\brief add new depth image to history.
    ///remove oldest image if there are more images in this tile
    ///than depthHistorySize.
    void addHistory(const std::vector<float> & depth, int w, int h);

    void clear();
    
    int getMaxSize();

    int size();

    void setMaxSize(int s);

    void setTileIndex(int t);

    std::vector<float> getHistory(int pixelIdx);

    void saveHistory(const std::string & dir);
private:

    std::vector<std::deque<ImageF32 > >depthHistory;
    std::vector<int> depthCount;

    ///\brief how many layser of history to store.
    int maxSize;

    ///\brief which tile is active   
    int tileIndex;
    
    static const int MAX_TILE_INDEX = 100;
};