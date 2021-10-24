#include "Array2D.h"
#include "Array3D.h"
#include "ArrayUtil.hpp"
#include "FileUtil.hpp"
#include "ConfigFile.hpp"
#include "StackColorVol.hpp"
#include "DepthHistory.h"
#include "Image.h"
#include "OctAnalysis.h"
#include "Recon.hpp"
#include "TrigMesh.hpp"
#include "meshutil.h"
#include "AlignImages.hpp"
#include "VolumeIO.hpp"
#include "cvMatUtil.hpp"
#include "Avenir.h"
#include "lodepng.h"

#include <opencv2\opencv.hpp>
#include <string>
#include <filesystem>

int SavePng(const std::string& filename, Array2D8u& slice);
std::string type2str(int type);

void saveMaskObj(std::string filename, cv::Mat & mask, float * dx) {
    TrigMesh mout;
    for (int row = 0; row < mask.rows; row++) {
        for (int col = 0; col < mask.cols; col++) {
            if (mask.at<uint8_t>(row, col) == 0) {
                continue;
            }
            TrigMesh cube;
            TrigMesh::Vector3s mn, mx;
            mn[0] = col * dx[1];
            mn[1] = row * dx[0];
            mn[2] = 0;
            mx[0] = mn[0] + dx[1];
            mx[1] = mn[1] + dx[0];
            mx[2] = dx[2];
            makeCube(cube, mn, mx);
            mout.append(cube);
        }
    }
    mout.save_obj(filename.c_str());
}

void depthToObjMesh(const ConfigFile & conf ) {
    ImageF32 img;
    std::string depthFile;
    if (conf.hasOpt("depth")) {
        depthFile = conf.getString("depth");
    }
    size_t width, height;
    loadFloatImage(depthFile, img.data, width, height);
    float dx[3] = { 0.05f, 0.05f, 0.0042f };
    //saveHeightObj("depth.obj", img.data, (int)width, (int)height, dx);
    std::string printDataDir = conf.getString("printDataDir");
    int supIdx = 0;
    conf.getInt("printDataIdx", supIdx);
    std::string supFile = printDataDir + std::to_string(supIdx) + ".png";
    cv::Mat supMask = cv::imread(supFile, cv::IMREAD_UNCHANGED);
    std::string matFile = printDataDir + std::to_string(supIdx+1) + ".png";
    cv::Mat matMask = cv::imread(matFile, cv::IMREAD_UNCHANGED);

    dx[2] = 0.03f;
    std::string maskMeshFile = "supMask.obj";
    saveMaskObj(maskMeshFile, supMask, dx);
    maskMeshFile = "matMask.obj";
    saveMaskObj(maskMeshFile, matMask, dx);

}

float estimateDepth(const std::deque<std::vector<float> > & history, int row, int col, int w) {
    int linIdx = row * w + col;
    float estimateSlope = 0;
    if (history.size() == 1) {
        return history[0][linIdx];
    }
    if (history.size() < 1) {
        return 0;
    }
    //least squares fit
    double sumX = 0, sumY = 0;
    double meanY = 0;
    for (size_t i = 0; i < history.size(); i++) {
        meanY += history[i][linIdx];
    }
    meanY /= (double)history.size();
    for (size_t i = 0; i < history.size(); i++) {
        float x = (i - history.size() / 2.0f + 0.5f);
        sumX += x * x;
        sumY += x * (history[i][linIdx] - meanY);
    }
    double beta = sumY / sumX;
    double val = beta * (-(double)history.size() / 2.0f + 0.5f) + meanY;

    return (float)val;
}

void loadFloatArr(std::string filename, std::vector<float> & a)
{
    std::ifstream in(filename, std::ifstream::binary);
    if (!in.good()) {
        return;
    }
    in.read((char*)(a.data()),a.size() * sizeof(float));
    in.close();
}

void makeTimeLapse(const ConfigFile & conf)
{
    std::string imgDir = conf.getString("dataDir");
    std::string outDir = imgDir + "/out/";
    createDir(outDir);
    int startIdx = 15;
    int endIdx = 402;
    int w = 541;
    int h = 541;
    //deeper than this value is treated as the background.
    float baseDepth = 77;
    //depth value changes about this much every step.
    float depthStep = 1.5;
    //falling deeper than that is ignored.
    float maxDepth = 150;
    std::deque<std::vector<float> >history;
    const unsigned int HISTORY_SIZE = 10;
    float lb = 5;
    float ub = 5;
    int objCnt = 0;

    float dx[3] = { 0.05f, 0.05f, 0.01f };

    int maskIdx = 33;
    std::vector<float> img(w*h);
    std::string filename = imgDir + "/" + std::to_string(maskIdx) + ".bin";
    loadFloatArr(filename, img);
    std::vector<int> mask(w*h, 1);
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            if (img[row*w + col] > baseDepth) {
                mask[row*w + col] = 0;
            }
        }
    }
    for (int i = startIdx; i <= endIdx; i++) {
        filename = imgDir + "/" + std::to_string(i) + ".bin";
        loadFloatArr(filename, img);
        
        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                int linIdx = row * w + col;
                if (mask[linIdx] == 0) {
                    img[linIdx] = 55 + (i - startIdx) * depthStep;
                }
                else if(history.size()>3){
                    float hisVal = estimateDepth(history, row, col, w);
                    //if (img[linIdx] > maxDepth) {
                    //    if (std::abs(img[linIdx] - history[0][linIdx]) > 2) {
                    //    //    track[linIdx] = 0;
                    //    }
                    //}
                    img[linIdx] = std::max(img[linIdx] , hisVal - lb);
                    img[linIdx] = std::min(img[linIdx] , hisVal + ub);
                }
            }
        }
        history.push_front(img);
        if (history.size() > HISTORY_SIZE) {
            history.pop_back();
        }
        cv::Mat outImg(cv::Size(w, h), CV_32FC1, (unsigned char *)img.data());
        cv::flip(outImg, outImg, 0);
        std::string outname = outDir + std::to_string(i) + ".png";
        cv::imwrite(outname, outImg);

        //if ((i - startIdx) % 1 == 0) {
            for (size_t l = 0; l < img.size(); l++) {
              img[l] = -(50-img[l] + depthStep * (i-startIdx));
            }
            std::stringstream objName;
            objName << outDir << std::setfill('0') << std::setw(4) << std::to_string(objCnt) << ".obj";
            bool saveTrig = true;
            if (objCnt > 0) {
                saveTrig = false;
            }
            saveHeightObj(objName.str(), img, w, h, dx, saveTrig);
            objCnt++;
        //}
        
        cv::flip(outImg, outImg, 0);
    }
}

