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
#include "MorphOp.h"
#include "cvMatUtil.hpp"
#include "Avenir.h"
#include "ImageIO.h"
#include "SliceGen.h"
#include "SliceToMesh.h"
#include <string>
#include <filesystem>
#include <vector>

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
    SaveVolAsObjMesh(outFile0, vol, voxRes, 1);
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
                SaveVolAsObjMesh(outFile0, vol, voxRes, 1);
                std::string outFile1 = outDir1 + std::to_string(objCnt) + ".obj";
                SaveVolAsObjMesh(outFile1, vol, voxRes, 2);
                std::string outFile2 = outDir2 + std::to_string(objCnt) + ".obj";
                SaveVolAsObjMesh(outFile2, vol, voxRes, 3);
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
    SaveVolAsObjMesh(outFile0, vol, voxRes, 1);
    std::string outFile1 = dataDir + "/1.obj";
    SaveVolAsObjMesh(outFile1, vol, voxRes, 3);
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

void DownsizeScansX() {
  std::string scanDir = "H:/nature/scan1109/";
  std::string scanDirOut = "H:/nature/scan0.5x/";
  unsigned startIdx = 3880;
  unsigned endIdx = 3880;
  for (size_t i = startIdx; i <= endIdx; i++) {
    std::string inFile = scanDir + "/scan_" + std::to_string(i) + ".png";
    cv::Mat in = cv::imread(inFile,cv::IMREAD_GRAYSCALE);
    if (in.empty()) {
      continue;
    }
    cv::threshold(in, in, 200, 255, cv::THRESH_TOZERO_INV);
    cv::Mat out;
    cv::resize(in, out, cv::Size(0, 0), 0.5, 1.0, cv::INTER_LINEAR);
    std::string outFile = scanDirOut + "/" + std::to_string(i) + ".png";
    cv::imwrite(outFile, out);
  }
}

void DownsizePrintsX() {
  std::string scanDir = "H:/nature/print1109/";
  std::string scanDirOut = "H:/nature/print0.5x/";
  unsigned startIdx = 4;
  unsigned endIdx = 4792;
  for (size_t i = startIdx; i < endIdx; i++) {
    std::string inFile = scanDir + "/slice_" + std::to_string(i) + "_0.png";
    cv::Mat in = cv::imread(inFile, cv::IMREAD_GRAYSCALE);
    if (in.empty()) {
      continue;
    }
    cv::Mat out;
    cv::resize(in, out, cv::Size(0, 0), 0.5, 1.0, cv::INTER_NEAREST);
    std::string outFile = scanDirOut + "/" + std::to_string(i) + ".png";
    cv::imwrite(outFile, out);

    std::string inFile1 = scanDir + "/slice_" + std::to_string(i) + "_1.png";
    in = cv::imread(inFile, cv::IMREAD_GRAYSCALE);
    cv::resize(in, out, cv::Size(0, 0), 0.5, 1.0, cv::INTER_NEAREST);
    outFile = scanDirOut + "/" + std::to_string(i+1) + ".png";
    cv::imwrite(outFile, out);

  }
}

void UpdateDepth(Array2Df & depth, const Array2D8u & scan, const Array2D8u & printMask, float z0) {
  Vec2u size = scan.GetSize();
  float scanRes = 0.008f;
  float maxInc = 0.24f;
  float maxDec = 0.15f;

  float maxScanVal = 20.0f;
  for (unsigned row = 0; row < size[1]; row++) {
    const uint8_t *srcRow = scan.DataPtr() + row * size[0];
    float* dstRow = &depth.GetData()[row * size[0]];
    const uint8_t* maskPtr = printMask.DataPtr() + row * size[0];
    for (unsigned col = 0; col < size[0]; col++) {
      if (maskPtr[col] == 0) {
        continue;
      }
      float s = srcRow[col];
      
       s -= 100;
       s = std::min(s, maxScanVal);
       s = z0 + s * scanRes;
       
       float oldh = dstRow[col];
       float newh = std::min(oldh + maxInc, s);
       newh = std::max(newh, oldh);
       dstRow[col] = newh;
    }
  }
}

