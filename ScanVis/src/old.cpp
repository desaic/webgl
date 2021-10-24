#include "ConfigFile.hpp"
#include "FileUtil.hpp"
#include "ArrayUtil.hpp"
#include "Image.h"
#include "Array3D.h"
#include "opencv2/opencv.hpp"
#include <vector>
void parseRaw(const ConfigFile & conf) {
    std::string filename = conf.getString("dataFile");
    FileUtilIn in;
    in.open(filename);// , std::ifstream::binary);
    if (!in.good()) {
        return;
    }

    int lineSize, nLines;
    //in.in.read((char*)(&lineSize), sizeof(int));
    //in.in.read((char*)(&nLines), sizeof(int));
    in.in >> lineSize >> nLines;
    std::vector<int> data(lineSize*nLines);
    //in.in.read((char*)(data.data()), data.size() * sizeof(unsigned short));
    for (size_t i = 0; i < data.size(); i++) {
        in.in >> data[i];
    }
    in.close();
    int prevTrigger = -1;
    int minDist = 50;
    std::vector<std::vector<std::vector<short> > > dataset;
    std::vector< std::vector<short> > line;
    std::vector<short> pulse;
    for (size_t i = 0; i < data.size()-1; i++) {
        if (i < data.size() - 2) {
            if (data[i] == 0xface && data[i + 1] == 0xfeed) {

            }
        }
        //if ( (ua == 0xface) && (ub == 0xfeed) ) {
        //    std::cout << i << "\n";
        //}
        
        bool laserTrigger = ((data[i] & 0x8000) > 0);
        unsigned short mask = (~0x8000);
        data[i] = (data[i] & mask);
        if (data[i] > 16383) {
            data[i] = data[i] - 32768;
        }
        pulse.push_back(data[i]);
        if (laserTrigger) {
            if (prevTrigger<0 || i - minDist>prevTrigger) {
                prevTrigger = i;
                //pulses.push_back(pulse);
                //pulse.clear();
            }
        }
    }
    FileUtilOut out("K:/tmp.txt");
    out.out << nLines << "\n";
    //for (int i = 0; i < pulses.size(); i++) {
    //    out.out << pulses[i].size() << "\n";
    //    for (size_t j = 0; j < pulses[i].size(); j++) {
    //        out.out << pulses[i][j] << "\n";
    //    }
    //    out.out << "\n";
    //}
    out.close();
}

void parseCBCTScan(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string outDir = conf.getString("outDir");
    createDir(outDir);
    int nSlices = 920;
    const int fgThresh = 143;
    int startSlice = 290;
    int endSlice = 622;
    for (int slice = 0; slice < nSlices; slice++) {
        std::string filename = sequenceFilename(slice, dataDir.c_str(), NULL, 4, ".cb");
        FileUtilIn in;
        in.open(filename, std::ifstream::binary);
        if (!in.good()) {
            return;
        }
        size_t headerSize = 0x518;
        size_t dataSize = 0x100000;  //count
        size_t width = 1024;
        size_t height = dataSize / width;
        std::vector<uint16_t> data(dataSize, 0);
        std::vector<uint8_t> header(headerSize, 0);
        in.in.read((char*)header.data(), headerSize);
        in.in.read((char*)data.data(), data.size() * sizeof(uint16_t));
        cv::Mat img(height, width, CV_8UC1);
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                img.at<uint8_t>(row, col) = data[row*width + col] / 256;
            }
        }
        std::string outName = sequenceFilename(slice, outDir.c_str(), NULL, 4, ".png");
        cv::imwrite(outName, img);
    }
}