void saveObj(std::string outfile, const Array3D<uint8_t> & vol,
    float zRes, int mat)
{
    TrigMesh box;
    TrigMesh mout;
    //convert zres to mm.
    cv::Vec3f dx( 0.05f, 0.05f, zRes/1000);
    int nx = vol.GetSize()[0];
    int ny = vol.GetSize()[1];
    int nz = vol.GetSize()[2];
    const int N_NBR = 6;
    //y is flipped because in voxel ordering y is flipped.
    int nbrIdx[N_NBR][3] = { { 0, 0, -1 },{ 0, 0, 1 },{ 0, 1, 0 },{ 0, -1, 0 },
    { -1, 0, 0 },{ 1, 0, 0 } };
    for (int ii = 0; ii < nx; ii++) {
        for (int jj = 0; jj < ny; jj++) {
            for (int kk = 0; kk < nz; kk++) {
                if (vol(ii, jj, kk) != mat) {
                    continue;
                }

                int nbrCount = 0;
                std::vector<bool> hasNbr(N_NBR, 0);
                for (int n = 0; n < N_NBR; n++) {
                    int ni = ii + nbrIdx[n][0];
                    int nj = jj + nbrIdx[n][1];
                    int nk = kk + nbrIdx[n][2];
                    if (!inbound(ni, nj, nk, nx, ny, nz)) {
                        continue;
                    }
                    if (vol(ni, nj, nk)==mat) {
                        nbrCount++;
                        hasNbr[n] = true;
                    }
                }
                if (nbrCount == N_NBR) {
                    continue;
                }
                cv::Vec3f coord(dx[0] * (0.5f + ii), dx[1]*(0.5f + ny-jj-1), dx[2]*(0.5f + kk));
                cv::Vec3f box0 = (coord - 0.5f * dx);
                cv::Vec3f box1 = (coord + 0.5f * dx);
                TrigMesh::Vector3s b0 = { box0[0], box0[1], box0[2] };
                TrigMesh::Vector3s b1 = { box1[0], box1[1], box1[2] };
                makeCube(box, b0, b1);
                //remove internal faces
                std::vector<TrigMesh::Vector3i> newTrigs;
                for (int n = 0; n < N_NBR; n++) {
                    if (!hasNbr[n]) {
                        newTrigs.push_back(box.t[n * 2]);
                        newTrigs.push_back(box.t[n * 2 + 1]);
                    }
                }
                box.t = newTrigs;
                box.rmUnusedVert();
                mout.append(box);
            }
        }
    }
    mout.save_obj(outfile.c_str());
}

void testSaveVolume(const ConfigFile & conf) {
    const float voxZRes = 1;
    Array3D<uint8_t> vol;
    int w = 100;
    int h = 100;
    int nz = 100;
    vol.Allocate(w, h, nz,0);
    std::string dataDir = conf.getString("dataDir");
    std::string outDir = dataDir + "/meshes/";
    std::string outFile0 = outDir + std::to_string(0) + ".obj";

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            for (int z = 0; z < nz; z++) {
                float dist = x * x + y * y + z * z;
                if (dist < 1000.0f) {
                    vol(x, y, z) = 1;
                }
            }
        }
    }
    float voxRes [3]= { 0.1, 0.1, 0.1 };
    saveObjRect(outFile0, vol, voxRes, 1);
}

void loadPreviousVol(const ConfigFile & conf, Array3D<uint8_t> & vol) {
    if (!conf.hasOpt("prevVol")) {
        return;
    }
    std::string dataDir = conf.getString("dataDir");
    std::string prevFile = dataDir + conf.getString("prevVol");
    Array3D<uint8_t> pvol;
    int startIdx = conf.getInt("startIdx");
    int endIdx = conf.getInt("endIdx");
    int nLayers = (endIdx - startIdx + 1);

}

void scalePrintData(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string printDataDir = dataDir + "/slices/";
    std::string printDataOutDir = dataDir + "/slicesScaled/";
    createDir(printDataOutDir);
    int startIdx = conf.getInt("startIdx");
    int endIdx = conf.getInt("endIdx");

    cv::Mat printData;
    std::vector<float> scanRes = conf.getFloatVector("scanRes");
    std::vector<float> printRes = conf.getFloatVector("printRes");

    for (int idx = startIdx; idx <= endIdx; idx++) {
        std::string printDataFile = printDataDir +"bits"+ std::to_string(idx) + ".png";
        cv::Mat printIn = cv::imread(printDataFile);
        cv::Mat resized;
        cv::resize(printIn, resized, cv::Size(0, 0), printRes[0] / scanRes[0], printRes[1] / scanRes[1], cv::INTER_NEAREST);
        std::string printDataOutFile = printDataOutDir + "/" + std::to_string(idx) + ".png";
        cv::imwrite(printDataOutFile, resized);
    }
}