void MaskOneScan(const Array2Df& depth, Array2D8u& scan, float z0) {
  Vec2u size = scan.GetSize();
  float scanRes = 0.008f;
  float maxInc = 0.24f;
  float scanRange = 1.0f;
  for (unsigned row = 0; row < size[1]; row++) {
    uint8_t* dstRow = scan.DataPtr() + row * size[0];
    const float* srcRow = &depth.GetData()[row * size[0]];
    for (unsigned col = 0; col < size[0]; col++) {
      float d = srcRow[col];
      d-=z0;
      float s = (dstRow[col]-100.0f) * scanRes;

      float depthToScanVal = d / scanRes + 100;
      depthToScanVal = std::max(0.0f, depthToScanVal);
      depthToScanVal = std::min(140.0f, depthToScanVal);

      if (s > d + maxInc || s<d) {
        dstRow[col] = depthToScanVal;
      }
    }
  }
}

void FloatToUint8(const Array2Df & in, Array2D8u & out, float z0) {
  Vec2u size = in.GetSize();
  out.Allocate(size[0], size[1]);
  float range = 1;
  float zres = 0.01;
  for (unsigned row = 0; row < size[1]; row++) {
    uint8_t* dstRow = out.DataPtr() + row * size[0];
    const float* srcRow = &(in.GetData()[row * size[0]]);
    for (unsigned col = 0; col < size[0]; col++) {
      float h = (srcRow[col] - z0 + 1);
      h = std::max(0.0f, h);
      h /= zres;
      h = std::min(255.0f, h);
      dstRow[col] = uint8_t(h);
    }
  }
}

void Thresh(Array2D8u& arr, uint8_t thresh) {
  auto& data = arr.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] > thresh) {
      data[i] = 255;
    }
    else {
      data[i] = 0;
    }
  }
}

void MaskScans() {
  std::string scanDir = "H:/nature/scan0.5x/";
  std::string printDir = "H:/nature/print0.5x/";
  std::string scanDirOut = "H:/nature/cleanScan/";
  unsigned startIdx = 118;
  unsigned endIdx = 3880;
  Array2Df depth;
  float zres = 0.025f;
  int PrintDilateRad = 3;
  for (size_t i = startIdx; i <= endIdx; i++) {
    std::string scanFile = scanDir + "/" + std::to_string(i) + ".png";
    std::string printFile = printDir + "/" + std::to_string(i) + ".png";

    Array2D8u scan;
    LoadPngGrey(scanFile, scan);
    if (scan.Empty()) {
      continue;
    }
    Array2D8u print;
    LoadPngGrey(printFile, print);

    Array2D8u printMask = print;
    Thresh(printMask,1);
    Dilate(printMask, PrintDilateRad, PrintDilateRad);
    Vec2u scanSize = scan.GetSize();
    if (i == startIdx) {
      depth.Allocate(scanSize[0], scanSize[1]);
      depth.Fill(0);
    }
    float z0 = (i - startIdx) * zres;
    UpdateDepth(depth, scan, printMask, z0);
    MaskOneScan(depth, scan, z0);
    std::string outFile = scanDirOut + "/" + std::to_string(i) + ".png";
    SavePngGrey(outFile, scan);

    Array2D8u depthOut;
    if ((i - 4) % 60 == 0) {
      FloatToUint8(depth, depthOut, z0); 
      std::string outFile = scanDirOut + "/depth" + std::to_string(i) + ".png";
      SavePngGrey(outFile, depthOut);
    }
  }
}

void UpdateDepth(Array2Df& depth, const Array2D8u& scan, float z0) {
  Vec2u size = scan.GetSize();
  float scanRes = 0.008f;
  float maxInc = 0.24f;
  float maxDec = 0.15f;
  float maxScanVal = 20.0f;
  for (unsigned row = 0; row < size[1]; row++) {
    const uint8_t* srcRow = scan.DataPtr() + row * size[0];
    float* dstRow = &depth.GetData()[row * size[0]];
    for (unsigned col = 0; col < size[0]; col++) {
      float s = srcRow[col];
      s -= 100;
      if (s < -40) {
        continue;
      }
      s = std::min(s, maxScanVal);
      s = z0 + s * scanRes;
      
      float oldh = dstRow[col];
      float newh = std::min(oldh + maxInc, s);
      newh = std::max(newh, oldh);
      dstRow[col] = newh;
    }
  }
}