void bin2Png(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string scanDir = dataDir + "/scan1/";
    std::string scanDirOut = dataDir + "/scan/";
    createDir(scanDirOut);
    int startIdx = 12;
    int endIdx = 397;
    float scanZOffset = 250;
    for (int i = startIdx; i <= endIdx; i++) {
        std::string depthFile = scanDir + std::to_string(i) + ".bin";
        std::vector<float> scanIn;
        size_t scanW, scanH;
        loadFloatImage(depthFile, scanIn, scanW, scanH);
        cv::Mat depth = cv::Mat::zeros(scanH, scanW, CV_8UC1);
        for (int row = 0; row < scanH; row++) {
            for (int col = 0; col < scanW; col++) {
                depth.at<uint8_t>(row, col) = (uint8_t)(scanIn[row*scanW + col] - scanZOffset);
            }
        }
        //cv::medianBlur(depth, depth, 3);
        std::string outFile = scanDirOut + std::to_string(i) + ".png";
        cv::flip(depth, depth, 0);
        cv::imwrite(outFile, depth);
    }
}

void computeScanLine(const ConfigFile &conf) {
    std::string laserFile = conf.getString("laserFile");
    std::string triggerFile = conf.getString("triggerFile");

    std::vector<int> laser, trigger;
    loadArr1d(laserFile, laser);
    loadArr1d(triggerFile, trigger);
    //find trigger indices.
    int maxTriggerVal = 0;
    int triggerThresh = 0;
    const int waveSize = 98;
    size_t arrSize = std::min(trigger.size(), laser.size());
    trigger.resize(arrSize);
    laser.resize(arrSize);
    for (size_t i = 0; i < trigger.size(); i++) {
        maxTriggerVal = std::max(maxTriggerVal, trigger[i]);
    }
    triggerThresh = 0.8*maxTriggerVal;
    const int minPulseLen = 30;
    int waveShift = 25;
    //abaco data all multiples of 16 
    int padWaveSize = 128;
    std::vector<std::vector<float> > waves;
    for (size_t i = 0; i < trigger.size() - waveSize - waveShift; i++) {
        if (trigger[i] > triggerThresh) {
            std::vector<float> wave(padWaveSize, 0.0f);
            for (int j = 0; j < waveSize; j++) {
                wave[j] = laser[i + waveShift + j]/16;
            }
            i += minPulseLen;
            waves.push_back(wave);
        }
    }
    cv::Mat debugImg (cv::Size(waves.size(), padWaveSize), CV_8UC1);
    //scan lines  without post fft brightness greater than this 
    //is treated as background.
    float bgThresh = 3000;
    //fft each separate waveform.
    for (size_t i = 0; i < waves.size(); i++) {
        //w is a row vector
        cv::Mat w(cv::Size((int)waves[i].size(), 1), CV_32FC1, waves[i].data());
        cv::Mat fft;
        //fft comes out as a row vector
        cv::dft(w, fft);
        float maxMag = 0;
        for (int k = 0; k < fft.cols; k++) {
            maxMag = std::max(maxMag, std::abs(fft.at<float>(0, k)));
        }
        if (maxMag < bgThresh) {
            maxMag = bgThresh;
        }
        for (int k = 0; k < fft.cols; k++) {
            float mag = std::abs(fft.at<float>(0, k)) / maxMag;
            debugImg.at<uint8_t>(k, i) = (uint8_t)(mag * 255);
        }
    }
    cv::imwrite("D:/data/ML/debug.png", debugImg);

    //save debug file.
    FileUtilOut debugFile("debug.txt");
    debugFile.out << waves.size() << " " << waveSize << "\n";
    for (size_t i = 0; i < waves.size(); i++) {
        for (size_t j = 0; j < waves[i].size(); j++) {
            debugFile.out << waves[i][j] << " ";
        }
        debugFile.out << "\n";
    }
    debugFile.close();
}