//save volume as huge triangle meshes for two separate materials 
void makeVolumeTimeLapse(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::string scanDir = dataDir + "/scanOut/";
    std::string outDir = dataDir + "/meshes/";
    std::string printDataDir = dataDir + "/slicesScaled/";
    std::string outDir0 = dataDir + "/meshes/0/";
    std::string outDir1 = dataDir + "/meshes/1/";
    std::string outDir2 = dataDir + "/meshes/2/";
    createDir(outDir);
    createDir(outDir0);
    createDir(outDir1);
    createDir(outDir2);
    int startIdx = conf.getInt("startIdx");
    int endIdx = conf.getInt("endIdx");
    //subtracted from depth map so that depth values fit in 
    //uint8_t
    //const float scanZOffset = 250;
    std::vector<int> scanSize;
    conf.getIntVector("scanSize", scanSize);
    int w = scanSize[0];
    int h = scanSize[1];
    const float zRes = 10.0;
    const int bgThresh = 50;
    const int printLevel = 100;
    //const float zRes = 10.0f;
    //depth value changes about this much every step.
    float platformStep = 15;// um;
    
    const float voxZRes = 15;
    Array3D8u vol, prevVol;
    loadPreviousVol(conf, vol);
    if (vol.GetSize()[0] == 0) {
        vol.Allocate(w, h, size_t(endIdx * platformStep / voxZRes) + 5);
        std::fill(vol.GetData().begin(), vol.GetData().end(), 0);
    }
    int startSaving = 1;
    //const int maxCol = 954;
    cv::Mat printData;
    //int printIdx0 = conf.getInt("printDataIdx");
    //float scanIdx0 = 0;
    //conf.getFloat("scanIdx0", scanIdx0);
    
    std::vector<float> scanRes = conf.getFloatVector("scanRes");
    std::vector<float> printRes = conf.getFloatVector("printRes");
    //std::vector<float> scanOrigin = conf.getFloatVector("scanOrigin");
    int margin = conf.getInt("margin");
    int dilate_size = 2;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(2 * dilate_size + 1, 2 * dilate_size + 1),
        cv::Point(dilate_size, dilate_size));

    int objCnt = 0;
    const int dilateMat = 3;

    //std::vector<uint8_t> printed(w*h, 255);
    cv::Mat depth , prevDepth;
    float voxRes[3] = { 0.042, 0.04233,0.02 };
    for (int idx = startIdx; idx <= endIdx; idx++) {
        
        //load scan data
        //if (idx >= printIdx0) {
            //int scanIdx = (int)((idx - printIdx0) /3.224 + scanIdx0);
        int scanIdx = idx;// (int)((idx - printIdx0) / 1.5973 + scanIdx0);
        //std::string depthFile = scanDir + std::to_string(scanIdx) + ".bin";
        std::string depthFile = scanDir + std::to_string(scanIdx) + ".png";
        //std::vector<float> scanIn;
        //size_t scanW, scanH;
        //bool status = loadFloatImage(depthFile, scanIn, scanW, scanH);
        depth = cv::imread(depthFile, cv::IMREAD_GRAYSCALE);
        
        if (depth.rows > 0) {
            //depth = cv::Mat::zeros(scanH, scanW, CV_8UC1);
            //for (int row = 0; row < scanH; row++) {
            //    for (int col = 0; col < scanW; col++) {
            //        depth.at<uint8_t>(row, col) = (uint8_t)(scanIn[row*scanW + col] - scanZOffset);
            //    }
            //}
            cv::medianBlur(depth, depth, 3);
            //cv::imwrite("debugDepth.png", depth);
            if (prevDepth.rows == 0) {
                prevDepth = depth;
            }
        }
        //}
        
        //load and shift print data according to scanOrigin
        std::string printDataFile = printDataDir + std::to_string(idx) + ".png";
        cv::Mat printIn = cv::imread(printDataFile);
        //cv::Mat printData = cv::imread(printDataFile);
        cv::Mat printData = cv::Mat::zeros(cv::Size(w, h), CV_8UC1);
        cv::Mat dilatedMask = cv::Mat::zeros(cv::Size(w, h), CV_8UC1);
        for (int row = 0; row < printData.rows; row++) {
            int srcRow = row;
            if (srcRow < 0 || srcRow >= printIn.rows) {
                continue;
            }
            for (int col = 0; col < printData.cols; col++) {
                int srcCol = col;
                if (srcCol < 0 || srcCol >= printIn.cols) {
                    continue;
                }
                cv::Vec3b color = printIn.at<cv::Vec3b>(srcRow, srcCol);
                printData.at<uint8_t>(row, col) = color[2];
                if (color[2] >= 1) {
                    dilatedMask.at<uint8_t>(row, col) = 255;
                }
            }
        }
        //cv::dilate(dilatedMask, dilatedMask, element);
        ////expand footprint of wax to capture any material printed outside of model.
        //for (int row = 0; row < printData.rows; row++) {
        //    for (int col = 0; col < printData.cols; col++) {
        //        uint8_t g = dilatedMask.at<uint8_t>(row, col);
        //        uint8_t p = printData.at<uint8_t>(row, col);
        //        if (g > 0 && p==0) {
        //            printData.at<uint8_t>(row, col) = 1;
        //        }
        //    }
        //}

        float avgHeight = idx * platformStep / voxZRes;
        
        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                //if (col >= maxCol) {
                //    break;
                //}
                uint8_t print = printData.at<uint8_t>(row, col);
                if (print == 0 ){//|| (!printed[row*depth.cols + col])) {
                    continue;
                }
                float height = avgHeight;

                if (depth.cols > 0) {
                  int scanCol = std::min(col + margin, w);

                    float d = depth.at<uint8_t>(row, scanCol);
                    //float prevd = prevDepth.at<uint8_t>(row, scanCol);
                    //float diff = zRes * (prevd - d);  //microns
                    //if depth changed slowly between scans
                    //update prevDepth to current depth
                    //if (std::abs(diff) < 60) {
                    //  prevDepth.at<uint8_t>(row, scanCol) = d;
                    //}
                    //if d is far away but consistent, set scan height
                    // to average - 2. ???
                    //if (d > bgThresh && std::abs(diff) < 30) {
                        //height -= 2;
                        //fill missing build material with wax.
                        //print = 1;
                        //printed[row*depth.cols + col] = 1;
                        //continue;
                    //}
                   // else {
                    //update new surface according to change in height.
                     float diff = clamp(d- printLevel, -50.0f, 50.0f);
                     height += diff / voxZRes;
                    //}
                }
                
                //if this column is not supported, do not add anything
                //otherwise fill the layer gap using the current deposited material.
                int newz = std::floor(height + 0.5f);
                newz = clamp(newz, 0, (int)vol.GetData()[2]-1);
                bool hasSup = false;
                for (int z = newz; z >= std::max(newz - int(150/voxZRes), 0); z--) {
                    uint8_t oldm = vol(col, row, z);
                    if (oldm > 0) {
                        hasSup = true;
                        break;
                    }
                }
                if (idx > startIdx + 100/ voxZRes && !hasSup) {
                    continue;
                }
                for (int z = newz; z >= 0; z--) {
                    uint8_t oldm = vol(col, row, z);
                    if (oldm > 0) {
                        break;
                    }
                    else {
                        vol(col, row, z) = print;
                    }
                }
            }
        }
        //if (((idx-startIdx) % 200==0) ) {
        if(idx==endIdx){
            //if (objCnt == startSaving - 1 && prevVol.data.size() == 0) {
            //    prevVol = vol;
            //}
            //if (objCnt >= startSaving ) {
            //  //render prevVol once and then render the rest changing volume
            //    if (prevVol.data.size() > 0) {
            //        for (size_t vidx = 0; vidx < prevVol.data.size(); vidx++) {
            //            vol.data[vidx] -= prevVol.data[vidx];
            //        }
            //        prevVol.data.clear();
            //    }

                flipY(vol);
                std::string outFile0 = outDir0 + std::to_string(objCnt) + ".obj";
                saveObjRect(outFile0, vol, voxRes, 1);
                std::string outFile1 = outDir1 + std::to_string(objCnt) + ".obj";
                saveObjRect(outFile1, vol, voxRes, 2);
                std::string outFile2 = outDir2 + std::to_string(objCnt) + ".obj";
                saveObjRect(outFile2, vol, voxRes, 3);
                flipY(vol);
            //}
            objCnt++;
            //cv::Mat cvPrinted(cv::Size(w, h), CV_8UC1, printed.data());
            //cv::imwrite(dataDir + "printed" + std::to_string(idx) + ".png", cvPrinted);
            //cv::imwrite(dataDir + "data" + std::to_string(idx) + ".png", 50*printData);
        }
    }
    
}

void saveRaw3D(const ConfigFile & conf) {
    std::string volFile = conf.getString("vol");
    std::string volFileOut = conf.getString("volOut");
    Array3D8u vol;
    std::ifstream in(volFile, std::ifstream::binary);
    if (!in.good()) {
        std::cout << "can not open " << volFile << "\n";
        return;
    }
    int size[3];
    in.read((char*)(&size), sizeof(size));
    vol.Allocate(size[0], size[1], size[2]);
    in.read((char*)vol.GetData().data(), vol.GetData().size());
    in.close();

    if (vol.GetData().size() == 0) {
        return;
    }
    
    Array3D8u volOut;
    float zScale = 0.25f;
    volOut.Allocate(vol.GetData()[0], vol.GetData()[1], (size_t)(vol.GetData()[2] * zScale) + 1);
    std::fill(volOut.GetData().begin(), volOut.GetData().end(), 0);
    int zWindow = std::max(1, (int)(1.0f / zScale));
    for (int x = 0; x < vol.GetSize()[0]; x++) {
        for (int y = 0; y < vol.GetSize()[1]; y++) {
            for (int z = 0; z < volOut.GetSize()[2]; z++) {
                float sum = 0;
                int zStart = std::max(z*4, (int)(z / zScale));
                for (int w = 0; w < zWindow; w++) {
                    int znbr = std::min( zStart + w, (int)vol.GetSize()[2] - 1);
                    sum += (float)vol(x, y, znbr);
                }
                sum = std::min(sum/ zWindow, 255.0f);
                volOut(x, y, z) = (uint8_t)sum;
            }
        }
    }
   
    std::ofstream out(volFileOut, std::ofstream::binary);
    out.write((const char*)volOut.GetData().data(), volOut.GetData().size());
    out.close();
    std::ofstream sizeout(volFileOut + "_size.txt");
    sizeout << volOut.GetSize()[0] << " " << volOut.GetSize()[1] << " " << volOut.GetSize()[2] << "\n";
    sizeout.close();
}