void SaveHeightWithUV(std::string file, unsigned width, unsigned height, float*dx, float duv) {
  std::ofstream out(file);
  if (!out.good()) {
    return;
  }
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      out << "v " << j * dx[0] << " " << i * dx[1] << " " << 0 << "\n";
    }
  }
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      out << "vt " << j * duv << " " << i * duv << "\n";
    }
  }
  for (int i = 0; i < height - 1; i++) {
    for (int j = 0; j < width - 1; j++) {
      out << "f " << (i * width + j + 1) << "/" << (i * width + j + 1) << " " << (i * width + j + 2)
          << "/" << (i * width + j + 2) << " " << ((i + 1) * width + j + 1) << "/"
          << ((i + 1) * width + j + 1) << "\n";
      out << "f " << ((i + 1) * width + j + 1) << "/" << ((i + 1) * width + j + 1) << " "
          << (i * width + j + 2) << "/" << (i * width + j + 2) << " " << ((i + 1) * width + j + 2)
          << "/" << ((i + 1) * width + j + 2) << "\n";
    }
  }
  
}

template <typename T>
void FlipY(const Array2D<T>& src, Array2D<T>& dst) {
  Vec2u size = src.GetSize();
  dst.Allocate(size[0], size[1]);
  for (unsigned row = 0; row < size[1]; row++) {
    const T * srcRow = src.DataPtr() + row * size[0];
    T* dstRow = dst.DataPtr() + (size[1] - row - 1) * size[0];
    for (unsigned col = 0; col < size[0]; col++) {
      dstRow[col] = srcRow[col];
    }
  }
}

void PrintToColor(const Array2D8u& print, Array2D8u& image) {
  uint8_t matColors[3][3] = {{204, 204, 204},{59, 59, 171}, {204, 77, 77}};
  Vec2u size = print.GetSize();
  Vec2u imageSize = image.GetSize();
  unsigned rowOff = imageSize[1] - size[1];
  for (unsigned row = 0; row < size[1]; row++) {
    const uint8_t* srcRow = print.DataPtr() + row * size[0];
    uint8_t* dstRow = image.DataPtr() + (row + rowOff) * imageSize[0];
    for (unsigned col = 0; col < size[0]; col++) {
      uint8_t mat = srcRow[col] / 50;
      if (mat > 0&&mat<=3) {
        mat--;
        dstRow[3 * col] = matColors[mat][0];
        dstRow[3 * col+1] = matColors[mat][1];
        dstRow[3 * col+2] = matColors[mat][2];
      }
    }
  }
}

//render heightmaps for wax
void MakeHeightMeshSeq() {
  std::string scanDir = "H:/nature/scan0.25mm/";
  std::string printDir = "H:/nature/print0.25mm/";
  std::string heightDir = "H:/nature/heightMesh";
  createDir(heightDir);
  unsigned startIdx = 118;
  unsigned endIdx = 3880;
  float voxRes = 0.255;
  float zRes = 0.025;
  Array2Df height;
  int meshCount = 0;
  int numMats = 3;
  float dx[3] = { 0.1f*voxRes, 0.1f*voxRes, 0.1f };
  float duv = 1.0f / 1600;
  //rbg
  Array2D8u texImage(4800, 1600);
  texImage.Fill(204);
  
  for (size_t i = startIdx; i < endIdx; i++) {
    std::string scanFile = scanDir + "/" + std::to_string(i) + ".png";
    std::string printFile = printDir + "/" + std::to_string(i) + ".png";
    Array2D8u scan;
    LoadPngGrey(scanFile, scan);
    if (scan.Empty()) {
      continue;
    }
    Array2D8u print;
    LoadPngGrey(printFile, print);
    Vec2u scanSize = scan.GetSize();
    if (height.Empty()) {
      height.Allocate(scanSize[0], scanSize[1]);
      height.Fill(0);
      SaveHeightWithUV(heightDir +"/height_uv.obj", scanSize[0], scanSize[1], dx, duv);
    }
    
    float z0 = i * zRes;
    UpdateDepth(height, scan, z0);
    
    std::ostringstream outMesh;
    outMesh << heightDir << "/" << std::setfill('0') << std::setw(4) << meshCount << ".obj";

    Array2Df outh;
    FlipY(height, outh);
    saveHeightObj(outMesh.str(), outh.GetData(), int(scanSize[0]), int(scanSize[1]), dx, false);
    
    texImage.Fill(204);
    PrintToColor(print, texImage);
    std::ostringstream imageFile;
    imageFile << heightDir << "/" << std::setfill('0') << std::setw(4) << meshCount << ".png";
    SavePngColor(imageFile.str(), texImage);
    meshCount++;
  }
}