void computeFFTVol(const ConfigFile & conf) {
    std::string laserFile = conf.getString("laserFile");
    std::string triggerFile = conf.getString("triggerFile");
    FileUtilIn laserIn(laserFile);
    FileUtilIn triggerIn(triggerFile);
    int nLines = 0, width = 0;
    //laser and trigger
    laserIn.in >> width >> nLines;
    triggerIn.in >> width >> nLines;
    std::vector<int> line(width);
    std::vector<int> trigger(width);
    std::vector<uint8_t > vol;
    //find trigger indices.
    int maxTriggerVal = 0;
    int triggerThresh = 0;
    const int waveSize = 98;
    const int minPulseLen = 30;
    int waveShift = 25;

    int padWaveSize = 128;

    int h = nLines;
    float bgThresh = 3000;
    int volSize[3];
    volSize[0] = 800;
    volSize[1] = nLines;
    volSize[2] = padWaveSize;
    size_t nVox = volSize[0] * (size_t)volSize[1] * volSize[2];
    vol.resize(nVox, 0);
    for (int l = 0; l < nLines; l++) {
        for (int i = 0; i < width; i++) {
            laserIn.in >> line[i];
            triggerIn.in >> trigger[i];
        }
        size_t arrSize = std::min(trigger.size(), line.size());
        trigger.resize(arrSize);
        line.resize(arrSize);

        if (maxTriggerVal == 0) {
            for (size_t i = 0; i < trigger.size(); i++) {
                maxTriggerVal = std::max(maxTriggerVal, trigger[i]);
            }
            triggerThresh = 0.8*maxTriggerVal;
        }

        int waveCount = 0;
        for (size_t i = 0; i < trigger.size() - waveSize - waveShift; i++) {
            if (trigger[i] > triggerThresh) {
                std::vector<float> wave(padWaveSize, 0.0f);
                for (int j = 0; j < waveSize; j++) {
                    //abaco data all multiples of 16 
                    wave[j] = line[i + waveShift + j] / 16;
                }
                i += minPulseLen;
            
                //fft each separate waveform.
                 //w is a row vector
                cv::Mat laser(cv::Size((int)wave.size(), 1), CV_32FC1, wave.data());
                cv::Mat fft;
                //fft comes out as a row vector
                cv::dft(laser, fft);

                float maxMag = 0;
                for (int k = 0; k < fft.cols; k++) {
                    maxMag = std::max(maxMag, std::abs(fft.at<float>(0, k)));
                }
                if (maxMag < bgThresh) {
                    maxMag = bgThresh;
                }
                for (int k = 0; k < fft.cols; k++) {
                    float mag = std::abs(fft.at<float>(0, k)) / maxMag;
                    int linIdx = gridToLinearIdx(waveCount/2, l, k, volSize);
                    vol[linIdx] += (uint8_t)(mag * 125);
                }

                waveCount++;
                if (waveCount/2 >= volSize[0]) {
                    break;
                }
            }
        }

    }

    std::ofstream out("D:/data/ML/coin/vol.vol", std::ofstream::binary);
    
    out.write((const char *)&volSize[0], sizeof(int));
    out.write((const char *)&volSize[1], sizeof(int));
    out.write((const char *)&volSize[2], sizeof(int));
    out.write((const char *)vol.data(), vol.size());
    out.close();
}