void alignImages(const ConfigFile & conf) {
  ///reference image. image being aligned to
  std::string refFile = conf.getString("refImage");
  std::string inputFile = conf.getString("inputImage");
  cv::Mat refImage = cv::imread(refFile, cv::IMREAD_UNCHANGED);
  cv::Mat inputImage = cv::imread(inputFile, cv::IMREAD_UNCHANGED);
  int type = inputImage.type();
  std::cout << type2str(type)<<"\n";
  cv::Mat HInput, HRef;
  alignImages(inputImage, refImage, HInput, HRef);
  type = HInput.type();
  std::cout << type2str(type) << "\n";
  std::cout << "save homography matrices\n";
  std::string hFile = conf.getString("homography");
  writeHMats(hFile, HInput, HRef);
  
  uint32_t targetSize[2] = { 500,500 };
  cv::Mat refWarped(targetSize[0], targetSize[1], CV_16UC3);
  cv::warpPerspective(refImage, refWarped, HRef, refWarped.size());
  cv::imwrite("scan_warped.png", refWarped);
}

void shaveVol(const ConfigFile & conf) {
    std::string dataDir = conf.getString("dataDir");
    std::vector<int> scanSize;
    conf.getIntVector("fullSize", scanSize);
    int w = scanSize[0];
    int h = scanSize[1];
    const float zRes = 5.05f;
    const int bgThresh = 80;
    const int printLevel = 60;
    const int volMinZ = 200;
    float diffThresh = 200;
    //const float zRes = 10.0f;
    //depth value changes about this much every step.
    float platformStep = 25;// um;

    Array3D8u vol;
    loadPreviousVol(conf, vol);
    if (vol.GetSize()[0] == 0) {
        vol.Allocate(w, h, size_t(200 * platformStep / 10) + 5,0);        
    }
    
    std::vector<float> scanRes = conf.getFloatVector("scanRes");
    std::vector<float> printRes = conf.getFloatVector("printRes");
    std::vector<float> scanOrigin = conf.getFloatVector("scanOrigin");
    int dilate_size = 2;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(2 * dilate_size + 1, 2 * dilate_size + 1),
        cv::Point(dilate_size, dilate_size));

    int objCnt = 0;
    std::cout << "vol size " << vol.GetSize()[0] << " " << vol.GetSize()[1] << " " << vol.GetSize()[2] << "\n";
    std::vector<uint8_t> printData(vol.GetSize()[0] * vol.GetSize()[1], 0);
    //find top of volume.
    int volMaxZ = 0;
    for (int x = 0; x < vol.GetSize()[0]; x++) {
        for (int y = 0; y < vol.GetSize()[1]; y++) {
            int z = 0;
            for (; z < vol.GetSize()[2]; z++) {
                if (vol(x, y, z) == 0) {
                    break;
                }
            }
            if (z > volMinZ) {
                printData[y*vol.GetSize()[0] + x] = vol(x, y, z - 1);
            }
            volMaxZ = std::max(volMaxZ, z - 1);
        }
    }

    //find average height of scan
    std::string depthFile = dataDir + conf.getString("scanFile");
    cv::Mat depth = cv::imread(depthFile, cv::IMREAD_GRAYSCALE);
    if (depth.rows == 0) {
        std::cout << "cannot open image " << depthFile << "\n";
    }
    std::cout << "depth size " << depth.cols << " " << depth.rows << "\n";
    cv::Mat shiftDepth = (bgThresh + 1)*cv::Mat::ones(cv::Size(w, h), CV_8UC1);
    for (int row = 0; row < shiftDepth.rows; row++) {
        int srcRow = row + scanOrigin[1];
        if (srcRow < 0 || srcRow >= depth.rows) {
            continue;
        }
        for (int col = 0; col < shiftDepth.cols; col++) {
            int srcCol = col + scanOrigin[0];
            if (srcCol < 0 || srcCol >= depth.cols) {
                continue;
            }
            uint8_t d = depth.at<uint8_t>(srcRow, srcCol);
            shiftDepth.at<uint8_t>(row, col) = d;
        }
    }
    depth = shiftDepth;

    double sum = 0;
    int matCnt = 0;
    cv::Mat maskedImg = depth;
    for (int row = 0; row < depth.rows; row++) {
        for (int col = 0; col < depth.cols; col++) {
            if (printData[row*depth.cols + col] == 0) {
                maskedImg.at<uint8_t>(row, col) = 0;
                continue;
            }
            sum += depth.at<uint8_t>(row, col);
            matCnt++;
        }
    }
    cv::imwrite(dataDir+"/masked.png", maskedImg);
    sum /= matCnt;
    float voxRes[3] = { 0.042, 0.04233,0.02 };
    std::cout << "avg depth " << sum << "\n";
    std::string outFile0 = dataDir + "/0.obj";
    saveObjRect(outFile0, vol, voxRes, 1);
    std::string outFile1 = dataDir + "/1.obj";
    saveObjRect(outFile1, vol, voxRes, 3);
    system("pause");
}