void WriteMatIntoVol(Array3D8u &vol, const Array2Df& height, const Array2D8u & slice, 
  float voxRes) {
  Vec3u vsize = vol.GetSize();
  Vec2u sliceSize = slice.GetSize();
  for (unsigned row = 0; row < sliceSize[1]; row++) {
    if (row >= vsize[1]) {
      break;
    }

    const float* hrow = height.DataPtr() + row * sliceSize[0];
    const uint8_t* sliceRow = slice.DataPtr() + row * sliceSize[0];

    for (unsigned col = 0; col < sliceSize[0]; col++) {
      if (col >= vsize[0]) {
        break;
      }
      float h = hrow[col];
      int z = int(h / voxRes + 0.5f);
      if (z < 0) {
        continue;
      }
      if (z >= vsize[2]) {
        z = vsize[2] - 1;
      }
      uint8_t mat = sliceRow[col] / 50;
      if (mat > 1) {
        unsigned dstRow = vsize[1] - row - 1;
        vol(col, dstRow, z) = mat;
      }
    }
  }
}

void SubtractVol(Array3D8u& vol, const Array3D8u&b) {
  Vec3u size = vol.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (b(x, y, z) > 0) {
          vol(x, y, z) = 0;
        }
      }
    }
  }
}

void MakeVolMeshSeq() {
  std::string scanDir = "H:/nature/scan0.25mm/";
  std::string printDir = "H:/nature/print0.25mm/";
  std::string volDir = "H:/nature/volMesh";
  createDir(volDir);
  createDir(volDir + "/2");
  createDir(volDir + "/3");
  unsigned startIdx = 4;
  unsigned endIdx = 4792;
  float voxRes = 0.255;
  float zRes = 0.02;
  Array2Df height;
  int meshCount = 0;
  int numMats = 3;
  float dx[3] = { 0.1f * voxRes, 0.1f * voxRes, 0.1f };
  float duv = 1.0f / 1600;
  Array3D8u vol;
  Array3D8u oldvol;
  for (size_t i = startIdx; i < endIdx; i++) {
    std::string scanFile = scanDir + "/" + std::to_string(i) + ".png";
    std::string printFile = printDir + "/" + std::to_string(i) + ".png";
    Array2D8u scan;
    LoadPngGrey(scanFile, scan);
    if (scan.Empty()) {
      continue;
    }
    Array2D8u print;
    LoadPngGrey(printFile, print);
    Vec2u scanSize = scan.GetSize();
    if (height.Empty()) {
      height.Allocate(scanSize[0], scanSize[1]);
      height.Fill(0);

      vol.Allocate(scanSize[0], scanSize[1], endIdx * zRes / voxRes + 3);
    }
    float z0 = i * zRes;
    UpdateDepth(height, scan, z0);

    if (i>100) {
      if (meshCount == 489) {
        oldvol = vol;
      }
      WriteMatIntoVol(vol, height, print, voxRes);
      if (meshCount == 489) {
        SubtractVol(vol, oldvol);
      }
      if (meshCount > 488) {
        std::ostringstream mat2File;
        mat2File << volDir << "/2/" << std::setfill('0') << std::setw(4) << meshCount << ".obj";
        //output cm
        float res[3] = { 0.1 * voxRes,0.1 * voxRes,0.1 * voxRes };
        SaveVolAsObjMesh(mat2File.str(), vol, res, 2);

        std::ostringstream mat3File;
        mat3File << volDir << "/3/" << std::setfill('0') << std::setw(4) << meshCount << ".obj";
        SaveVolAsObjMesh(mat3File.str(), vol, res, 3);
      }
    }
    meshCount++;
  }
}