void saveBinaryVol(const ConfigFile & conf)
{
    std::string laserFile = conf.getString("laserFile");
    std::string triggerFile = conf.getString("triggerFile");
    FileUtilIn laserIn(laserFile);
    FileUtilIn triggerIn(triggerFile);
    int nLines = 0, width = 0;
    //laser and trigger
    laserIn.in >> width >> nLines;
    triggerIn.in >> width >> nLines;
    std::vector<int> line(width);
    std::vector<int> trigger(width);
    std::vector<short > vol;
    //find trigger indices.
    int maxTriggerVal = 0;
    int triggerThresh = 0;
    const int waveSize = 98;
    const int minPulseLen = 30;
    int waveShift = 25;

    int h = nLines;
    float bgThresh = 3000;
    int volSize[3];
    volSize[0] = 1600;
    volSize[1] = nLines;
    volSize[2] = waveSize;
    size_t nVox = volSize[0] * (size_t)volSize[1] * volSize[2];
    vol.resize(nVox);
    for (int l = 0; l < nLines; l++) {
        for (int i = 0; i < width; i++) {
            laserIn.in >> line[i];
            triggerIn.in >> trigger[i];
        }
        size_t arrSize = std::min(trigger.size(), line.size());
        trigger.resize(arrSize);
        line.resize(arrSize);

        if (maxTriggerVal == 0) {
            for (size_t i = 0; i < trigger.size(); i++) {
                maxTriggerVal = std::max(maxTriggerVal, trigger[i]);
            }
            triggerThresh = 0.8*maxTriggerVal;
        }

        int waveCount = 0;
        for (size_t i = 0; i < trigger.size() - waveSize - waveShift; i++) {
            if (trigger[i] > triggerThresh) {
                std::vector<short> wave(waveSize, 0.0f);
                for (int j = 0; j < waveSize; j++) {
//abaco data all multiples of 16 
wave[j] = line[i + waveShift + j] / 16;
                }
                i += minPulseLen;
                for (int k = 0; k < wave.size(); k++) {
                    int linIdx = gridToLinearIdx(waveCount, l, k, volSize);
                    vol[linIdx] = (wave[k]);
                }

                waveCount++;
                if (waveCount >= volSize[0]) {
                    break;
                }
            }
        }

    }

    std::ofstream out("D:/data/ML/coin/raw.vol", std::ofstream::binary);
    out << volSize[0] << "\n";
    out << volSize[1] << "\n";
    out << volSize[2] << "\n";
    out.write((const char *)vol.data(), sizeof(short) * vol.size());
    out.close();
}

void extractWaveform(const ConfigFile & conf) {
    std::string filename = conf.getString("dataFile");
    std::ifstream in(filename);
    std::string outFile = conf.getString("outFile");
    std::ofstream out(outFile);
    int nLines = 0, width = 0;
    in >> width>>nLines;
    std::vector<int> wave(width);
    int lineNum = 300;
    for (int l = 0; l < lineNum; l++) {
        for (int i = 0; i < width; i++) {
            in >> wave[i];
        }
    }
    out << width << "\n";
    for (int i = 0; i < width; i++) {
        out << wave[i] << "\n";
    }
    in.close();
}