///\param materialID CV_8UC3
///\param seq used to generate sequential file name
void saveSeparateHeightObj(float * hf, int width, int height,
  const cv::Mat & materialID, int seq, std::string outDir)
{
  std::vector<int> usedMat(3);
  usedMat[0] = 1;
  usedMat[1] = 2;
  usedMat[2] = 3;
  bool saveTrig = true;
  float dx[3] = { 0.064f, 0.05f, 1.0f };
  const char * suffix = ".obj";
  
  for (auto & matId : usedMat) {
    std::vector<int> mask(width*height, 0);

    int nRow = std::min(height, materialID.rows);
    int nCol = std::min(width, materialID.cols);
    for (int row = 0; row < nRow; row++) {
      for (int col = 0; col < nCol; col++) {
        int l = row * width + col;
        cv::Vec3b color = materialID.at<cv::Vec3b>(row, col);
        //also save triangles at void to ground the height field.
        mask[l] = (color[2]==matId || (matId == 1 && color[2] == 0) );
      }
    }
    
    std::string matDir = outDir + "/" + std::to_string(matId) + "/";
    createDir(matDir);
    std::string outfile = sequenceFilename(seq, matDir.c_str(), NULL, 5, suffix);
    saveSelectHeightObj(outfile, hf, mask.data(), width, height, dx, saveTrig);
  }
}
//computes a height map time lapse / digital twin mesh sequence
void depth2meshes(ConfigFile & conf) {
  //mm
  float scanRes[2] = { 0.064,0.05 };
  //float printRes[2] = { 0.032, 0.0635 };
  //float displayRes[2] = { 0.05,0.05 };
  float layerStep = 0.015;
  float scanZRes = 0.01;
  std::string dataDir = conf.getString("dataDir");// "K:/densityCubes/prof";
  //std::string outDir = "K:/densityCubes/png";
  std::string sliceDir = dataDir + "/slicesScaled/";
  std::string scanDir = dataDir + "/scanOut/";
  //std::string sliceOutDir = "K:/densityCubes/scaledSlice";
  std::string objDir =  + "K:/staticMixer/obj/";
  //std::string outDir0 = "K:/densityCubes/vol/0/";
  //std::string outDir1 = "K:/densityCubes/vol/1/";
  //std::string outDir2 = "K:/densityCubes/vol/2/";
  //createDir(outDir0);
  //createDir(outDir1);
  //createDir(outDir2);

  int startIdx = conf.getInt("startIdx");
  int endIdx = conf.getInt("endIdx");
  //subtracted from depth map so that depth values fit in 
  //uint8_t
  //const float scanZOffset = 250;
  std::vector<int> scanSize;
  conf.getIntVector("scanSize", scanSize);
  int w = scanSize[0];
  int h = scanSize[1];

  int status = 0;
  std::vector<std::string> l = listFiles(dataDir, status);
  int morphSize = 5;
  cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(morphSize*2+1, morphSize*2+1), cv::Point(morphSize, morphSize));
  Array3D8u vol;
  int nLayer = endIdx - startIdx + 1;
  float volZRes = 0.015f;
  vol.Allocate(w, h, nLayer,0); 
  cv::Mat prevDepth;
  int margin = conf.getInt("margin");
  cv::Mat prevSlice;// = cv::Mat::zeros(h, w, CV_8UC1);
  //int displaySize[2] = { 4850,1166 };

  //reconstructed heightfield
  std::vector<float>hf(w*size_t(h), 0.0f);
  for (size_t i = 0; i < nLayer; i+=1)
  {
    if (i % 10 == 0) {
      std::cout << "i " << i << "\n";
    }
    int size[2];
    std::string printDataFile = sliceDir + std::to_string(i) + ".png";
    cv::Mat slice = cv::imread(printDataFile);
    if (slice.rows == 0) {
      slice = prevSlice;
    }
    if (prevSlice.rows == 0) {
      prevSlice = slice;
    }
    //std::vector<double> doubledata;
    //std::vector<float> data, croppedData;
    //std::string sliceFile = sliceDir + "/bits" + std::to_string(i) + ".png";
    //loadProf(l[i], size, doubledata);
    //data.resize(doubledata.size(), 0.0f);
    //for (size_t j = 0; j < doubledata.size(); j++) {
    //  double val = doubledata[j];
    //  if (!isValidData(val)) {
    //    continue;
    //  }
    //  data[j] = float(val);
    //}
    //cv::Mat scan(size[1], size[0], CV_32FC1, data.data());
    //std::string outName = outDir + "/" + std::to_string(i) + ".png";
    //scan.convertTo(scanfloat, CV_8UC1, 255, 125);
    //cv::resize(scan, scan, cv::Size(), scanRes[0]/displayRes[0] , scanRes[1]/displayRes[1]);

    //cv::Mat slice = cv::imread(sliceFile, CV_LOAD_IMAGE_COLOR);
    //cv::resize(slice, slice, cv::Size(), printRes[0] / displayRes[0], printRes[1]/ displayRes[1], CV_INTER_NN);
    //std::string sliceOutFile = sliceOutDir + "/" + std::to_string(i) + ".png";
    //cv::Rect roi(0, 0, scan.cols, scan.rows);
    //roi.width = std::min(roi.width, displaySize[0]);
    //roi.height = std::min(roi.height, displaySize[1]);
    //croppedData.resize(roi.width * roi.height);
    //roi.width = 1000;
    //roi.height = 1000;
    //cv::Mat cropScan(roi.height, roi.width, CV_32FC1, croppedData.data());
    //cannot blur float with window > 5.
    //cv::medianBlur(cropScan, cropScan, 5);
    //scan(roi).copyTo(cropScan);
    //double mn = 0, mx = 0, avg = 0;
    //size_t validCnt = 0;
   
    //set area near boundary of print data to 0.
    //cv::resize(slice, slice, cv::Size(slice.cols / 2, slice.rows / 2), CV_INTER_NN);

   // cv::Mat eroded=cv::Mat::zeros(slice.size(), CV_8UC1);

    //cv::erode(eroded, eroded, element);

    std::string depthFile = scanDir + std::to_string(i) + ".png";
    cv::Mat depth = cv::imread(depthFile, cv::IMREAD_GRAYSCALE);
    if (prevDepth.rows == 0) {
      prevDepth = cv::Mat::zeros(slice.rows, slice.cols, CV_8UC1);
    }
    if (depth.rows == 0) {
      depth = prevDepth;
    }
    else {
      cv::medianBlur(depth, depth, 3);
      prevDepth = depth;
    }

    //int scaledWidth = roi.width / 2;
    //int scaledHeight = roi.height / 2;

    //cv::Mat scaledMat(scaledHeight, scaledWidth, CV_32FC1 , scaledData.data());
    //cv::resize(cropScan, scaledMat, scaledMat.size(), 0,0, CV_INTER_AREA);
    //cv::imwrite(outName, scaledMat * 255 + 125);
    //set area outside of print data to 0.
    for (int row = 0; row < slice.rows; row++) {
      for (int col = 0; col < slice.cols; col++) {
        if (row >= h|| col >= w) {
          continue;
        }
        int mat = slice.at<cv::Vec3b>(row, col)[2];
        int erodeVal = 1;// eroded.at<uint8_t>(row, col);
        if (mat == 0 || erodeVal == 0) {
          continue;
          //scaledMat.at<float>(row, col) = 0.0f;
        //  hf[row * w + col] = 0.0f;
        }
        //float depth = double(scaledMat.at<float>(row, col));
        //mn = std::min(mn, val);
        //mx = std::max(mx, val);
        //avg += val;
        //validCnt++;
        int scanCol = col + margin;
        scanCol = std::min(depth.cols, scanCol);
        float d = (scanZRes * depth.at<uint8_t>(row, scanCol) - 1.0f);
        if (d < -0.2) {
          d = -0.2;
        }
        if (d > 0.2) {
          d = 0.2;
        }
        hf[row*w + col] = std::max(hf[row*w + col], d + layerStep * i);

      }
    }

    //the material mask never shrinks so that previously printed area is always rendered.
    for (int row = 0; row < slice.rows; row++) {
      for (int col = 0; col < slice.cols; col++) {
        cv::Vec3b color = slice.at<cv::Vec3b>(row, col);
        if (color[2] > 0) {
          //    eroded.at<uint8_t>(row, col) = 255;
          prevSlice.at<cv::Vec3b>(row, col) = color;
        }
      }
    }
    slice = prevSlice;
    //if (validCnt > 0) {
    //  avg /= validCnt;
    //}
    //std::cout << "min max avg " << mn << " " << mx << " " << avg << "\n";
    //int nRow = std::min(int(vol.size[1]), h);
    //nRow = std::min(nRow, slice.rows);
    //int nCol = std::min(int(vol.size[0]), w);
   
    //for (int row = 0; row < nRow; row++) {
    //  for (int col = 0; col < nCol; col++) {
    //    float h = hf[row*w + col];
    //    int newMat = slice.at<cv::Vec3b>(row, col)[2];
    //    if (newMat == 0) {
    //      continue;
    //    }
    //    int intHeight = int(h / volZRes);
    //    for (int z = intHeight; z >= 0; z--) {
    //      uint8_t oldMat = vol(col, row, z);
    //      if (oldMat > 0) {
    //        break;
    //      }
    //      vol(col, row, z) = uint8_t( newMat);
    //    }
    //  }
    //}
    if (i % 10 == 0 && i>=1000) {
      saveSeparateHeightObj(hf.data(), w, h, slice, i, objDir);
    }
    //cv::imwrite(sliceOutFile, slice);
  //  if (i == 1420 || i == 800 || i == 80) {
  //   // flipY(vol);
  //    std::string outFile0 = outDir0 + std::to_string(i) + ".obj";
  //    saveObjRect(outFile0, vol, volZRes*1000, 1);
  //    std::string outFile1 = outDir1 + std::to_string(i) + ".obj";
  //    saveObjRect(outFile1, vol, volZRes * 1000, 2);
  //   // flipY(vol);
  //    vol.saveFile(dataDir + "/vol"+std::to_string(i)+".vol");
  //  }
  }
}