void SizeScans1080p() {
  std::string scanDir = "H:/nature/cleanScan/";
  std::string scanDirOut = "H:/nature/scan1080/";
  unsigned startIdx = 4;
  unsigned endIdx = 3880;
  unsigned scanCount = 0;
  unsigned dstWidth = 1920;
  unsigned dstHeight = 1080;

  cv::Mat meanScan;

  //temporal smoothing
  for (size_t i = startIdx; i <= endIdx; i++) {
    std::string inFile = scanDir + "/" + std::to_string(i) + ".png";
    cv::Mat in = cv::imread(inFile, cv::IMREAD_GRAYSCALE);
    if (in.empty()) {
      continue;
    }
    if (meanScan.empty()) {
      meanScan = in;
    }

    in = 0.5 * meanScan + 0.5 * in;
    meanScan = 0.5 * meanScan + 0.5 * in;

    cv::Mat out;
    unsigned w0 = unsigned( (in.cols * dstHeight) / in.rows );
     cv::Mat dstImage;
    if (w0<dstWidth) {
      cv::resize(in, out, cv::Size(w0, dstHeight), 0, 0, cv::INTER_LINEAR);
      unsigned borderCols = dstWidth - out.cols;
      cv::copyMakeBorder(out, dstImage, 0, 0, 0, borderCols, cv::BORDER_CONSTANT, 0);
    }
    else {
      unsigned h0 = unsigned(in.rows * dstWidth / in.cols);
      cv::resize(in, out, cv::Size(dstWidth, h0), 0, 0, cv::INTER_LINEAR);
      unsigned borderRows = dstHeight - h0;
      cv::copyMakeBorder(out, dstImage, 0, borderRows, 0, 0, cv::BORDER_CONSTANT, 0);
    }
    cv::flip(dstImage, dstImage, 0);
    std::ostringstream oss;
    oss << scanDirOut << "/" << std::setfill('0') << std::setw(4)<<std::to_string(scanCount)<<".png";
    cv::imwrite(oss.str(), dstImage);
    scanCount++;
  }
}

void DownsampleScan4x() {
  std::string scanDir = "H:/nature/cleanScan/";
  std::string scanDirOut = "H:/nature/scan0.25mm/";
  unsigned startIdx = 118;
  unsigned endIdx = 3880;
  for (size_t i = startIdx; i <= endIdx; i++) {
    std::string inFile = scanDir + "/" + std::to_string(i) + ".png";
    cv::Mat in = cv::imread(inFile, cv::IMREAD_GRAYSCALE);
    if (in.empty()) {
      continue;
    }
    cv::threshold(in, in, 120, 120, cv::THRESH_TRUNC);
    cv::Mat out;
    cv::resize(in, out, cv::Size(0,0), 0.25, 0.25, cv::INTER_LINEAR);
    std::ostringstream oss;
    cv::flip(out, out, 0);
    oss << scanDirOut << "/" << std::to_string(i) << ".png";
    cv::imwrite(oss.str(), out);
  }
}
void DownsamplePrint4x() {
  std::string scanDir = "H:/nature/print0.5x/";
  std::string scanDirOut = "H:/nature/print0.25mm/";
  unsigned startIdx = 3880;
  unsigned endIdx = 3881;
  for (size_t i = startIdx; i <= endIdx; i++) {
    std::string inFile = scanDir + "/" + std::to_string(i) + ".png";
    cv::Mat in = cv::imread(inFile, cv::IMREAD_GRAYSCALE);
    if (in.empty()) {
      continue;
    }
    cv::Mat out;
    cv::resize(in, out, cv::Size(0, 0), 0.25, 0.25, cv::INTER_NEAREST);
    std::ostringstream oss;
    cv::flip(out, out, 0);
    oss << scanDirOut << "/" << std::to_string(i) << ".png";
    cv::imwrite(oss.str(), out);
  }
}

cv::Mat calculate_cdf(cv::Mat histogram) {
  cv::Mat sum = cv::Mat::zeros(histogram.size(),CV_32FC1);
  sum.at<float>(0) = (histogram.at<float>(0));
  size_t len = histogram.total();
  for (int i = 1; i < histogram.total(); ++i) {
    sum.at<float>(i) = sum.at<float>(i - 1) + histogram.at<float>(i);
  }

  cv::Mat normalized_cdf = sum / float(sum.at<float>(len - 1));
  return normalized_cdf;
}