void stitchDepthPng(const ConfigFile & conf) {
    //z offset in .bin file so that depth is in the range of
    //0-255 to be stored as an 8-bit image.
    const int IMG_OFFSET = 150;
    std::string dataDir = conf.getString("dataDir");
    int imgIdx = 0;
    std::string imgFile = dataDir + std::to_string(imgIdx) + ".bin";
    std::string outDir = conf.getString("outDir");
    createDir(outDir);
    ImageF32 img;
    size_t width, height;
    std::vector<int> fullSize;
    conf.getIntVector("fullSize", fullSize);
    //const int NUM_TILES = 4;
    //int tilePos[NUM_TILES][2] = { {0,539},{0,0},{545,0},{545,539} };
    //const int NUM_TILES = 2;
    //int tilePos[NUM_TILES][2] = { {0,320},{545,320}};
    const int NUM_TILES = 6;
    std::vector<std::array<int, 2> > tilePos(NUM_TILES);
    int nRows = 2;
    int nCols = 3;
    for (int row = 0; row < nRows; row++) {
        for (int col = 0; col < nCols; col++) {
            bool evenCol = (col % 2 == 0);
            int tileIdx = col * nRows + row;
            if (evenCol) {
                tilePos[tileIdx][1] = (nRows - row - 1) * 500;
            }
            else {
                tilePos[tileIdx][1] = row * 500;
            }
            tilePos[tileIdx][0] = col * 500;
        }
    }
    const float minPixelWeight = 1e-5;
    int endIdx = 4;
    int startIdx = 0;
    conf.getInt("endIdx", endIdx);
    conf.getInt("startIdx", startIdx);
    for (int imgIdx = startIdx; imgIdx <= endIdx; imgIdx+= NUM_TILES) {
        cv::Mat fullImg = cv::Mat::zeros(cv::Size(fullSize[0], fullSize[1]), CV_32FC1);
        std::vector<float> weight(fullSize[0] * fullSize[1], 0);
        for (int tileIdx = 0; tileIdx < NUM_TILES; tileIdx++) {
            imgFile = dataDir +"/"+ std::to_string(imgIdx + tileIdx) + ".bin";
            loadFloatImage(imgFile, img.data, width, height);
            img.size[0] = (uint32_t)width;
            img.size[1] = (uint32_t)height;
            ImageF32 outImg;
            transformImage(img, outImg, 0.04, 0);

            cv::Mat cvImg(cv::Size(width, height), CV_32FC1, img.data.data());
            cvImg = cvImg - IMG_OFFSET;// -9 * tileIdx / nRows;
            std::string outFileName = outDir + "/" + std::to_string(imgIdx) + ".png";
            cv::imwrite(outFileName, cvImg);
            const auto & s = img.size;
            for (int col = 0; col<s[0]; col++) {
                for (int row = 0; row<s[1]; row++) {
                    int globalRow = row + tilePos[tileIdx][1];
                    int globalCol = col + tilePos[tileIdx][0];
                    if (globalRow<0 || globalRow >= fullSize[1] || globalCol<0 || globalCol >= fullSize[0]) {
                        continue;
                    }
                    float depth = img(col, row);
                    //blend images using a weight that decreases with distance from tile center.
                    //dx, dy, normalized weight for closeness to image center.
                    //The value is 1 if a pixel is at image center and linearly decrease to
                    //0 as the coordinate moves to image edge.
                    float dx = 1.0f - std::abs(2.0f * col - s[0] + 1) / (s[0] - 1);
                    float dy = 1.0f - std::abs(2.0f * row - s[1] + 1) / (s[1] - 1);
                    float w = std::max(0.0f, dx*dy) + minPixelWeight;
                    fullImg.at<float>(globalRow, globalCol) += w * depth;
                    weight[globalCol + globalRow * fullSize[0]] += w;
                }
            }
        }
        //normalize pixels by sums of weights.
        for (int col = 0; col<fullImg.cols; col++) {
            for (int row = 0; row < fullImg.rows; row++) {
                float w = weight[col + row * fullSize[0]];
                if (w > minPixelWeight / 2) {
                    fullImg.at<float> (row, col) /= w;
                }
            }
        }
        std::string fullFile = outDir + "/full" + std::to_string(imgIdx / NUM_TILES) + ".png";
        cv::imwrite(fullFile, fullImg);
    }
}