void LoadSlices(std::string dir,
  Array3D8u&vol, Vec3u mn, Vec3u mx) {

}

void slices2Meshes(const ConfigFile& conf)
{
  std::string sliceDir = conf.getString("sliceDir");
  Array3D8u vol;
  Vec3u mn, mx;
  LoadSlices(sliceDir, vol, mn, mx);
}

void cropScanData(const ConfigFile& conf) {
  std::string dataDir = conf.getString("dataDir");
  std::string scanDir = dataDir + "/scan";
  std::string scanOutDir = dataDir + "/scanOut/";
  createDir(scanOutDir);
  int startIdx = conf.getInt("startIdx");
  int endIdx = conf.getInt("endIdx");
  //leave a margin of 10 pixels when cropping.
  int margin = conf.getInt("margin");
  std::vector<float> img;
  const float outputRes = 0.01f;//mm
  float minDepth = -1.0f;//mm;
  float maxDepth = 1.0f;

  std::vector<int> scanOrigin(3, 0);
  conf.getIntVector("scanOrigin", scanOrigin);
  std::vector<int> scanSize(2, 0);
  conf.getIntVector("scanSize", scanSize);

  cv::Rect roi(scanOrigin[0] - margin, scanOrigin[1], scanSize[0], scanSize[1]+margin);
  for (int i = startIdx; i <= endIdx; i++) {
    std::string filename = scanDir + "/" + std::to_string(i) + ".png";
    cv::Mat input = cv::imread(filename, cv::IMREAD_UNCHANGED);
    if (input.rows == 0) {
      continue;
    }
    cv::Mat output = cv::Mat(input.rows, input.cols, CV_8UC1);
    img.resize(input.rows * input.cols);
    img.assign((float*)input.data, (float*)input.data + input.total());
    for (int row = 0; row < input.rows; row++) {
      for (int col = 0; col < input.cols; col++) {
        float depth = img[row * input.cols + col];
        //if (depth > -1) {
        //  std::cout << "debug";
        //}
        depth = std::max(0.0f, depth - minDepth);
        depth = std::min(depth, maxDepth - minDepth);
        uint8_t pixVal = (uint8_t)(depth / outputRes);
        output.at<uint8_t>(row, col) = pixVal;
      }
    }

    std::string outputFile = scanOutDir + std::to_string(i) + ".png";
    cv::imwrite(outputFile, output(roi));
  }
}

Array3D<uint8_t>MakeRaftPattern(unsigned sx, unsigned sy, unsigned sz) {
  Array3D<uint8_t> raft;
  raft.Allocate(sx, sy, sz);
  raft.Fill(1);
  const int anchorHeight = 40;
  const int topSupLayers = 20;
  //not enough layers to make anchors
  if (sz < topSupLayers) {
    return raft;
  }

  unsigned zMax = sz - topSupLayers;
  unsigned bigHeight = anchorHeight / 2;
  float yScale = 2.0f;

  const int pillarSizeX = 30;
  const int pillarSizeY = 15;
  const int pillarHeight = 10;
  int smallFrac= 4;
  int bigFrac = 3;

  for (unsigned z = 0; z < sz; z++) {
    bool small = false;
    bool makePillars = false;
    if (zMax - z > bigHeight) {
      small = true;
    }
    if (z >= zMax && z < zMax + pillarHeight) {
      makePillars = true;
    }
    for (unsigned y = 0; y < sy; y++) {
      int dy = std::abs(int(y) - int(sy / 2));
      for (unsigned x = 0; x < sx; x++) {
        int dx = std::abs(int(x) - int(sx / 2));
        if (z < zMax ){
          int xbound = sx / bigFrac;
          int ybound = sy / bigFrac;
          if (small) {
            xbound = sx / smallFrac;
            ybound = sy / smallFrac;
          }
          if (dx < xbound || dy < ybound) {
            raft(x, y, z) = 2;
          }
        }
        else if (makePillars) {
          int gridIdxY = dy / pillarSizeY;
          int gridIdxX = dx / pillarSizeX;
          uint8_t id = 1;
          if (gridIdxY % 3 == 1 && gridIdxX % 3 == 1) {
            id = 2;
          }
          raft(x, y, z) = id;
        }
        else {
          raft(x, y, z) = 1;
        }
        //std::cout << int(raft(x, y, z));
      }
      //std::cout << "\n";
    }
    //std::cout << "\n";
  }
  return raft;
}

uint8_t SampleRaft(const Array3D8u &raft , unsigned x, unsigned y, unsigned z)
{
  unsigned coord[3];
  Vec3u size = raft.GetSize();
  if (size[0] == 0 || size[1] == 0 || size[2] == 0) {
    return 1;
  }
  coord[0] = x % size[0];
  coord[1] = y % size[1];
  coord[2] = z;
  if (z >= size[2]) {
    coord[2] = size[2] - 1;
  }
  return raft(coord[0], coord[1], coord[2]);
}

///tiles raft pattern in cell
Array2D8u MakeRaft(uint8_t anchorId, const Array3D8u & raft,
  unsigned z, Vec3u cellGridSize) {
  Vec3u rSize = raft.GetSize();
  Array2D8u slice(cellGridSize[0], cellGridSize[1]);
  uint8_t supId = 1;

  for (unsigned y = 0; y < slice.GetSize()[1]; y++) {
    for (unsigned x = 0; x < slice.GetSize()[0]; x++) {
      slice(x, y) = supId;
      uint8_t pat = SampleRaft(raft, x, y, z);
      if (pat == 2) {
        slice(x, y) = anchorId;
      }      
    }
  }
  return slice;
}

void CopyImage(Array2D8u& dst,
  const Array2D8u& src, unsigned x0, unsigned y0)
{
  for (unsigned x = 0; x < src.GetSize()[0]; x++) {
    if (x + x0 >= dst.GetSize()[0]) {
      break;
    }
    for (unsigned y = 0; y < src.GetSize()[1]; y++) {
      if (y + y0 >= dst.GetSize()[1]) {
        break;
      }
      dst(x + x0, y + y0) = src(x, y);
    }
  }
}

void CopyTransparentImage(Array2D8u& dst,
  const Array2D8u& src, unsigned x0, unsigned y0)
{
  for (unsigned x = 0; x < src.GetSize()[0]; x++) {
    if (x + x0 >= dst.GetSize()[0]) {
      break;
    }
    for (unsigned y = 0; y < src.GetSize()[1]; y++) {
      if (y + y0 >= dst.GetSize()[1]) {
        break;
      }
      if (src(x, y)) {
        dst(x + x0, y + y0) = src(x, y);
      }
    }
  }
}