cv::Mat calculate_lookup(cv::Mat src_cdf, cv::Mat ref_cdf) {
  
  cv::Mat lookup_table = cv::Mat::zeros(1, 256, CV_8UC1);
  const float eps = 1e-6;
  for (unsigned srci = 0; srci < src_cdf.total(); srci++) {
    float f1 = src_cdf.at<float>(srci);
    for (unsigned refi = 0; refi < ref_cdf.total();refi++) {
      float f2 = ref_cdf.at<float>(refi);
      if (ref_cdf.at<float>(refi) >= src_cdf.at<float>(srci)) {
        lookup_table.at<uint8_t>(srci) = refi;
        break;
      }      
    }
  }
  return lookup_table;
}
void MatchHistoGray(const cv::Mat& ref, cv::Mat & dst) {
  cv::Mat refImage = ref;
  bool uniform = true; bool accumulate = false;
  int histSize = 256;
  // Calculate the histogram of the target image
  cv::Mat histDst;
  float range[] = { 0, 256 };
  const float* histRange = { range };
  cv::calcHist(&dst, 1, 0, cv::Mat(), histDst, 1, &histSize, &histRange, uniform, accumulate);

  // Calculate the cumulative distribution function (CDF) of the source image
  cv::Mat histRef;
  cv::calcHist(&refImage, 1, 0, cv::Mat(), histRef, 1, &histSize, &histRange, uniform, accumulate);

  cv::Mat refCDF = calculate_cdf(histRef);
  cv::Mat dstCDF = calculate_cdf(histDst);
  
  cv::Mat lut = calculate_lookup(dstCDF, refCDF);

  // Apply the matching LUT to the equalized source image
  cv::LUT(dst, lut, dst);
}

void MatchHistoRGB(const cv::Mat& src, cv::Mat dst) {
  std::vector<cv::Mat> bgr_src;
  cv::split(src, bgr_src);
  std::vector<cv::Mat> bgr_dst;
  cv::split(dst, bgr_dst);
  for (unsigned c = 0; c < 3; c++) {
    MatchHistoGray(bgr_src[c], bgr_dst[c]);
  }
  cv::merge(bgr_dst, dst);
}

void CopyTimelapseImages() {
  std::string inDir = "H:/nature/handTimeLapse1109/";
  std::string outDir = "H:/nature/timelapse1109/";
  int i0 = 1;
  int i1 = 3883;
  int outCount = 0;
  
  cv::Mat refImage;
  std::string refImageFile = "H:/nature/timeLapseRef.png";
  refImage = cv::imread(refImageFile);

  cv::Mat avgFrame;
  for (int i = i0; i <= i1; i++) {
    cv::Mat image;
    std::string imageName = inDir + std::to_string(i) + ".png";
    image =cv::imread(imageName);
    if (image.empty()) {
      continue;
    }
    float theta = 180.0f+2.47f; // Angle in degrees

    cv::Point2f center(image.cols / 2.0, image.rows / 2.0);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, theta, 1.0);
    cv::Rect roi(42, 44, 1850, 980);
    cv::Mat rotatedImage;
    cv::warpAffine(image, rotatedImage, rotationMatrix, image.size());
    cv::Mat cropped = rotatedImage(roi);
    
    MatchHistoRGB(refImage, cropped);      

    if (i == i0) {
      avgFrame = cropped;
    }
    float alpha = 0.8;
    addWeighted(avgFrame, alpha, cropped, 1.0f-alpha, 0.0, image);
    avgFrame = image;
    std::ostringstream outImageName;
    outImageName << outDir << std::setfill('0') << std::setw(4) << outCount << ".png";
    cv::imwrite(outImageName.str(), image);
    outCount++;
  }
}

int main(int argc, char * argv[])
{
  //DownsizeScansX();
  //DownsizePrintsX();
  //MaskScans();
  //SizeScans1080p();
  //DownsampleScan4x();
  //DownsamplePrint4x();
  MakeHeightMeshSeq();
  //MakeVolMeshSeq();
  //CopyTimelapseImages();
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