void addGradient(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string outDir = dataDir + "\\out\\";
    createDir(outDir);

    int startIdx = conf.getInt("startIdx");
    int endIdx = conf.getInt("endIdx");
    std::srand(100);
    for (int i = startIdx; i <= endIdx; i++) {
        std::string fileName = dataDir + "\\bits" + std::to_string(i) + ".png";

        cv::Mat printData = cv::imread(fileName);
        for (int row = 0; row < printData.rows; row++) {
            for (int col = 0; col < printData.cols; col++) {
                cv::Vec3b color = printData.at<cv::Vec3b>(row, col);
                int mat = color[2];
                
                if (mat == 6 ) {
                    float random_variable = std::rand() / (float)RAND_MAX;
                    float thresh = (col-1576) / 1800.0f;
                    thresh = std::max(0.0f, thresh);
                    thresh = std::min(1.0f, thresh);
                    thresh = 0.15 / (thresh + 0.01)-0.15;
                    if (col > 3500) {
                        thresh = 0;
                    }
                    if (random_variable < thresh) {
                        color[2] = 2;
                        color[1] = 70;
                    }
                    else {
                        color[1] = 140;
                        color[2] = 3;
                    }
                    printData.at<cv::Vec3b>(row, col) = color;
                }
                else if (mat == 7) {
                    float thresh = (row - 81) / 300.0f;
                    thresh = std::max(0.0f, thresh);
                    thresh = std::min(1.0f, thresh);
                    thresh = 0.1 / (1 - thresh + 0.01);
                    float random_variable = std::rand() / (float)RAND_MAX;
                    if (random_variable < thresh) {
                        color[2] = 2;
                        color[1] = 70;
                    }
                    else {
                        color[1] = 140;
                        color[2] = 3;
                    }
                    printData.at<cv::Vec3b>(row, col) = color;
                }
            }
        }

        //scale x to 3x because triple jetting
        //cv::Mat resized, resized0;
        //shrink by 1 unit according to machine learning data
        //cv::resize(printData, resized0, cv::Size(0, 0), 142 / 143.0f, 140 / 142.0f, cv::INTER_NEAREST);
        //cv::resize(resized0, resized, cv::Size(0, 0), 3, 1, cv::INTER_NEAREST);
        //cv::resize(printData, resized, cv::Size(0, 0), 3, 1, cv::INTER_NEAREST);
        std::string outFile = outDir + std::to_string(i) + ".png";
        cv::flip(printData, printData, 0);
        cv::imwrite(outFile, printData);
    }
    std::ofstream fileNameOut(outDir + "/filenames.txt");
    for (int i = startIdx; i <= endIdx; i++) {
        fileNameOut << std::to_string(i) << ".png ";
    }
    fileNameOut.close();
}

void bin2txt(std::string binFile) {
    std::ifstream in(binFile, std::ifstream::binary);
    if (!in.good()) {
        return;
    }
    size_t size[2];
    in.read((char*)size, 2 * sizeof(size_t));

    std::ofstream out("out.txt");
    std::vector<float> data(size[0] * size[1]);
    in.read((char*)data.data(), sizeof(float)*size[0] * size[1]);
    out << size[0] << " " << size[1] << "\n";
    for (size_t i = 0; i < data.size(); i++) {
        out << ((int)data[i]) << "\n";
    }
    out.close();
}

///sum pixel values along z axis to produce a 2D image from a 3D volume.
void sumZ(const ConfigFile & conf) {
    std::string volFile = conf.getString("vol");
    Array3D<uint8_t> vol;
    std::ifstream in(volFile, std::ifstream::binary);
    if (!in.good()) {
        std::cout << "can not open " << volFile << "\n";
        return;
    }
    int size[3];
    in.read((char*)(&size), sizeof(size));
    vol.Allocate(size[0], size[1], size[2]);
    in.read((char*)vol.DataPtr(), vol.GetData().size());
    in.close();

    if (vol.GetData().size() == 0) {
        return;
    }
    //0   1
    //2   3
    int cutp[4][3] = { {0,0,353}, {561,0,343}, {0,561,335}, {561,561,323} };
    cv::Mat sumImg = cv::Mat::zeros(vol.GetSize()[1], vol.GetSize()[0], CV_32FC1);
    for (int x = 0; x < vol.GetSize()[0]; x++) {
        for (int y = 0; y < vol.GetSize()[1]; y++) {
            float a = (x / (float)vol.GetSize()[0]);
            float b = (y / (float)vol.GetSize()[1]);
            float zCut = cutp[0][2] * (1 - a)*(1 - b) + cutp[1][2] * a*(1 - b) +
                cutp[2][2] * (1 - a)* b + cutp[3][2] * a*b;
            float sum = 0;
            int z0 = std::max(0, (int)zCut);
            int z1 = std::min((int)vol.GetSize()[2], z0 + 50);
            for (int z = z0; z < z1; z++) {
                sum += vol(x, y, z);
            }
            sum /= z1 - z0;
            sumImg.at<float>(y, x) = sum;
        }
    }
    cv::imwrite("sumImg.png", sumImg);
}