struct RectPart
{
  Vec2u botSize;
  Vec2u topSize;
  unsigned height;
  unsigned erode;
  uint8_t mat;
  RectPart():height(0), erode(0),mat(2) {
  }
  void Draw(Array2D8u& dst, unsigned x0, unsigned y0, unsigned z) {
    Vec2u maxSize;
    maxSize[0] = std::max(botSize[0], topSize[0]);
    maxSize[1] = std::max(botSize[1], topSize[1]);
    unsigned x1 = x0 + maxSize[0];
    unsigned y1 = y0 + maxSize[1];
    if (x1 > dst.GetSize()[0]) {
      x1 = dst.GetSize()[0];
    }
    if (y1 > dst.GetSize()[1]) {
      y1 = dst.GetSize()[1];
    }

    float weight = z/float(height);
    weight = std::min(1.0f, weight);
    Vec2u rectSize;
    rectSize[0] = botSize[0] * (1.0f - weight) + weight * topSize[0];
    rectSize[1] = botSize[1] * (1.0f - weight) + weight * topSize[1];
    for (unsigned y = y0; y < y1; y++) {
      for (unsigned x = x0; x < x1; x++) {
        dst(x, y) = 1;
      }
    }
    if (z >= height) {
      return;
    }
    Vec2u margin;
    margin[0] = (maxSize[0] - rectSize[0])/2;
    margin[1] = (maxSize[1] - rectSize[1]) / 2;

    y1 = y0 + margin[1] + rectSize[1];
    x1 = x0 + margin[0] + rectSize[0];
    for (unsigned y = y0+margin[1]; y < y1; y++) {
      for (unsigned x = x0+margin[0]; x < x1; x++) {
        dst(x, y) = mat;
        if (x - x0 - margin[0] < erode || x1 - x - 1 < erode) {
          dst(x, y) = 0;
        }
        if (y - y0 - margin[1] < erode || y1 - y - 1 < erode) {
          dst(x, y) = 0;
        }
      }
    }
  }
};

void FillRect(Array2D8u & dst, unsigned x0, unsigned x1,
  unsigned y0, unsigned y1, uint8_t val) {
  for (unsigned y = y0; y < y1; y++) {
    for (unsigned x = x0; x < x1; x++) {
      dst(x, y) = val;
    }
  }
}

void addSpitBar(Array2D8u& slice)
{
  
  std::vector<char> materials(3,1);
  materials[1] = 2;
  materials[2] = 3;
  unsigned spitWidth = 100;
  //right spit bar
  int totalSpitWidth = 2 * spitWidth * materials.size();
  Array2D8u oldSlice = slice;
  Vec2u oldSize = oldSlice.GetSize();
  slice.Allocate(oldSize[0] + totalSpitWidth, oldSize[1]);
  slice.Fill(0);
  for (size_t row = 0; row < oldSize[1]; row++) {
    for (size_t col = 0; col < oldSize[0]; col++) {
      slice(col + totalSpitWidth/2, row) = oldSlice(col, row);
    }
  }
  Vec2u sliceSize = slice.GetSize();
  for (unsigned m = 0; m < materials.size(); m++) {
    for (unsigned x = 0; x < spitWidth; x++) {
      unsigned dstx = (sliceSize[0] - 1) - spitWidth * (unsigned(materials.size()) - m) + x;
      if (dstx < 0 || dstx >= sliceSize[0]) {
        continue;
      }
      for (unsigned y = 0; y < sliceSize[1]; y++) {
        slice(dstx, y) = materials[m];
      }
    }
  }
  for (unsigned m = 0; m < materials.size(); m++) {
    for (unsigned x = 0; x < spitWidth; x++) {
      unsigned dstx = spitWidth * m + x;
      if (dstx < 0 || dstx >= sliceSize[0]) {
        continue;
      }
      for (unsigned y = 0; y < sliceSize[1]; y++) {
        slice(dstx, y) = materials[m];
      }
    }
  }  
}

Array2D8u DrawText(const std::string& s, uint8_t val)
{
  unsigned width = 0;
  unsigned MAXLEN = 10000;
  unsigned len = std::min(unsigned(s.size()), MAXLEN);
  std::vector<FontGlyph> glyphs(len);
  unsigned height = 0;
  Array2D8u pic;
  if (s.size() == 0) {
    return pic;
  }
  for (unsigned i = 0; i < len; i++ ) {
    glyphs[i] = avenir.GetBitmap(s[i]);
    width += glyphs[i].width;
  }  
  height = glyphs[0].height;
  pic.Allocate(width, height, 0);
  unsigned col0 = 0;
  for (unsigned i = 0; i < len; i++) {
    const FontGlyph& g = glyphs[i];
    for (unsigned row = 0; row < height; row++) {
      for (unsigned col = 0; col < g.width; col++) {
        uint8_t input = g(col, row);
        if (input > 0) {
          pic(col + col0, row) = val;
        }
      }
    }
    col0 += g.width;
  }
  return pic;
}

Array2D8u Scale(const Array2D8u& input, float sx, float sy)
{
  Vec2u inSize = input.GetSize();
  Array2D8u out(inSize[0] * sx, inSize[1] * sy);
  Vec2u outSize = out.GetSize();
  for (unsigned row = 0; row < outSize[1]; row++) {
    unsigned srcRow = row / sy;
    if (srcRow >= inSize[1]) {
      break;
    }
    for (unsigned col = 0; col < outSize[0]; col++) {
      unsigned srcCol = col / sx;
      if (srcCol >= inSize[0]) {
        break;
      }
      out(col, row) = input(srcCol, srcRow);
    }
  }
  return out;
}