void savePointCloud(const ConfigFile & conf)
{
    std::string volFile = conf.getString("vol");
    std::string volFileOut = conf.getString("volOut");

    Array3D<uint8_t> vol;
    std::ifstream in(volFile, std::ifstream::binary);
    if (!in.good()) {
        std::cout << "can not open " << volFile << "\n";
        return;
    }
    int size[3];
    in.read((char*)(&size), sizeof(size));
    vol.Allocate(size[0], size[1], size[2]);
    in.read((char*)vol.DataPtr(), vol.GetData().size());
    in.close();

    if (vol.GetData().size() == 0) {
        return;
    }

    int thresh = 5;
    std::ofstream out("pointCloud.ply");
    size_t cnt = 0;
    float dx[3] = { 0.05f, 0.05f, 0.05f };
    for (int x = 0; x < size[0]; x++) {
        for (int y = 0; y < size[1]; y++) {
            for (int z = 0; z < size[2]; z++) {
                if (vol(x, y, z) > thresh) {
                    cnt++;
                }
            }
        }
    }
    out << "ply\n"
        << "format ascii 1.0\n"
        << "element vertex " << cnt << "\n"
        << "property float x\n"
        << "property float y\n"
        << "property float z\n"
        << "property uchar red\n"
        << "property uchar green\n"
        << "property uchar blue\n"
        << "property uchar alpha\n"
        << "element face 0\n"
        << "property list uchar int vertex_indices\n"
        << "end_header\n";
    for (int x = 0; x < size[0]; x++) {
        for (int y = 0; y < size[1]; y++) {
            for (int z = 0; z < size[2]; z++) {
                uint8_t val = vol(x, y, z);
                if (val > thresh) {
                    
                    out << (x*dx[0]) << " " << (y*dx[1]) << " " << (z*dx[2]);
                    int outVal = (int)val ;
                    outVal = std::min(255, 5 * outVal);
                    out <<" "<< outVal << " " << outVal << " " << outVal << " " << outVal << "\n";
                }
            }
        }
    }
    out << "\n";
    out.close();
}

//jet support material at 2x y resolution. 
//Also erode support at each layer.
void denseSupport(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string outDir = dataDir + "\\out\\";
    createDir(outDir);

    int startIdx = conf.getInt("startIdx");
    int endIdx = conf.getInt("endIdx");
    int erode_size[2] = { 2, 1 };
    
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT,
        cv::Size(2 * erode_size[0] + 1, 2 * erode_size[1] + 1),
        cv::Point(erode_size[0], erode_size[1]));
    std::cout << element << "\n";
    for (int i = startIdx; i <= endIdx; i++) {
        std::string fileName = dataDir + "\\bits" + std::to_string(i) + ".png";
        cv::Mat printData = cv::imread(fileName);
        //erode support material
        cv::Mat supImg = cv::Mat::zeros(printData.size(), CV_8UC1);
        for (int row = 0; row < printData.rows; row++) {
            for (int col = 0; col < printData.cols; col++) {
                cv::Vec3b color = printData.at<cv::Vec3b>(row, col);
                int mat = color[2];
                if (mat == 1) {
                    supImg.at<uint8_t>(row, col) = 255;
                }
            }
        } 
        cv::erode(supImg, supImg, element);

        //delete old support and copy new support into print data
        for (int row = 0; row < printData.rows; row++) {
            for (int col = 0; col < printData.cols; col++) {
                cv::Vec3b color = printData.at<cv::Vec3b>(row, col);
                int mat = color[2];
                int newMat = supImg.at<uint8_t>(row, col) > 0;
                if (mat == 1 && newMat == 0) {
                    printData.at<cv::Vec3b>(row, col) = cv::Vec3b(0, 0, 0);
                }
            }
        }

        //scale image in y and use sparse pattern for z
        cv::Mat out;
        cv::resize(printData, out, cv::Size(printData.cols * 3, printData.rows), 0, 0, cv::INTER_LINEAR);
        //cv::resize(printData, out, cv::Size(printData.cols*3, printData.rows * 2), 0, 0, CV_INTER_NN);
        //for (int row = 1; row < out.rows; row+=2) {
        //    for (int col = 0; col < out.cols; col++) {
        //        cv::Vec3b color = out.at<cv::Vec3b>(row, col);
        //        int mat = color[2];
        //        if (mat != 1 && mat != 0) {
        //            out.at<cv::Vec3b>(row, col) = cv::Vec3b(0, 0, 0);
        //        }
        //    }
        //}
        std::string outFileName = outDir + std::to_string(i) + ".png";
        cv::imwrite(outFileName, out);
    }
    
    std::ofstream fileNameOut(outDir + "/filenames.txt");
    for (int i = startIdx; i <= endIdx; i++) {
        fileNameOut << std::to_string(i) << ".png ";
    }
    fileNameOut.close();
}