void FullPlateCalib(const ConfigFile& conf)
{
  std::string outDir = conf.getString("outDir");
  std::vector<float> printRes = conf.getFloatVector("printRes");
  std::vector<float> traySize = conf.getFloatVector("traySize");
  std::vector<float> partSize = conf.getFloatVector("partSize");
  std::vector<float> margin = conf.getFloatVector("margin");
  std::vector<float> coating = conf.getFloatVector("coating");
  const unsigned DIM = 3;
  float raft = 1.0f;
  conf.getFloat("raft", raft);
  float totalHeight = raft + partSize[2]+coating[2];
  size_t numLayers = size_t(totalHeight/ printRes[2]);
  //each cell contains a part
  Vec3f cellSize;
  cellSize[0] = partSize[0] + 2 * margin[0] + 2*coating[0];
  cellSize[1] = partSize[1] + 2 * margin[1] + 2 * coating[1];
  cellSize[2] = partSize[2] + raft + coating[2];
  Vec3u coatingPix;
  Vec3u cellGridSize;
  size_t raftLayers = raft / printRes[2];
  for (size_t i = 0; i < DIM; i++) {
    cellGridSize[i] = cellSize[i] / printRes[i];
    coatingPix[i] = coating[i] / printRes[i];
  }
  Vec2u marginPix;
  marginPix[0] = margin[0] / printRes[0];
  marginPix[1] = margin[1] / printRes[1];
  /// cell grid size without the margins
  Vec3u partGridSize;
  partGridSize[0] = (partSize[0] + 2 * coating[0]) / printRes[0];
  partGridSize[1] = (partSize[1] + 2 * coating[1]) / printRes[1];
  partGridSize[2] = (partSize[2]) / printRes[2];

  uint8_t matId = 2;
  if (conf.hasOpt("matId")) {
    matId = conf.getInt("matId");
  }
  Vec2u raftPatSize(400, 200);
  Array3D8u raftPat=MakeRaftPattern(raftPatSize[0], raftPatSize[1], raftLayers);
  Array2D8u raftSlice(raftPat.GetSize()[0], raftPat.GetSize()[1]);
  for (size_t y = 0; y < raftPat.GetSize()[1]; y++) {
    for (size_t x = 0; x < raftPat.GetSize()[0]; x++) {
      raftSlice(x, y) = 50*raftPat(x, y, 0);
    }
  }
  SavePng(outDir + "/raft.png", raftSlice);
  Vec2u numParts;
  Vec3u trayGridSize;
  for (size_t i = 0; i < DIM; i++) {
    trayGridSize[i] = traySize[i] / printRes[i];
  }
  numParts[0] = trayGridSize[0] / cellGridSize[0];
  numParts[1] = trayGridSize[1] / cellGridSize[1];

  Vec2u printSize;
  printSize[0] = numParts[0] * cellGridSize[0];
  printSize[1] = numParts[1] * cellGridSize[1];
  std::vector<RectPart> parts(numParts[0]*numParts[1]);

  unsigned textLayers = 20;
  unsigned textScaleX = 6;
  unsigned textScaleY = 3;
  for (unsigned cx = 0; cx < numParts[0]; cx++) {
    //vertical,expanding or contracting 
    int shape = cx % 3;
    int erode = (cx / 3) % 3;
    for (unsigned cy = 0; cy < numParts[1]; cy++) {
      RectPart& p = parts[cy * numParts[0] + cx];
      p.erode = erode;
      p.height = partGridSize[2];
      p.topSize[0] = unsigned(partSize[0] / printRes[0] + 0.5f);
      p.topSize[1] = unsigned(partSize[1] / printRes[1] + 0.5f);
      p.botSize[0] = unsigned(partSize[0] / printRes[0] + 0.5f);
      p.botSize[1] = unsigned(partSize[1] / printRes[1] + 0.5f);
      if (shape == 1) {
        p.botSize[0] /= 2;
        p.botSize[1] /= 2;
      }
      else if (shape == 2) {
        p.topSize[0] /= 2;
        p.topSize[1] /= 2;
      }
      p.mat = matId;
    }
  }

  for (size_t z = 0; z < numLayers; z++) {
    std::cout << z << "\n";
    Array2D8u slice(printSize[0], printSize[1]);
    slice.Fill(0);
    for (unsigned cx = 0; cx < numParts[0]; cx++) {
      for (unsigned cy = 0; cy < numParts[1]; cy++) {
        unsigned x0 = cx * cellGridSize[0] + marginPix[0];
        unsigned y0 = cy * cellGridSize[1] + marginPix[1];
        if (z <= raftLayers) {
          Array2D8u cellSlice = MakeRaft(matId, raftPat, z, partGridSize);
          CopyImage(slice, cellSlice, x0, y0);
        }
        else {
          RectPart& p = parts[cy * numParts[0] + cx];
          FillRect(slice, x0, x0 + partGridSize[0], y0, y0 + partGridSize[1], 1);
          p.Draw(slice, x0 + coatingPix[0], y0 + coatingPix[1], z-raftLayers-1);
          if (z-raftLayers<textLayers) {
            std::ostringstream oss;
            oss << cx << "," << cy<<" er"<<p.erode;
            std::string label = oss.str();
            Array2D8u text = DrawText(label, 1);
            //text = Scale(text, textScaleX, textScaleY);
            CopyTransparentImage(slice, text, x0 + coatingPix[0] + partGridSize[0] / 3, y0 + coatingPix[1] + partGridSize[1] / 3);
          }
        }
      }
    }
    addSpitBar(slice);
    std::string outname = outDir + std::to_string(z)+".png";
    //for (unsigned y = 0; y < slice.GetSize()[1]; y++) {
    //  for (unsigned x = 0; x < slice.GetSize()[0]; x++) {
    //    slice(x, y) = slice(x, y) * 50;
    //  }
    //}
    SavePng(outname, slice);
  }
}

int LoadPng(const std::string& filename, Array2D8u& slice)
{
  std::filesystem::path p(filename);
  if (!(std::filesystem::exists(p) && std::filesystem::is_regular_file(p))) {
    return -1;
  }
  std::vector<unsigned char> buf;
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  unsigned error = lodepng::load_file(buf, filename.c_str());
  if (error) {
    std::cout << "lodepng load_file error " << error << ". file name " << filename << "\n";
    return -2;
  }
  unsigned w, h;
  slice.GetData().clear();
  error = lodepng::decode(slice.GetData(), w, h, state, buf);
  if (error) {
    std::cout << "decode error " << error << "\n";
    return -3;
  }
  slice.SetSize(w, h);
  return 0;
}

int SavePng(const std::string& filename, Array2D8u& slice)
{
  std::ofstream out(filename, std::ofstream::binary);
  if (!out.good()) {
    return -1;
  }
  std::vector<unsigned char> buf;
  lodepng::State state;
  // input color type
  state.info_raw.colortype = LCT_GREY;
  state.info_raw.bitdepth = 8;
  // output color type
  state.info_png.color.colortype = LCT_GREY;
  state.info_png.color.bitdepth = 8;
  state.encoder.auto_convert = 0;
  std::vector<uint8_t> outbuf;
  Vec2u size = slice.GetSize();
  int error = lodepng::encode(outbuf, slice.GetData(), size[0], size[1],
    state);
  if (error) {
    std::cout << "encode png error " << error << "\n";
    return -3;
  }
  out.write((const char *)outbuf.data(), outbuf.size());
  return 0;
}

void TestFont() {
  char c0 = avenir.unicode_first;
  char c1 = avenir.unicode_last;
  for (char c = c0; c <= c1; c++) {
    FontGlyph g = avenir.GetBitmap(c);
    for (uint32_t row = 0; row < g.height; row++) {
      for (uint32_t col = 0; col < g.width; col++) {
        uint8_t bit = g(col, row);
        if (bit) {
          std::cout << "*";
        }
        else {
          std::cout << " ";
        }
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

void TestWritePng() {
  Array2D8u img(500,400);
  SavePng("test.png", img);
}

void TestDrawText()
{
  std::ostringstream oss;
  oss << "x " << 15 << " y " << 2 << " erode " << 2;
  std::string label = oss.str();
  Array2D8u text = DrawText(label, 50);
  text = Scale(text, 6, 3);
  SavePng("testText.png", text);
}

int main(int argc, char * argv[])
{
  TestDrawText();
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " config.txt";
        return -1;
    }
    ConfigFile conf;
    conf.load(argv[1]);
    int opCode = -1;
    conf.getInt("opCode", opCode);
    switch (opCode) {
    case 1:
      depth2meshes(conf);
      break;
    case 2:
      slices2Meshes(conf);
      break;
    case 3:
      FullPlateCalib(conf);
      break;
    case 8:
      testSaveVolume(conf);
      break;    
    case 12:
      saveRaw3D(conf);
      break;
    case 18:
      computeSurface(conf);
      break;
    case 21:
      extractPrintData(conf);
      break;
    case 22:
      makeTimeLapse(conf);
      break;
    case 23:
      scalePrintData(conf);
      break;
    case 24:
      //crop scan data and convert to gray scale image for human readability
      cropScanData(conf);
      break;
    case 25:
      alignImages(conf);
      break;
    case 26:
      makeVolumeTimeLapse(conf);
      break;
    case 27:
      shaveVol(conf);
      break;
    case 28:
      StackColorVol(conf);
      break;
    default:
      std::cout << "unknown op code " << opCode << "\n";
      break;
    }
    return 0;
}

std::string type2str(int type) {
  std::string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);

  switch (depth) {
  case CV_8U:  r = "8U"; break;
  case CV_8S:  r = "8S"; break;
  case CV_16U: r = "16U"; break;
  case CV_16S: r = "16S"; break;
  case CV_32S: r = "32S"; break;
  case CV_32F: r = "32F"; break;
  case CV_64F: r = "64F"; break;
  default:     r = "User"; break;
  }

  r += "C";
  r += (chans + '0');

  return r;
}