void filterPrintData(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string outDir = dataDir + "\\out\\";
    createDir(outDir);

    int startIdx = conf.getInt("startIdx");
    int endIdx = conf.getInt("endIdx");
    int erode_size = 1;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT,
        cv::Size(2 * 3 * erode_size + 1, 2 * erode_size + 1),
        cv::Point(erode_size, erode_size));

    for (int i = startIdx; i <= endIdx; i++) {
        std::string fileName = dataDir + "\\bits" + std::to_string(i) + ".png";

        cv::Mat printData = cv::imread(fileName);
        //split into two image files by material
        cv::Mat imgs[2];
        imgs[0] = cv::Mat::zeros(printData.size(), CV_8UC1);
        imgs[1] = cv::Mat::zeros(printData.size(), CV_8UC1);
        for (int row = 0; row < printData.rows; row++) {
            for (int col = 0; col < printData.cols; col++) {
                cv::Vec3b color = printData.at<cv::Vec3b>(row, col);
                int mat = color[2];
                if (mat == 1) {
                    imgs[0].at<uint8_t>(row, col) = 255;
                }
                else if(mat == 3) {
                    imgs[1].at<uint8_t>(row, col) = 255;
                }
            }
        }
        //apply filter
        cv::erode(imgs[1], imgs[1], element);
        //dilate support material
        cv::dilate(imgs[0], imgs[0], element);
        //assemble materials
        for (int row = 0; row < printData.rows; row++) {
            for (int col = 0; col < printData.cols; col++) {
                uint8_t mat = imgs[0].at<uint8_t>(row, col);
                cv::Vec3b color(50, 50, 1);
                if (mat > 0) {
                    printData.at<cv::Vec3b>(row, col) = color;
                }
                else {
                    printData.at<cv::Vec3b>(row, col) = 0;
                }
                color = cv::Vec3b(150, 150, 3);
                mat = imgs[1].at<uint8_t>(row, col);
                if (mat > 0) {
                    printData.at<cv::Vec3b>(row, col) = color;
                }
            }
        }
        
        //scale x to 3x because triple jetting
        //cv::Mat resized, resized0;
        //shrink by 1 unit according to machine learning data
        //cv::resize(printData, resized0, cv::Size(0,0), 142 / 143.0f, 140 / 142.0f, cv::INTER_NEAREST);
        //cv::resize(resized0, resized, cv::Size(0, 0), 3, 1, cv::INTER_NEAREST);
        std::string outFile = outDir + std::to_string(i) + ".png";
        cv::flip(printData, printData, 0);
        cv::imwrite(outFile, printData);
    }
    std::ofstream fileNameOut(outDir + "/filenames.txt");
    for (int i = startIdx; i <= endIdx; i++) {
        fileNameOut << std::to_string(i) << ".png ";
    }
    fileNameOut.close();
}

